#include "app/ActionDispatcher.h"

#include "input/KeyboardInjector.h"
#include "input/MouseInjector.h"
#include "utils/Log.h"
#include "utils/ScreenGrab.h"

#include <QtGlobal>
#include <QTimer>

namespace gazer {

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

void ActionDispatcher::dispatchPage(const QVector<PageAction>& actions, const QString& sourcePageId,
                                    const QString& targetId)
{
    Q_UNUSED(targetId);
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
        case PageActionType::Page: {
            QString err;
            QString tid = a.targetId;
            if (tid.compare(QLatin1String("self"), Qt::CaseInsensitive) == 0) {
                tid = sourcePageId;
            }
            if (!m_svc.pages().applyPageAction(a.verb, a.targetKind, tid, &err)) {
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
                ok = m_svc.commands().run({QStringLiteral("mouseMoveToGaze"), sourcePageId}, &err);
            } else {
                const QRect desk = virtualDesktop();
                const int x = qRound(a.moveX.resolve(desk.width(), desk.height()));
                const int y = qRound(a.moveY.resolve(desk.height(), desk.height()));
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
            if (!m_svc.commands().run({QStringLiteral("mouseMoveToGaze"), sourcePageId}, &err)) {
                notify(err.isEmpty() ? QStringLiteral("MoveAndClick failed") : err);
                break;
            }
            if (!dispatchClick(a, &err)) {
                notify(err.isEmpty() ? QStringLiteral("MoveAndClick failed") : err);
            }
            break;
        }
        case PageActionType::Ahk:
            notify(QStringLiteral("AHK actions are not supported"));
            break;
        case PageActionType::Unknown:
            GAZER_WARN << "Unknown Page action on" << targetId;
            break;
        }
    }
}

} // namespace gazer
