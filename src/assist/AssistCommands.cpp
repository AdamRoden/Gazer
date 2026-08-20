#include "assist/AssistCommands.h"

#include "app/AppSettings.h"
#include "app/CommandRegistry.h"
#include "assist/ActionLoopService.h"
#include "assist/AssistSession.h"
#include "assist/GazeMouseFollow.h"
#include "assist/GazeReticle.h"
#include "assist/LookToScroll.h"
#include "assist/MouseAssistState.h"
#include "assist/MouseDwellMove.h"
#include "layout/LayoutInstanceManager.h"
#include "ui/MagnifierOverlay.h"

#include <QPoint>

namespace gazer {

namespace {
const QString kGazeClickLoopKey = QStringLiteral("loop.gazeClick");
} // namespace

void registerAssistCommands(AssistCommandContext& ctx)
{
    using Mode = AssistSession::Mode;
    using ArmPurpose = MouseDwellMove::ArmPurpose;

    auto* session = ctx.session;
    auto* lts = ctx.lookToScroll;
    auto* mouseDwell = ctx.mouseDwellMove;
    auto* follow = ctx.gazeMouseFollow;
    auto* reticle = ctx.gazeReticle;
    auto* mag = ctx.magnifier;
    auto* instances = ctx.instances;
    auto* settings = ctx.settings;
    auto* actionLoops = ctx.actionLoops;
    auto* commands = ctx.commands;
    auto* mouseAssist = ctx.mouseAssist;
    auto applySettings = ctx.applySettings;
    auto refresh = ctx.refreshActiveIndicators;
    auto notify = ctx.notifyStatus;

    // When leaving a mode, tear down the tool that owns it — except soft handoffs
    // within the mouse-dwell family, and LTS kept alive under any mouse-dwell aim.
    QObject::connect(session, &AssistSession::leaving, session,
                     [lts, mouseDwell, follow](Mode left, Mode next) {
                         switch (left) {
                         case Mode::LookToScroll:
                             // Soft: Move-to / place-cursor / mag-pick while LTS stays on.
                             if (AssistSession::isMouseDwellFamily(next)) {
                                 break;
                             }
                             lts->setEnabled(false);
                             break;
                         case Mode::LookToScrollPlaceCursor:
                         case Mode::MouseDwell:
                         case Mode::MagPickPoint:
                             if (AssistSession::sameMouseDwellFamily(left, next)
                                 || next == Mode::LookToScroll) {
                                 break;
                             }
                             mouseDwell->setArmed(false);
                             break;
                         case Mode::GazeFollow:
                             follow->setEnabled(false);
                             break;
                         case Mode::None:
                             break;
                         }
                     });

    // Mouse dwell drives session mode from its arm purpose + phase.
    QObject::connect(mouseDwell, &MouseDwellMove::armedChanged, session,
                     [session, mouseDwell, lts](bool armed) {
                         if (!armed) {
                             const Mode m = session->mode();
                             if (AssistSession::isMouseDwellFamily(m)) {
                                 session->leave(m);
                             }
                             if (lts->isEnabled() && session->isNone()) {
                                 session->enter(Mode::LookToScroll);
                             }
                             return;
                         }
                         if (mouseDwell->isLookToScrollPlace()) {
                             session->enter(Mode::LookToScrollPlaceCursor);
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
                                                : Mode::MouseDwell);
                         }
                     });

    QObject::connect(lts, &LookToScroll::enabledChanged, session, [session, notify](bool on) {
        if (on) {
            if (session->mode() != Mode::LookToScrollPlaceCursor) {
                session->enter(Mode::LookToScroll);
            }
            return;
        }
        if (session->mode() == Mode::LookToScroll) {
            session->leave(Mode::LookToScroll);
        }
        notify(QStringLiteral("Look↕Scroll OFF"));
    });

    // Resume / re-place: arm direct Move-to for LTS (never mag-pick).
    QObject::connect(lts, &LookToScroll::placeScrollPointRequested, session,
                     [mouseDwell, notify]() {
                         mouseDwell->setArmed(true, ArmPurpose::LookToScrollPlace);
                         notify(QStringLiteral(
                             "Look↕Scroll: dwell to place scroll point, then look to scroll"));
                     });

    // After a successful place, enable or unsuspend LTS (purpose lives on the tool).
    QObject::connect(mouseDwell, &MouseDwellMove::movedTo, session,
                     [mouseDwell, lts, notify](QPoint) {
                         if (mouseDwell->armPurpose() != ArmPurpose::LookToScrollPlace) {
                             return;
                         }
                         if (!lts->isEnabled()) {
                             lts->setEnabled(true);
                             notify(QStringLiteral("Look↕Scroll ON (cursor placed)"));
                         } else {
                             lts->setScrollSuspended(false);
                             notify(QStringLiteral("Look↕Scroll resumed (cursor placed)"));
                         }
                     });

    QObject::connect(follow, &GazeMouseFollow::enabledChanged, session, [session](bool on) {
        if (on) {
            session->enter(Mode::GazeFollow);
        } else if (session->mode() == Mode::GazeFollow) {
            session->leave(Mode::GazeFollow);
        }
    });

