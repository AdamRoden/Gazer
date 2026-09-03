#include "app/ActiveStateResolver.h"

#include "app/AppSettings.h"
#include "assist/ActionLoopService.h"
#include "assist/GazeMouseFollow.h"
#include "assist/GazeReticle.h"
#include "assist/ComboMouse.h"
#include "assist/LookToScroll.h"
#include "assist/MouseAssistState.h"
#include "assist/MouseDwellMove.h"
#include "input/KeyStateManager.h"
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
    if (key == QLatin1String("dwellSuspend")) {
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
    {
        using ArmPurpose = MouseDwellMove::ArmPurpose;
        static const struct {
            const char* id;
            ArmPurpose purpose;
        } kArm[] = {
            {"mouseMoveToGaze", ArmPurpose::CursorMove},
            {"mouseLeftClickAtGaze", ArmPurpose::CursorMoveLeftClick},
            {"mouseRightClickAtGaze", ArmPurpose::CursorMoveRightClick},
            {"mouseMiddleClickAtGaze", ArmPurpose::CursorMoveMiddleClick},
        };
        for (const auto& e : kArm) {
            if (key == QLatin1String(e.id)) {
                return ctx.mouseDwellMove && ctx.mouseDwellMove->isArmed()
                       && ctx.mouseDwellMove->armPurpose() == e.purpose;
            }
        }
    }
    if (key == QLatin1String("mouseMoveToGazeClickLoop")) {
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
    if (key == QLatin1String("mouseMoveForesightSecondZoom")) {
        return ctx.settings && ctx.settings->mouseMoveForesightSecondZoom;
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
    if (key == QLatin1String("mod.any")) {
        return ctx.keyState && ctx.keyState->anyHeld();
    }
    if (key.startsWith(QLatin1String("mod."))) {
        const QString rest = key.mid(4);
        if (!ctx.keyState) {
            return false;
        }
        if (rest.endsWith(QLatin1String(".locked"))) {
            return ctx.keyState->isLocked(rest.left(rest.size() - 7));
        }
        if (rest.endsWith(QLatin1String(".down"))) {
            return ctx.keyState->state(rest.left(rest.size() - 5)) == KeyHoldState::Down;
        }
        return ctx.keyState->isHeld(rest);
    }
    if (!ctx.settings) {
        return false;
    }
    const AppSettings& s = *ctx.settings;
    for (const AppSettings::StyleToggle& t : AppSettings::kStyleToggles) {
        if (key == QLatin1String(t.command)) {
            return s.styleFlag(t);
        }
    }
    static const struct {
        const char* id;
        bool (*test)(const AppSettings&);
    } kSettings[] = {
        {"settings.dwell.slow", [](const AppSettings& s) { return s.dwellPreset() == 0; }},
        {"settings.dwell.normal", [](const AppSettings& s) { return s.dwellPreset() == 1; }},
        {"settings.dwell.fast", [](const AppSettings& s) { return s.dwellPreset() == 2; }},
        {"settings.dwell.custom", [](const AppSettings& s) { return s.dwellPreset() == 3; }},
        {"settings.flash.foreground", [](const AppSettings& s) { return s.flashUseForeground; }},
        {"settings.flash.custom", [](const AppSettings& s) { return !s.flashUseForeground; }},
        {"settings.magPickStyle.cursor.toggle",
         [](const AppSettings& s) { return PickStyle::has(s.magPickStyle, PickStyle::Cursor); }},
        {"settings.magPickStyle.dot.toggle",
         [](const AppSettings& s) { return PickStyle::has(s.magPickStyle, PickStyle::Dot); }},
        {"settings.magPickStyle.crosshair.toggle",
         [](const AppSettings& s) { return PickStyle::has(s.magPickStyle, PickStyle::Crosshair); }},
        {"settings.magPickStyle.gaze.toggle",
         [](const AppSettings& s) {
             return PickStyle::has(s.magPickStyle, PickStyle::GazeIndicator);
         }},
        {"settings.mousePickStyle.cursor.toggle",
         [](const AppSettings& s) { return PickStyle::has(s.mousePickStyle, PickStyle::Cursor); }},
        {"settings.mousePickStyle.dot.toggle",
         [](const AppSettings& s) { return PickStyle::has(s.mousePickStyle, PickStyle::Dot); }},
        {"settings.mousePickStyle.crosshair.toggle",
         [](const AppSettings& s) { return PickStyle::has(s.mousePickStyle, PickStyle::Crosshair); }},
        {"settings.pickWindow.round", [](const AppSettings& s) { return s.pickWindowRound; }},
        {"settings.pickWindow.square", [](const AppSettings& s) { return !s.pickWindowRound; }},
        {"settings.pickCenter.gaze",
         [](const AppSettings& s) { return s.mouseMoveMagPickCenterOnDwell; }},
        {"settings.pickCenter.screen",
         [](const AppSettings& s) { return !s.mouseMoveMagPickCenterOnDwell; }},
        {"settings.session.layoutAutoClose.toggle",
         [](const AppSettings& s) { return s.layoutAutoClose; }},
        {"settings.session.autoCollapse.toggle",
         [](const AppSettings& s) { return s.autoCollapseMain; }},
        {"settings.session.startDocked.toggle",
         [](const AppSettings& s) { return s.startDocked; }},
        {"settings.speech.alsoType.toggle", [](const AppSettings& s) { return s.speakAlsoType; }},
        {"settings.lts.indicator.fan",
         [](const AppSettings& s) { return int(s.ltsIndicatorStyle) == 0; }},
        {"settings.lts.indicator.orb",
         [](const AppSettings& s) { return int(s.ltsIndicatorStyle) == 1; }},
        {"settings.lts.indicator.pause",
         [](const AppSettings& s) { return int(s.ltsIndicatorStyle) == 2; }},
        {"settings.mag.follow.slow",
         [](const AppSettings& s) { return int(s.magFollowProfile) == 0; }},
        {"settings.mag.follow.sticky",
         [](const AppSettings& s) { return int(s.magFollowProfile) == 1; }},
        {"settings.mag.follow.smooth",
         [](const AppSettings& s) { return int(s.magFollowProfile) == 2; }},
        {"settings.mag.follow.snappy",
         [](const AppSettings& s) { return int(s.magFollowProfile) == 3; }},
        {"settings.tracker.auto", [](const AppSettings& s) { return s.trackerPref == 0; }},
        {"settings.tracker.mouse", [](const AppSettings& s) { return s.trackerPref == 1; }},
        {"theme.dark", [](const AppSettings& s) { return s.themeAppearance == ThemeAppearance::Dark; }},
        {"theme.darkTinted",
         [](const AppSettings& s) { return s.themeAppearance == ThemeAppearance::DarkTinted; }},
        {"theme.lightTinted",
         [](const AppSettings& s) { return s.themeAppearance == ThemeAppearance::LightTinted; }},
        {"theme.light",
         [](const AppSettings& s) { return s.themeAppearance == ThemeAppearance::Light; }},
        {"theme.custom", [](const AppSettings& s) { return s.themeCustom; }},
    };
    for (const auto& e : kSettings) {
        if (key == QLatin1String(e.id)) {
            return e.test(s);
        }
    }
    if (key.startsWith(QLatin1String("theme.primary."))) {
        return !s.themeCustom
               && indexKeyEquals(key, QLatin1String("theme.primary."), s.themePrimaryIndex);
    }
    if (key.startsWith(QLatin1String("theme.secondary."))) {
        return !s.themeCustom
               && indexKeyEquals(key, QLatin1String("theme.secondary."), s.themeSecondaryIndex);
    }
    if (key.startsWith(QLatin1String("settings.color.editing."))) {
        const QString ck = key.mid(QStringLiteral("settings.color.editing.").size());
        return ctx.settingsUi && ctx.settingsUi->colorPickerKey() == ck;
    }
    return false;
}

} // namespace gazer
