#include "app/ActiveStateResolver.h"

#include "app/AppSettings.h"
#include "assist/ActionLoopService.h"
#include "assist/GazeMouseFollow.h"
#include "assist/GazeReticle.h"
#include "assist/ComboMouse.h"
#include "assist/LookToScroll.h"
#include "assist/MouseAssistState.h"
#include "assist/MouseDwellMove.h"
#include "ui/MagnifierOverlay.h"
#include "app/SettingsUi.h"
#include "ui/PickStyle.h"
#include "ui/Theme.h"

namespace gazer {

namespace {
bool indexKeyEquals(const QString& key, QLatin1String prefix, int value)
{
    if (!key.startsWith(prefix)) {
        return false;
    }
    bool ok = false;
    return key.mid(prefix.size()).toInt(&ok) == value && ok;
}
} // namespace

bool resolveActiveState(const ActiveStateContext& ctx, const QString& key)
{
    if (key.startsWith(QLatin1Char('!'))) {
        return !resolveActiveState(ctx, key.mid(1).trimmed());
    }
    if (key == QLatin1String("dwellSuspend") || key == QLatin1String("dwell.suspended")) {
        return ctx.dwellSuspended;
    }
    if (key == QLatin1String("lookToScroll")) {
        return ctx.lookToScroll && ctx.lookToScroll->isEnabled();
    }
    if (key == QLatin1String("lookToScroll.suspended")) {
        return ctx.lookToScroll && ctx.lookToScroll->isScrollSuspended();
    }
    if (key == QLatin1String("comboMouse")) {
        return ctx.comboMouse && ctx.comboMouse->isEnabled();
    }
    if (key == QLatin1String("comboMouse.drag")) {
        return ctx.comboMouse && ctx.comboMouse->isDragHeld();
    }
    if (key == QLatin1String("mouseDwellMove")) {
        return ctx.mouseDwellMove && ctx.mouseDwellMove->isArmed()
               && ctx.mouseDwellMove->armPurpose() == MouseDwellMove::ArmPurpose::CursorMove;
    }
    if (key == QLatin1String("mouseMoveAndLeftClick")) {
        return ctx.mouseDwellMove && ctx.mouseDwellMove->isArmed()
               && ctx.mouseDwellMove->armPurpose()
                      == MouseDwellMove::ArmPurpose::CursorMoveLeftClick;
    }
    if (key == QLatin1String("mouseMoveAndRightClick")) {
        return ctx.mouseDwellMove && ctx.mouseDwellMove->isArmed()
               && ctx.mouseDwellMove->armPurpose()
                      == MouseDwellMove::ArmPurpose::CursorMoveRightClick;
    }
    if (key == QLatin1String("mouseMoveAndMiddleClick")) {
        return ctx.mouseDwellMove && ctx.mouseDwellMove->isArmed()
               && ctx.mouseDwellMove->armPurpose()
                      == MouseDwellMove::ArmPurpose::CursorMoveMiddleClick;
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
    if (key == QLatin1String("mouseMoveMagPickFullScreen")) {
        return ctx.settings && ctx.settings->mouseMoveMagPickFullScreen;
    }
    if (key == QLatin1String("mouseMoveForesight")) {
        return ctx.settings && ctx.settings->mouseMoveForesight;
    }
    if (key == QLatin1String("mouseMoveForesightDoubleZoom")) {
        return ctx.settings && ctx.settings->mouseMoveForesightDoubleZoom;
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
    if (indexKeyEquals(key, QLatin1String("setting.ltsIndicator."), int(s.ltsIndicatorStyle))) {
        return true;
    }
    if (key == QLatin1String("setting.dwell.slow")) {
        return s.dwellPreset() == 0;
    }
    if (key == QLatin1String("setting.dwell.normal")) {
        return s.dwellPreset() == 1;
    }
    if (key == QLatin1String("setting.dwell.fast")) {
        return s.dwellPreset() == 2;
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
    if (key == QLatin1String("setting.flashUseForeground")) {
        return s.flashUseForeground;
    }
    if (key == QLatin1String("setting.flashUseCustom")) {
        return !s.flashUseForeground;
    }
    if (key == QLatin1String("setting.magPick.cursor")) {
        return PickStyle::has(s.magPickStyle, PickStyle::Cursor);
    }
    if (key == QLatin1String("setting.magPick.dot")) {
        return PickStyle::has(s.magPickStyle, PickStyle::Dot);
    }
    if (key == QLatin1String("setting.magPick.crosshair")) {
        return PickStyle::has(s.magPickStyle, PickStyle::Crosshair);
    }
    if (key == QLatin1String("setting.magPick.gaze")) {
        return PickStyle::has(s.magPickStyle, PickStyle::GazeIndicator);
    }
    if (key == QLatin1String("setting.mousePick.cursor")) {
        return PickStyle::has(s.mousePickStyle, PickStyle::Cursor);
    }
    if (key == QLatin1String("setting.mousePick.dot")) {
        return PickStyle::has(s.mousePickStyle, PickStyle::Dot);
    }
    if (key == QLatin1String("setting.pickWindow.round")) {
        return s.pickWindowRound;
    }
    if (key == QLatin1String("setting.pickWindow.square")) {
        return !s.pickWindowRound;
    }
    if (key == QLatin1String("setting.mousePick.crosshair")) {
        return PickStyle::has(s.mousePickStyle, PickStyle::Crosshair);
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
    if (indexKeyEquals(key, QLatin1String("setting.magFollow."), s.magFollowProfile)) {
        return true;
    }
    if (indexKeyEquals(key, QLatin1String("setting.tracker."), s.trackerPref)) {
        return true;
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
    if (key == QLatin1String("setting.theme.contrast.low")) {
        return snapContrastPercent(s.customContrast) == kThemeContrastLowPct;
    }
    if (key == QLatin1String("setting.theme.contrast.medium")) {
        return snapContrastPercent(s.customContrast) == kThemeContrastMediumPct;
    }
    if (key == QLatin1String("setting.theme.contrast.high")) {
        return snapContrastPercent(s.customContrast) == kThemeContrastHighPct;
    }
    if (key.startsWith(QLatin1String("setting.color.editing."))) {
        const QString ck = key.mid(QStringLiteral("setting.color.editing.").size());
        return ctx.settingsUi && ctx.settingsUi->colorPickerKey() == ck;
    }
    return false;
}

} // namespace gazer
