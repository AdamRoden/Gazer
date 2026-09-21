#include "assist/AssistCommands.h"

#include "app/AppSettings.h"
#include "app/CommandRegistry.h"
#include "assist/AssistSession.h"
#include "assist/GazeMouseFollow.h"
#include "assist/GazeReticle.h"
#include "assist/ComboMouse.h"
#include "assist/LookToMap.h"
#include "assist/LookToMaps.h"
#include "assist/LookToScroll.h"
#include "assist/LtsScrollMode.h"
#include "assist/MouseAssistState.h"
#include "assist/MouseDwellMove.h"
#include "layout/PageSession.h"
#include "ui/MagnifierOverlay.h"
#include "utils/Log.h"

#include <QPoint>
#include <QRect>
#include <utility>

namespace gazer {

PageDispatchFn wrapPageAimGate(PageSession* pages, MouseDwellMove* mouseDwell,
                               PageDispatchFn inner)
{
    return [pages, mouseDwell, inner = std::move(inner)](const QVector<PageAction>& actions,
                                                         const QString& pageId,
                                                         const QString& targetId) {
        const bool wasArmed = mouseDwell && mouseDwell->isArmed();
        const auto prevPurpose = mouseDwell ? mouseDwell->armPurpose()
                                            : MouseDwellMove::ArmPurpose::CursorMove;
        if (inner) {
            inner(actions, pageId, targetId);
        }
        if (!pages || !mouseDwell) {
            return;
        }
        if (mouseDwell->isArmed()
            && (!wasArmed || mouseDwell->armPurpose() != prevPurpose) && !targetId.isEmpty()) {
            const QRect gate = pages->targetScreenRect(pageId, targetId);
            if (!gate.isEmpty()) {
                mouseDwell->gateUntilGazeLeaves(gate);
                pages->setAimActivator(pageId, targetId);
            }
        } else if (!mouseDwell->isArmed()) {
            pages->clearAimActivator();
        }
    };
}

void registerAssistCommands(AssistCommandContext& ctx)
{
    using Mode = AssistSession::Mode;
    using ArmPurpose = MouseDwellMove::ArmPurpose;

    auto* session = ctx.session;
    auto* pages = ctx.pages;
    auto* lookTo = ctx.lookToMaps;
    auto* combo = ctx.comboMouse;
    auto* mouseDwell = ctx.mouseDwellMove;
    auto* follow = ctx.gazeMouseFollow;
    auto* reticle = ctx.gazeReticle;
    auto* mag = ctx.magnifier;
    auto* settings = ctx.settings;
    auto* commands = ctx.commands;
    auto* mouseAssist = ctx.mouseAssist;
    auto applySettings = ctx.applySettings;
    auto refresh = ctx.refreshActiveIndicators;
    auto notify = ctx.notifyStatus;
    auto setDwellSuspended = ctx.setDwellSuspended;
    auto isDwellSuspended = ctx.isDwellSuspended;
    auto applyDwellSuspend = [pages, setDwellSuspended](bool on) {
        if (pages) {
            pages->armDwellStartHold();
        }
        if (setDwellSuspended) {
            setDwellSuspended(on);
        }
    };

    // When leaving a mode, tear down the tool that owns it — except soft handoffs
    // within the mouse-dwell family.
    QObject::connect(session, &AssistSession::leaving, session,
                     [combo, mouseDwell, follow](Mode left, Mode next) {
                         switch (left) {
                         case Mode::LookToScrollPlaceCursor:
                         case Mode::MouseDwell:
                         case Mode::MagPickPoint:
                             if (AssistSession::sameMouseDwellFamily(left, next)
                                 || next == Mode::ComboMouse) {
                                 break;
                             }
                             mouseDwell->setArmed(false);
                             break;
                         case Mode::ComboMousePlaceCursor:
                             if (next == Mode::ComboMouse) {
                                 break;
                             }
                             if (!AssistSession::sameMouseDwellFamily(left, next)) {
                                 mouseDwell->setArmed(false);
                             }
                             if (combo) {
                                 combo->setEnabled(false);
                             }
                             break;
                         case Mode::GazeFollow:
                             follow->setEnabled(false);
                             break;
                         case Mode::ComboMouse:
                             if (next == Mode::ComboMousePlaceCursor) {
                                 break;
                             }
                             if (combo) {
                                 combo->setEnabled(false);
                             }
                             break;
                         case Mode::None:
                             break;
                         }
                     });

    // Mouse dwell drives session mode from its arm purpose + phase.
    QObject::connect(mouseDwell, &MouseDwellMove::armedChanged, session,
                     [session, mouseDwell, lookTo, pages](bool armed) {
                         if (!armed) {
                             if (pages) {
                                 pages->clearAimActivator();
                             }
                             const Mode m = session->mode();
                             if (AssistSession::isMouseDwellFamily(m)) {
                                 session->leave(m);
                             }
                             if (lookTo) {
                                 lookTo->cancelPlace();
                             }
                             return;
                         }
                         if (mouseDwell->isLookToScrollPlace()) {
                             session->enter(Mode::LookToScrollPlaceCursor);
                         } else if (mouseDwell->isComboMousePlace()) {
                             session->enter(Mode::ComboMousePlaceCursor);
                         } else if (mouseDwell->isMagPointPhase()) {
                             session->enter(Mode::MagPickPoint);
                         } else {
                             session->enter(Mode::MouseDwell);
                         }
                     });

    QObject::connect(mouseDwell, &MouseDwellMove::magPointPhaseChanged, session,
                     [session, mouseDwell](bool active) {
                         if (active) {
                             session->enter(Mode::MagPickPoint);
                         } else if (session->mode() == Mode::MagPickPoint
                                    && mouseDwell->isArmed()) {
                             session->enter(mouseDwell->isLookToScrollPlace()
                                                ? Mode::LookToScrollPlaceCursor
                                                : mouseDwell->isComboMousePlace()
                                                      ? Mode::ComboMousePlaceCursor
                                                      : Mode::MouseDwell);
                         }
                     });

    if (lookTo) {
        QObject::connect(lookTo, &LookToMaps::enabledChanged, session,
                         [notify](LookToDest dest, bool on) {
                             notify(QStringLiteral("%1 %2")
                                        .arg(QLatin1String(lookToDestLabel(dest)),
                                             on ? QStringLiteral("ON") : QStringLiteral("OFF")));
                         });
        QObject::connect(lookTo, &LookToMaps::placeOriginRequested, session,
                         [lookTo, mouseDwell, notify](LookToDest dest) {
                             LookToScroll& m = lookTo->map(dest);
                             mouseDwell->setArmed(true, ArmPurpose::LookToScrollPlace);
                             if (m.hasScrollOrigin()) {
                                 const int r = qMax(40, m.deadzonePx());
                                 const QPoint c = m.scrollOrigin();
                                 mouseDwell->gateUntilGazeLeaves(
                                     QRect(c.x() - r, c.y() - r, r * 2, r * 2));
                             }
                             notify(QStringLiteral("%1: dwell to place origin")
                                        .arg(QLatin1String(lookToDestLabel(dest))));
                         });
        QObject::connect(lookTo, &LookToMaps::axisModeChanged, session,
                         [notify](LookToDest dest, LtsScrollMode mode) {
                             notify(QStringLiteral("%1: %2")
                                        .arg(QLatin1String(lookToDestLabel(dest)),
                                             QLatin1String(ltsScrollModeName(mode))));
                         });
    }

    QObject::connect(mouseDwell, &MouseDwellMove::movedTo, session,
                     [mouseDwell, lookTo, combo, session, notify](QPoint pos) {
                         if (mouseDwell->armPurpose() == ArmPurpose::ComboMousePlace) {
                             if (combo) {
                                 combo->setEnabled(true);
                                 combo->showAt(pos);
                                 session->enter(Mode::ComboMouse);
                                 notify(QStringLiteral("ComboMouse ready"));
                             }
                             return;
                         }
                         if (mouseDwell->armPurpose() != ArmPurpose::LookToScrollPlace) {
                             return;
                         }
                         if (!lookTo) {
                             return;
                         }
                         const LookToDest dest = lookTo->placingDest();
                         lookTo->onPlaced(pos);
                         notify(QStringLiteral("%1 ON (origin placed)")
                                    .arg(QLatin1String(lookToDestLabel(dest))));
                     });

    if (combo) {
        combo->setHoldFn([mouseAssist](bool down) {
            if (!mouseAssist) {
                return false;
            }
            return mouseAssist->setHeld(QStringLiteral("left"), down);
        });
        combo->setHeldQuery([mouseAssist]() {
            return mouseAssist && mouseAssist->isLeftHeld();
        });
        QObject::connect(combo, &ComboMouse::placeRequested, session,
                         [mouseDwell, combo, lookTo, notify]() {
                             if (!combo || !combo->isEnabled() || !mouseDwell) {
                                 return;
                             }
                             if (lookTo) {
                                 lookTo->disableAll();
                             }
                             mouseDwell->setArmed(true, ArmPurpose::ComboMousePlace);
                             mouseDwell->gateUntilGazeLeaves(combo->originGateRect());
                             notify(QStringLiteral("ComboMouse: dwell to place"));
                         });
        QObject::connect(combo, &ComboMouse::enabledChanged, session,
                         [session, mouseDwell, combo, notify](bool on) {
                             if (on) {
                                 return;
                             }
                             if (mouseDwell && mouseDwell->isComboMousePlace()) {
                                 mouseDwell->setArmed(false);
                             }
                             if (session->mode() == Mode::ComboMouse) {
                                 session->leave(Mode::ComboMouse);
                             }
                             notify(QStringLiteral("ComboMouse OFF"));
                         });
    }

    QObject::connect(follow, &GazeMouseFollow::enabledChanged, session, [session](bool on) {
        if (on) {
            session->enter(Mode::GazeFollow);
        } else if (session->mode() == Mode::GazeFollow) {
            session->leave(Mode::GazeFollow);
        }
    });

    commands->registerBuiltin(QStringLiteral("toggleDwellSuspend"),
                              [applyDwellSuspend, isDwellSuspended, refresh, notify](QString*) {
                                  const bool on = !(isDwellSuspended && isDwellSuspended());
                                  applyDwellSuspend(on);
                                  refresh();
                                  notify(on ? QStringLiteral(
                                                  "Dwell SUSPENDED — only unlock cells work")
                                            : QStringLiteral("Dwell resumed"));
                                  return true;
                              });
    commands->registerBuiltin(QStringLiteral("suspendDwell"),
                              [applyDwellSuspend, refresh, notify](QString*) {
                                  applyDwellSuspend(true);
                                  refresh();
                                  notify(QStringLiteral("Dwell SUSPENDED — only unlock cells work"));
                                  return true;
                              });
    commands->registerBuiltin(QStringLiteral("resumeDwell"),
                              [applyDwellSuspend, refresh, notify](QString*) {
                                  applyDwellSuspend(false);
                                  refresh();
                                  notify(QStringLiteral("Dwell resumed"));
                                  return true;
                              });

    auto mapAction = [lookTo](LookToDest dest, auto fn) {
        return [lookTo, dest, fn](QString*) {
            if (!lookTo) {
                return false;
            }
            fn(lookTo->map(dest));
            return true;
        };
    };
    auto registerMapActions = [&](LookToDest dest, const QString& prefix) {
        commands->registerBuiltin(prefix + QStringLiteral("resume"),
                                  mapAction(dest, [](LookToScroll& m) { m.resumeScroll(); }));
        commands->registerBuiltin(prefix + QStringLiteral("speed.slower"),
                                  mapAction(dest, [](LookToScroll& m) { m.nudgeMaxSpeed(-1); }));
        commands->registerBuiltin(prefix + QStringLiteral("speed.faster"),
                                  mapAction(dest, [](LookToScroll& m) { m.nudgeMaxSpeed(+1); }));
        commands->registerBuiltin(prefix + QStringLiteral("quit"),
                                  mapAction(dest, [](LookToScroll& m) { m.setEnabled(false); }));
        commands->registerBuiltin(prefix + QStringLiteral("reset"),
                                  mapAction(dest, [](LookToScroll& m) { m.requestReset(); }));
        commands->registerBuiltin(prefix + QStringLiteral("cycleMode"),
                                  mapAction(dest, [](LookToScroll& m) { m.cycleScrollMode(); }));
    };
    registerMapActions(LookToDest::Scroll, QStringLiteral("lts."));
    registerMapActions(LookToDest::Scroll, QStringLiteral("lookTo.scroll."));
    registerMapActions(LookToDest::Mouse, QStringLiteral("lookTo.mouse."));
    registerMapActions(LookToDest::LeftStick, QStringLiteral("lookTo.leftStick."));
    registerMapActions(LookToDest::RightStick, QStringLiteral("lookTo.rightStick."));

    auto toggleLookTo = [lookTo, mouseDwell, combo, notify](LookToDest dest) {
        return [lookTo, mouseDwell, combo, notify, dest](QString*) {
            if (!lookTo || !mouseDwell) {
                return false;
            }
            LookToScroll& m = lookTo->map(dest);
            if (m.isEnabled()) {
                m.setEnabled(false);
                if (mouseDwell->isLookToScrollPlace() && lookTo->placingDest() == dest) {
                    mouseDwell->setArmed(false);
                }
                return true;
            }
            if (mouseDwell->isLookToScrollPlace() && lookTo->isPlacing()
                && lookTo->placingDest() == dest) {
                mouseDwell->setArmed(false);
                notify(QStringLiteral("%1 cancelled").arg(QLatin1String(lookToDestLabel(dest))));
                return true;
            }
            if (combo && combo->isEnabled()) {
                combo->setEnabled(false);
            }
            if (mouseDwell->isComboMousePlace()) {
                mouseDwell->setArmed(false);
            }
            if (mouseDwell->isLookToScrollPlace()) {
                mouseDwell->setArmed(false);
            }
            lookTo->beginPlace(dest);
            mouseDwell->setArmed(true, ArmPurpose::LookToScrollPlace);
            notify(QStringLiteral("%1: dwell to place origin")
                       .arg(QLatin1String(lookToDestLabel(dest))));
            return true;
        };
    };
    commands->registerBuiltin(QStringLiteral("lookToScroll"), toggleLookTo(LookToDest::Scroll));
    commands->registerBuiltin(QStringLiteral("toggleLookToScroll"), toggleLookTo(LookToDest::Scroll));
    commands->registerBuiltin(QStringLiteral("lookToMouse"), toggleLookTo(LookToDest::Mouse));
    commands->registerBuiltin(QStringLiteral("lookToLeftStick"), toggleLookTo(LookToDest::LeftStick));
    commands->registerBuiltin(QStringLiteral("lookToRightStick"),
                              toggleLookTo(LookToDest::RightStick));

    commands->registerBuiltin(QStringLiteral("toggleMagnifier"), [mag, reticle, refresh](QString*) {
        const bool turningOn = !mag->isEnabledLens();
        if (turningOn && reticle) {
            reticle->setEnabled(false); // exclusive with gaze indicator
        }
        mag->toggle();
        refresh();
        return true;
    });

    commands->registerBuiltin(QStringLiteral("toggleComboMouse"),
                              [combo, mouseDwell, lookTo, notify](QString*) {
                                  if (!combo || !mouseDwell) {
                                      return false;
                                  }
                                  if (combo->isEnabled()) {
                                      combo->setEnabled(false);
                                      return true;
                                  }
                                  if (mouseDwell->isComboMousePlace()) {
                                      mouseDwell->setArmed(false);
                                      notify(QStringLiteral("ComboMouse cancelled"));
                                      return true;
                                  }
                                  if (lookTo) {
                                      lookTo->disableAll();
                                  }
                                  mouseDwell->setArmed(true, ArmPurpose::ComboMousePlace);
                                  notify(QStringLiteral("ComboMouse: dwell to place"));
                                  return true;
                              });

    commands->registerBuiltin(QStringLiteral("mouseMoveToGaze"), [mouseDwell](QString*) {
        mouseDwell->toggleArmed(ArmPurpose::CursorMove);
        return true;
    });
    commands->registerBuiltin(
        QStringLiteral("mouseMoveToGazeClickLoop"),
        [mouseDwell, refresh, notify](QString*) {
            mouseDwell->toggleArmed(ArmPurpose::CursorMoveClickLoop);
            notify(mouseDwell->isArmed()
                       ? QStringLiteral(
                             "Gaze click loop ON — dwell to move (mag-pick if enabled), then click")
                       : QStringLiteral("Gaze click loop OFF"));
            refresh();
            return true;
        });
    auto toggleMoveClick = [mouseDwell, refresh, notify](ArmPurpose purpose, const QString& onMsg,
                                                         const QString& offMsg) {
        return [mouseDwell, refresh, notify, purpose, onMsg, offMsg](QString*) {
            mouseDwell->toggleArmed(purpose);
            notify(mouseDwell->isArmed() ? onMsg : offMsg);
            refresh();
            return true;
        };
    };
    const auto leftAtGaze = toggleMoveClick(
        ArmPurpose::CursorMoveLeftClick,
        QStringLiteral("Left click at gaze — dwell to place, then click"),
        QStringLiteral("Left click at gaze OFF"));
    const auto rightAtGaze = toggleMoveClick(
        ArmPurpose::CursorMoveRightClick,
        QStringLiteral("Right click at gaze — dwell to place, then click"),
        QStringLiteral("Right click at gaze OFF"));
    const auto middleAtGaze = toggleMoveClick(
        ArmPurpose::CursorMoveMiddleClick,
        QStringLiteral("Middle click at gaze — dwell to place, then click"),
        QStringLiteral("Middle click at gaze OFF"));
    commands->registerBuiltin(QStringLiteral("mouseLeftClickAtGaze"), leftAtGaze);
    commands->registerBuiltin(QStringLiteral("mouseRightClickAtGaze"), rightAtGaze);
    commands->registerBuiltin(QStringLiteral("mouseMiddleClickAtGaze"), middleAtGaze);
    commands->registerBuiltin(QStringLiteral("toggleGazeReticle"),
                              [reticle, mag, refresh, notify](QString*) {
                                  const bool turningOn = !reticle->isEnabled();
                                  if (turningOn && mag) {
                                      mag->setEnabledLens(false); // exclusive with magnifier
                                  }
                                  reticle->toggle();
                                  refresh();
                                  notify(reticle->isEnabled() ? QStringLiteral("Gaze reticle ON")
                                                              : QStringLiteral("Gaze reticle OFF"));
                                  return true;
                              });
    commands->registerBuiltin(QStringLiteral("toggleGazeMouseFollow"), [follow, notify](QString*) {
        follow->toggle();
        notify(follow->isEnabled() ? QStringLiteral("Gaze→mouse ON")
                                   : QStringLiteral("Gaze→mouse OFF"));
        return true;
    });
    commands->registerBuiltin(
        QStringLiteral("toggleMouseMoveMagPick"),
        [settings, mouseDwell, applySettings, notify](QString*) {
            settings->mouseMoveMagPick = !settings->mouseMoveMagPick;
            mouseDwell->setMagPickEnabled(settings->mouseMoveMagPick);
            applySettings(true);
            notify(settings->mouseMoveMagPick ? QStringLiteral("Move-to: magnify pick ON")
                                              : QStringLiteral("Move-to: direct ON"));
            return true;
        });
    commands->registerBuiltin(
        QStringLiteral("toggleMouseMoveMagPickCenter"),
        [settings, mouseDwell, applySettings, notify](QString*) {
            settings->mouseMoveMagPickCenterOnDwell = !settings->mouseMoveMagPickCenterOnDwell;
            mouseDwell->setMagPickCenterOnDwell(settings->mouseMoveMagPickCenterOnDwell);
            applySettings(true);
            notify(settings->mouseMoveMagPickCenterOnDwell
                       ? QStringLiteral("Mag-pick: center on dwell point")
                       : QStringLiteral("Mag-pick: center on screen"));
            return true;
        });
    commands->registerBuiltin(
        QStringLiteral("toggleMouseMoveMagPickFullScreen"),
        [settings, mouseDwell, applySettings, notify](QString*) {
            settings->mouseMoveMagPickFullScreen = !settings->mouseMoveMagPickFullScreen;
            mouseDwell->setMagPickFullScreen(settings->mouseMoveMagPickFullScreen);
            applySettings(true);
            notify(settings->mouseMoveMagPickFullScreen
                       ? QStringLiteral("Mag-pick: full-screen zoom ON")
                       : QStringLiteral("Mag-pick: full-screen zoom OFF"));
            return true;
        });
    commands->registerBuiltin(
        QStringLiteral("toggleMouseMoveForesight"),
        [settings, mouseDwell, applySettings, notify](QString*) {
            settings->mouseMoveForesight = !settings->mouseMoveForesight;
            mouseDwell->setForesightEnabled(settings->mouseMoveForesight);
            applySettings(true);
            notify(settings->mouseMoveForesight ? QStringLiteral("Foresight ON")
                                                : QStringLiteral("Foresight OFF"));
            return true;
        });
    commands->registerBuiltin(
        QStringLiteral("toggleMouseMoveForesightSecondZoom"),
        [settings, mouseDwell, applySettings, notify](QString*) {
            settings->mouseMoveForesightSecondZoom = !settings->mouseMoveForesightSecondZoom;
            mouseDwell->setForesightSecondZoom(settings->mouseMoveForesightSecondZoom);
            applySettings(true);
            notify(settings->mouseMoveForesightSecondZoom
                       ? QStringLiteral("Foresight second ON")
                       : QStringLiteral("Foresight second OFF"));
            return true;
        });

    mouseAssist->registerCommands(*commands);
}

} // namespace gazer
