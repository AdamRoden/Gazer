#include "app/ActiveStateResolver.h"

#include "app/AppSettings.h"
#include "assist/ActionLoopService.h"
#include "assist/GazeMouseFollow.h"
#include "assist/GazeReticle.h"
#include "assist/LookToScroll.h"
#include "assist/MouseAssistState.h"
#include "assist/MouseDwellMove.h"
#include "layout/LayoutInstanceManager.h"
#include "ui/MagnifierOverlay.h"
#include "ui/Theme.h"

namespace gazer {

bool resolveActiveState(const ActiveStateContext& ctx, const QString& key)
{
    if (key == QLatin1String("dwellSuspend") || key == QLatin1String("dwell.suspended")) {
        return ctx.instances && ctx.instances->isDwellSuspended();
    }
    if (key == QLatin1String("lookToScroll")) {
        return ctx.lookToScroll && ctx.lookToScroll->isEnabled();
    }
    if (key == QLatin1String("lookToScroll.suspended")) {
        return ctx.lookToScroll && ctx.lookToScroll->isScrollSuspended();
    }
    if (key == QLatin1String("mouseDwellMove")) {
        return ctx.mouseDwellMove && ctx.mouseDwellMove->isArmed()
               && !ctx.mouseDwellMove->isClickLoop();
    }
    // Gaze click loop: assist sticky registered on ActionLoopService + live arm state.
    if (key == QLatin1String("loop.gazeClick") || key == QLatin1String("mouseDwellClickLoop")) {
        if (ctx.actionLoops && ctx.actionLoops->isActiveState(QStringLiteral("loop.gazeClick"))) {
            return true;
        }
        return ctx.mouseDwellMove && ctx.mouseDwellMove->isClickLoop();
    }
    if (key == QLatin1String("mouseMoveMagPick")) {
        return ctx.settings && ctx.settings->mouseMoveMagPick;
    }
    if (key == QLatin1String("mouseMoveMagPickCenter")) {
        return ctx.settings && ctx.settings->mouseMoveMagPickCenterOnDwell;
    }
    if (key == QLatin1String("magnifier")) {
        return ctx.magnifier && ctx.magnifier->isEnabledLens();
    }
    if (key == QLatin1String("gazeReticle")) {
        return ctx.gazeReticle && ctx.gazeReticle->isEnabled();
    }
    if (key == QLatin1String("gazeMouseFollow")) {
        return ctx.gazeMouseFollow && ctx.gazeMouseFollow->isEnabled();
    }
    if (ctx.actionLoops && ctx.actionLoops->isActiveState(key)) {
        return true;
    }
    if (key == QLatin1String("mouse.leftHold")) {
        return ctx.mouseAssist && ctx.mouseAssist->isLeftHeld();
    }
    if (key == QLatin1String("mouse.rightHold")) {
        return ctx.mouseAssist && ctx.mouseAssist->isRightHeld();
    }
    if (key == QLatin1String("mouse.middleHold")) {
        return ctx.mouseAssist && ctx.mouseAssist->isMiddleHeld();
    }
    if (key == QLatin1String("mouse.anyHold")) {
        return ctx.mouseAssist && ctx.mouseAssist->anyButtonHeld();
    }
    if (!ctx.settings) {
        return false;
    }
    const AppSettings& s = *ctx.settings;
    if (key == QLatin1String("lts.placeCursorFirst")) {
        return s.ltsPlaceCursorFirst;
    }
    if (key == QLatin1String("setting.progressRadial")) {
        return s.progressRadial;
    }
    if (key == QLatin1String("setting.progressFill")) {
        return s.progressFill;
    }
    if (key == QLatin1String("setting.progressBorder")) {
        return s.progressBorder;
    }
    if (key == QLatin1String("setting.mouseProgressRadial")) {
        return s.mouseProgressRadial;
    }
    if (key == QLatin1String("setting.mouseProgressFill")) {
        return s.mouseProgressFill;
    }
    if (key == QLatin1String("setting.mouseProgressBorder")) {
        return s.mouseProgressBorder;
    }
    if (key == QLatin1String("setting.flashOnComplete")) {
        return s.flashOnComplete;
    }
    if (key == QLatin1String("setting.autoCollapseMain")) {
        return s.autoCollapseMain;
    }
    if (key == QLatin1String("setting.startDocked")) {
        return s.startDocked;
    }
    if (key == QLatin1String("setting.speakAlsoType")) {
        return s.speakAlsoType;
    }
    if (key == QLatin1String("setting.magFollow.0")) {
        return s.magFollowProfile == 0;
    }
    if (key == QLatin1String("setting.magFollow.1")) {
        return s.magFollowProfile == 1;
    }
    if (key == QLatin1String("setting.magFollow.2")) {
        return s.magFollowProfile == 2;
    }
    if (key == QLatin1String("setting.tracker.0")) {
        return s.trackerPref == 0;
    }
    if (key == QLatin1String("setting.tracker.1")) {
        return s.trackerPref == 1;
    }
    if (key == QLatin1String("setting.theme.dark")) {
        return s.themeMode == ThemeMode::Dark;
    }
    if (key == QLatin1String("setting.theme.light")) {
        return s.themeMode == ThemeMode::Light;
    }
    if (key == QLatin1String("setting.theme.custom")) {
        return s.themeMode == ThemeMode::Custom;
    }
    return false;
}

} // namespace gazer
