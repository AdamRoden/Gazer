#include "app/ActionDispatcher.h"

#include "app/CommandRegistry.h"
#include "app/ComposeUi.h"
#include "app/GazerServices.h"
#include "assist/AhkLauncher.h"
#include "assist/ComboMouse.h"
#include "assist/MouseAssistState.h"
#include "assist/MouseDwellMove.h"
#include "assist/SpeechEngine.h"
#include "input/KeyStateManager.h"
#include "input/MouseInjector.h"
#include "layout/PageDim.h"
#include "layout/PageSession.h"
#include "utils/Log.h"
#include "utils/ScreenGrab.h"

#include <QtGlobal>

namespace gazer {

namespace {

MouseDwellMove::ArmPurpose clickPurpose(const QString& button)
{
    const QString b = button.trimmed().toLower();
    if (b == QLatin1String("right")) {
        return MouseDwellMove::ArmPurpose::CursorMoveRightClick;
    }
    if (b == QLatin1String("middle")) {
        return MouseDwellMove::ArmPurpose::CursorMoveMiddleClick;
    }
    return MouseDwellMove::ArmPurpose::CursorMoveLeftClick;
}

MouseDwellMove::ArmZoom armZoomFrom(const PageAction& a)
{
    switch (a.zoomMode) {
    case PageZoomMode::Off:
        return MouseDwellMove::ArmZoom::direct();
    case PageZoomMode::Level:
        return MouseDwellMove::ArmZoom::at(double(a.zoomLevel));
    case PageZoomMode::Foresight:
        return MouseDwellMove::ArmZoom::foresight();
    case PageZoomMode::ForesightBonus:
        return MouseDwellMove::ArmZoom::foresightBonus();
    case PageZoomMode::Settings:
        break;
    }
    return MouseDwellMove::ArmZoom::settings();
}

} // namespace

ActionDispatcher::ActionDispatcher(GazerServices& services, QObject* parent)
    : QObject(parent)
    , m_svc(services)
{
}

bool ActionDispatcher::dispatchClick(const PageAction& a, QString* error)
{
    const QString btn = a.button.isEmpty() ? QStringLiteral("left") : a.button;
    switch (a.clickKind) {
    case PageClickKind::Toggle: {
        const bool down = (btn == QLatin1String("right"))   ? !m_svc.mouseAssist().isRightHeld()
                          : (btn == QLatin1String("middle")) ? !m_svc.mouseAssist().isMiddleHeld()
                                                            : !m_svc.mouseAssist().isLeftHeld();
        return m_svc.mouseAssist().setHeld(btn, down, error);
    }
    case PageClickKind::Down:
        return m_svc.mouseAssist().setHeld(btn, true, error);
    case PageClickKind::Up:
        return m_svc.mouseAssist().setHeld(btn, false, error);
    case PageClickKind::Double: {
        const bool ok = MouseInjector::doubleClick(btn, error);
        if (ok) {
            m_svc.mouseAssist().markReleased(btn);
        }
        return ok;
    }
    case PageClickKind::Default:
        break;
    }
    const bool ok = MouseInjector::click(btn, error);
    if (ok) {
        m_svc.mouseAssist().markReleased(btn);
    }
    return ok;
}

void ActionDispatcher::dispatchPage(const QVector<PageAction>& actions, const QString& sourcePageId,
                                    const QString& targetId)
{
    auto notify = [this](const QString& msg) { emit statusMessage(msg); };
    QVector<PageAction> showNav;
    auto flushShowNav = [&]() {
        if (showNav.isEmpty()) {
            return;
        }
        QString err;
        if (!m_svc.pages().showLayers(showNav, sourcePageId, targetId, &err)) {
            notify(err.isEmpty() ? QStringLiteral("Page action failed") : err);
        }
        showNav.clear();
    };
    for (const PageAction& a : actions) {
        if (a.type == PageActionType::ShowLayers) {
            showNav.push_back(a);
            continue;
        }
        flushShowNav();
        if (m_svc.composeUi().tryHandle(a, sourcePageId)) {
            continue;
        }
        switch (a.type) {
        case PageActionType::Command: {
            QString err;
            if (!m_svc.commands().run({a.command, sourcePageId}, &err)) {
                notify(err.isEmpty() ? QStringLiteral("Command failed: %1").arg(a.command) : err);
            }
            break;
        }
        case PageActionType::Nav:
        case PageActionType::HostPage:
        case PageActionType::GoBack: {
            if (a.type == PageActionType::Nav && a.verb == PageVerb::Close
                && (a.targetScope == PageNavScope::All
                    || a.targetScope == PageNavScope::Others)) {
                m_svc.comboMouse().setEnabled(false);
            }
            QString err;
            if (!m_svc.pages().applyNav(a, sourcePageId, targetId, &err)) {
                notify(err.isEmpty() ? QStringLiteral("Page action failed") : err);
            }
            break;
        }
        case PageActionType::Speak:
            m_svc.speechEngine().speak(a.speakText, SpeakKind::Canned);
            if (!a.speakText.isEmpty()) {
                notify(QStringLiteral("Said: %1").arg(a.speakText));
            }
            break;
        case PageActionType::Send: {
            if (a.sendKey.isEmpty()) {
                break;
            }
            const QString edge = a.sendEdge.trimmed().toLower();
            QString err;
            bool ok = true;
            KeyStateManager& keys = m_svc.keyState();
            if (edge == QLatin1String("down")) {
                ok = keys.down(a.sendKey, &err);
            } else if (edge == QLatin1String("up")) {
                ok = keys.up(a.sendKey, &err);
            } else if (a.sendDurationMs > 0) {
                ok = keys.hold(a.sendKey, a.sendDurationMs, &err);
            } else {
                ok = keys.activate(a.sendKey, &err);
            }
            if (!ok) {
                notify(err.isEmpty() ? QStringLiteral("Send failed") : err);
            }
            break;
        }
        case PageActionType::Click: {
            QString err;
            if (!dispatchClick(a, &err)) {
                notify(err.isEmpty() ? QStringLiteral("Click failed") : err);
            }
            break;
        }
        case PageActionType::Move: {
            QString err;
            bool ok = true;
            if (a.moveMode == PageMoveMode::Gaze) {
                m_svc.mouseDwellMove().toggleArmed(MouseDwellMove::ArmPurpose::CursorMove,
                                                   armZoomFrom(a));
                break;
            } else if (a.moveMode == PageMoveMode::Direction) {
                const int amount =
                    a.moveAmount >= 0 ? a.moveAmount : m_svc.mouseAssist().moveAmountPx();
                const QPoint d = PageDimParse::anchorDelta(a.moveDirection, amount);
                ok = MouseInjector::moveBy(d.x(), d.y(), &err);
            } else {
                const QRect desk = virtualDesktop();
                const QRect screen = overlayScreenGeometry();
                const int x = qRound(a.moveX.resolve(desk.width(), desk.height(), screen.width(),
                                                     screen.height()));
                const int y = qRound(a.moveY.resolve(desk.height(), desk.height(), screen.width(),
                                                     screen.height()));
                if (a.moveMode == PageMoveMode::Relative) {
                    ok = MouseInjector::moveBy(x, y, &err);
                } else {
                    ok = MouseInjector::moveTo(x, y, &err);
                }
            }
            if (!ok) {
                notify(err.isEmpty() ? QStringLiteral("Move failed") : err);
            }
            break;
        }
        case PageActionType::MoveAndClick: {
            m_svc.mouseDwellMove().toggleArmed(clickPurpose(a.button), armZoomFrom(a));
            break;
        }
        case PageActionType::Ahk: {
            QString err;
            if (!m_svc.ahk().run(a.ahkSource, &err)) {
                notify(err.isEmpty() ? QStringLiteral("AHK failed") : err);
            }
            break;
        }
        case PageActionType::ShowLayers:
            break;
        case PageActionType::Unknown:
            GAZER_WARN << "Unknown Page action on" << targetId;
            break;
        }
    }
    flushShowNav();
}

} // namespace gazer
