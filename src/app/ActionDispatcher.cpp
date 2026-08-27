#include "app/ActionDispatcher.h"

#include "assist/MouseDwellMove.h"
#include "input/KeyboardInjector.h"
#include "input/MouseInjector.h"
#include "layout/PageDim.h"
#include "utils/Log.h"
#include "utils/ScreenGrab.h"

#include <QtGlobal>
#include <QTimer>

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

void armMagPick(GazerServices& svc, MouseDwellMove::ArmPurpose purpose, double zoom)
{
    svc.mouseDwellMove().setArmed(true, purpose, zoom);
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
    const QString edge = a.clickEdge.trimmed().toLower();
    if (edge == QLatin1String("down")) {
        return m_svc.mouseAssist().setHeld(btn, true, error);
    }
    if (edge == QLatin1String("up")) {
        return m_svc.mouseAssist().setHeld(btn, false, error);
    }
    const int n = qMax(1, a.clickCount);
    bool ok = true;
    for (int i = 0; i < n && ok; ++i) {
        ok = MouseInjector::click(btn, error);
    }
    if (ok) {
        m_svc.mouseAssist().markReleased(btn);
    }
    return ok;
}

bool ActionDispatcher::moveToGaze(QString* error)
{
    return m_svc.commands().run({QStringLiteral("mouseMoveToGaze"), {}}, error);
}

void ActionDispatcher::dispatchPage(const QVector<PageAction>& actions, const QString& sourcePageId,
                                    const QString& targetId)
{
    auto notify = [this](const QString& msg) { emit statusMessage(msg); };
    for (const PageAction& a : actions) {
        switch (a.type) {
        case PageActionType::Command: {
            QString err;
            if (!m_svc.commands().run({a.command, sourcePageId}, &err)) {
                notify(err.isEmpty() ? QStringLiteral("Command failed: %1").arg(a.command) : err);
            }
            break;
        }
        case PageActionType::Nav:
        case PageActionType::GoBack: {
            QString err;
            if (!m_svc.pages().applyNav(a, sourcePageId, targetId, &err)) {
                notify(err.isEmpty() ? QStringLiteral("Page action failed") : err);
            }
            break;
        }
        case PageActionType::Speak: {
            QString err;
            if (!m_svc.phrases().speak(a.speakText, &err) && !err.isEmpty()) {
                notify(err);
            }
            break;
        }
        case PageActionType::Send: {
            if (a.sendKey.isEmpty()) {
                break;
            }
            const QString edge = a.sendEdge.trimmed().toLower();
            QString err;
            if (edge == QLatin1String("down")) {
                if (!KeyboardInjector::keyDown(a.sendKey, &err)) {
                    notify(err.isEmpty() ? QStringLiteral("Send Down failed") : err);
                }
            } else if (edge == QLatin1String("up")) {
                if (!KeyboardInjector::keyUp(a.sendKey, &err)) {
                    notify(err.isEmpty() ? QStringLiteral("Send Up failed") : err);
                }
            } else if (a.sendDurationMs > 0) {
                if (!KeyboardInjector::keyDown(a.sendKey, &err)) {
                    notify(err.isEmpty() ? QStringLiteral("Send failed") : err);
                    break;
                }
                const QString key = a.sendKey;
                QTimer::singleShot(a.sendDurationMs, this, [key]() {
                    QString ignored;
                    (void)KeyboardInjector::keyUp(key, &ignored);
                });
            } else if (!KeyboardInjector::tapKey(a.sendKey, &err)) {
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
                if (a.zoomMode == PageZoomMode::Off) {
                    ok = moveToGaze(&err);
                } else {
                    const double zoom = a.zoomMode == PageZoomMode::Level
                                            ? double(a.zoomLevel)
                                            : m_svc.settings().pickZoom;
                    armMagPick(m_svc, MouseDwellMove::ArmPurpose::CursorMove, zoom);
                    break;
                }
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
            QString err;
            if (a.zoomMode == PageZoomMode::Off) {
                if (!moveToGaze(&err)) {
                    notify(err.isEmpty() ? QStringLiteral("MoveAndClick failed") : err);
                    break;
                }
                if (!dispatchClick(a, &err)) {
                    notify(err.isEmpty() ? QStringLiteral("MoveAndClick failed") : err);
                }
                break;
            }
            const MouseDwellMove::ArmPurpose purpose = clickPurpose(a.button);
            if (a.zoomMode == PageZoomMode::Settings && m_svc.mouseDwellMove().isArmed()
                && m_svc.mouseDwellMove().armPurpose() == purpose) {
                m_svc.mouseDwellMove().setArmed(false);
                break;
            }
            const double zoom =
                a.zoomMode == PageZoomMode::Level ? double(a.zoomLevel) : 0.0;
            armMagPick(m_svc, purpose, zoom);
            break;
        }
        case PageActionType::Ahk: {
            QString err;
            if (!m_svc.ahk().run(a.ahkSource, &err)) {
                notify(err.isEmpty() ? QStringLiteral("AHK failed") : err);
            }
            break;
        }
        case PageActionType::Unknown:
            GAZER_WARN << "Unknown Page action on" << targetId;
            break;
        }
    }
}

} // namespace gazer