    commands->registerBuiltin(QStringLiteral("toggleDwellSuspend"),
                              [instances, refresh, notify](QString*) {
                                  instances->toggleDwellSuspended();
                                  refresh();
                                  notify(instances->isDwellSuspended()
                                             ? QStringLiteral(
                                                   "Dwell SUSPENDED — only unlock cells work")
                                             : QStringLiteral("Dwell resumed"));
                                  return true;
                              });
    commands->registerBuiltin(QStringLiteral("suspendDwell"),
                              [instances, refresh, notify](QString*) {
                                  instances->setDwellSuspended(true);
                                  refresh();
                                  notify(QStringLiteral("Dwell SUSPENDED — only unlock cells work"));
                                  return true;
                              });
    commands->registerBuiltin(QStringLiteral("resumeDwell"), [instances, refresh, notify](QString*) {
        instances->setDwellSuspended(false);
        refresh();
        notify(QStringLiteral("Dwell resumed"));
        return true;
    });

    commands->registerBuiltin(
        QStringLiteral("toggleLookToScroll"),
        [lts, mouseDwell, settings, notify](QString*) {
            if (lts->isEnabled()) {
                lts->setEnabled(false);
                if (mouseDwell->isLookToScrollPlace()) {
                    mouseDwell->setArmed(false);
                }
                return true;
            }
            if (mouseDwell->isLookToScrollPlace()) {
                mouseDwell->setArmed(false);
                notify(QStringLiteral("Look↕Scroll cancelled"));
                return true;
            }
            if (settings->ltsPlaceCursorFirst) {
                mouseDwell->setArmed(true, ArmPurpose::LookToScrollPlace);
                notify(QStringLiteral(
                    "Look↕Scroll: dwell to place cursor, then look to scroll"));
            } else {
                lts->setEnabled(true);
                notify(QStringLiteral("Look↕Scroll ON"));
            }
            return true;
        });

    commands->registerBuiltin(QStringLiteral("toggleMagnifier"), [mag, reticle, refresh](QString*) {
        const bool turningOn = !mag->isEnabledLens();
        if (turningOn && reticle) {
            reticle->setEnabled(false); // exclusive with gaze indicator
        }
        mag->toggle();
        refresh();
        return true;
    });

    // Used by action series / loops: move OS cursor to last valid gaze sample.
    // (Registered fully in GazerServices with lastGaze access.)
    commands->registerBuiltin(QStringLiteral("mouseDwellMove"), [mouseDwell](QString*) {
        // Explicit Move-to: normal CursorMove purpose (mag-pick setting applies).
        // Soft handoff keeps LTS enabled if it was already on.
        mouseDwell->toggle();
        return true;
    });
    // Keep assist sticky registry in sync whenever click-loop arms/disarms
    // (toggle, session leave, stopAllActionLoops, etc.).
    if (actionLoops && mouseDwell) {
        QObject::connect(mouseDwell, &MouseDwellMove::armedChanged, mouseDwell,
                         [mouseDwell, actionLoops](bool) {
                             actionLoops->setAssistSticky(kGazeClickLoopKey,
                                                          mouseDwell->isClickLoop());
                         });
    }

    // Assist sticky: dwell move+click re-arm (not timed actionLoop series steps).
    commands->registerBuiltin(
        QStringLiteral("mouseDwellClickLoop"), [mouseDwell, refresh, notify](QString*) {
            using Purpose = MouseDwellMove::ArmPurpose;
            if (mouseDwell->isArmed()
                && mouseDwell->armPurpose() == Purpose::CursorMoveClickLoop) {
                mouseDwell->setArmed(false);
                notify(QStringLiteral("Gaze click loop OFF"));
            } else {
                mouseDwell->setArmed(true, Purpose::CursorMoveClickLoop);
                notify(QStringLiteral(
                    "Gaze click loop ON — dwell to move (mag-pick if enabled), then click"));
            }
            refresh();
            return true;
        });
    auto toggleMoveClick = [mouseDwell, refresh, notify](ArmPurpose purpose, const QString& onMsg,
                                                         const QString& offMsg) {
        return [mouseDwell, refresh, notify, purpose, onMsg, offMsg](QString*) {
            if (mouseDwell->isArmed() && mouseDwell->armPurpose() == purpose) {
                mouseDwell->setArmed(false);
                notify(offMsg);
            } else {
                mouseDwell->setArmed(true, purpose);
                notify(onMsg);
            }
            refresh();
            return true;
        };
    };
    commands->registerBuiltin(
        QStringLiteral("mouseMoveAndLeftClick"),
        toggleMoveClick(ArmPurpose::CursorMoveLeftClick,
                        QStringLiteral("Move + left click — dwell to place, then click"),
                        QStringLiteral("Move + left click OFF")));
    commands->registerBuiltin(
        QStringLiteral("mouseMoveAndRightClick"),
        toggleMoveClick(ArmPurpose::CursorMoveRightClick,
                        QStringLiteral("Move + right click — dwell to place, then click"),
                        QStringLiteral("Move + right click OFF")));
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
        QStringLiteral("toggleMouseMoveForesightDoubleZoom"),
        [settings, mouseDwell, applySettings, notify](QString*) {
            settings->mouseMoveForesightDoubleZoom = !settings->mouseMoveForesightDoubleZoom;
            mouseDwell->setForesightDoubleZoom(settings->mouseMoveForesightDoubleZoom);
            applySettings(true);
            notify(settings->mouseMoveForesightDoubleZoom
                       ? QStringLiteral("Foresight double-zoom ON")
                       : QStringLiteral("Foresight double-zoom OFF"));
            return true;
        });

    mouseAssist->registerCommands(*commands);
}

} // namespace gazer
