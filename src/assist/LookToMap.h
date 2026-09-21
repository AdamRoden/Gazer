#pragma once

#include "assist/LtsIndicator.h"
#include "assist/LtsScrollMode.h"
#include "assist/LtsSpeed.h"

#include <QtGlobal>
#include <QString>

namespace gazer {

enum class LookToDest {
    Scroll = 0,
    Mouse = 1,
    LeftStick = 2,
    RightStick = 3,
};

inline constexpr int kLookToDestCount = 4;

enum class LookToRing {
    None = 0,
    Deadzone,
    Ramp,
    Full,
    Outer,
};

inline constexpr double kLookToMouseSpeedStops[] = {200.0, 400.0, 800.0, 1600.0, 3200.0};
inline constexpr int kLookToMouseSpeedStopCount =
    int(sizeof(kLookToMouseSpeedStops) / sizeof(kLookToMouseSpeedStops[0]));
inline constexpr double kLookToMouseSpeedDefault = 800.0;

inline constexpr double kLookToStickSpeedStops[] = {0.25, 0.50, 0.75, 1.00};
inline constexpr int kLookToStickSpeedStopCount =
    int(sizeof(kLookToStickSpeedStops) / sizeof(kLookToStickSpeedStops[0]));
inline constexpr double kLookToStickSpeedDefault = 1.0;

/// Pixel radii for the analog gaze disk. Output is 0 inside `deadzonePx`, ramps
/// 0–100% until `rampEndPx`, holds 100% until `fullOuterPx`, then either stays
/// at 100% or drops to 0 when `outerDeadzoneEnabled`.
struct LookToMapSettings {
    int deadzonePx = 80;
    int rampEndPx = 380;
    int fullOuterPx = 460;
    int outerDeadzonePx = 540;
    bool outerDeadzoneEnabled = false;
    bool hubEnabled = true;
    double maxSpeed = kLtsSpeedDefault;
    double accelPerSec = kLtsAccelDefault;
    int centerDwellMs = 700;
    LtsScrollMode axisMode = LtsScrollMode::Both;
    bool showPause = true;
    bool showInnerDeadzone = true;
    bool showMax = true;
    bool showOuterDeadzone = true;
    bool showBorder = true;
    bool showFill = true;
};

[[nodiscard]] inline LookToDest lookToDestFromInt(int v)
{
    if (v <= int(LookToDest::Scroll)) {
        return LookToDest::Scroll;
    }
    if (v >= int(LookToDest::RightStick)) {
        return LookToDest::RightStick;
    }
    return static_cast<LookToDest>(v);
}

[[nodiscard]] inline const char* lookToDestId(LookToDest d)
{
    switch (d) {
    case LookToDest::Mouse:
        return "mouse";
    case LookToDest::LeftStick:
        return "leftStick";
    case LookToDest::RightStick:
        return "rightStick";
    case LookToDest::Scroll:
        return "scroll";
    }
    return "scroll";
}

[[nodiscard]] inline const char* lookToDestLabel(LookToDest d)
{
    switch (d) {
    case LookToDest::Mouse:
        return "Look to mouse";
    case LookToDest::LeftStick:
        return "Look to left stick";
    case LookToDest::RightStick:
        return "Look to right stick";
    case LookToDest::Scroll:
        return "Look to scroll";
    }
    return "Look to scroll";
}

[[nodiscard]] inline const char* lookToDestShortLabel(LookToDest d)
{
    switch (d) {
    case LookToDest::Mouse:
        return "Mouse";
    case LookToDest::LeftStick:
        return "Left stick";
    case LookToDest::RightStick:
        return "Right stick";
    case LookToDest::Scroll:
        return "Scroll";
    }
    return "Scroll";
}

[[nodiscard]] inline const char* lookToDestCommand(LookToDest d)
{
    switch (d) {
    case LookToDest::Mouse:
        return "lookToMouse";
    case LookToDest::LeftStick:
        return "lookToLeftStick";
    case LookToDest::RightStick:
        return "lookToRightStick";
    case LookToDest::Scroll:
        return "lookToScroll";
    }
    return "lookToScroll";
}

[[nodiscard]] inline const char* lookToDestIcon(LookToDest d)
{
    switch (d) {
    case LookToDest::Mouse:
        return "mouseMove";
    case LookToDest::LeftStick:
    case LookToDest::RightStick:
        return "sportsEsports";
    case LookToDest::Scroll:
        return "lookToScroll";
    }
    return "lookToScroll";
}

[[nodiscard]] inline const char* lookToDestCaption(LookToDest d)
{
    switch (d) {
    case LookToDest::Mouse:
        return "Move the pointer by looking away from the origin.";
    case LookToDest::LeftStick:
        return "Drive the left analog stick from gaze vs the origin.";
    case LookToDest::RightStick:
        return "Drive the right analog stick from gaze vs the origin.";
    case LookToDest::Scroll:
        return "Scroll by looking away from the cursor.";
    }
    return "Scroll by looking away from the cursor.";
}

[[nodiscard]] inline LookToDest lookToDestFromId(const QString& id, bool* ok = nullptr)
{
    const QString s = id.trimmed();
    if (s.compare(QLatin1String("mouse"), Qt::CaseInsensitive) == 0) {
        if (ok) {
            *ok = true;
        }
        return LookToDest::Mouse;
    }
    if (s.compare(QLatin1String("leftStick"), Qt::CaseInsensitive) == 0
        || s.compare(QLatin1String("left"), Qt::CaseInsensitive) == 0) {
        if (ok) {
            *ok = true;
        }
        return LookToDest::LeftStick;
    }
    if (s.compare(QLatin1String("rightStick"), Qt::CaseInsensitive) == 0
        || s.compare(QLatin1String("right"), Qt::CaseInsensitive) == 0) {
        if (ok) {
            *ok = true;
        }
        return LookToDest::RightStick;
    }
    if (s.compare(QLatin1String("scroll"), Qt::CaseInsensitive) == 0) {
        if (ok) {
            *ok = true;
        }
        return LookToDest::Scroll;
    }
    if (ok) {
        *ok = false;
    }
    return LookToDest::Scroll;
}

[[nodiscard]] inline const double* lookToSpeedStops(LookToDest d, int* count)
{
    switch (d) {
    case LookToDest::Mouse:
        if (count) {
            *count = kLookToMouseSpeedStopCount;
        }
        return kLookToMouseSpeedStops;
    case LookToDest::LeftStick:
    case LookToDest::RightStick:
        if (count) {
            *count = kLookToStickSpeedStopCount;
        }
        return kLookToStickSpeedStops;
    case LookToDest::Scroll:
        if (count) {
            *count = kLtsSpeedStopCount;
        }
        return kLtsSpeedStops;
    }
    if (count) {
        *count = kLtsSpeedStopCount;
    }
    return kLtsSpeedStops;
}

[[nodiscard]] inline int nearestLookToSpeedIndex(LookToDest d, double n)
{
    int count = 0;
    const double* stops = lookToSpeedStops(d, &count);
    int idx = 0;
    double bestD = qAbs(n - stops[0]);
    for (int i = 1; i < count; ++i) {
        const double dist = qAbs(n - stops[i]);
        if (dist <= bestD) {
            bestD = dist;
            idx = i;
        }
    }
    return idx;
}

[[nodiscard]] inline double snapLookToSpeed(LookToDest d, double n)
{
    int count = 0;
    const double* stops = lookToSpeedStops(d, &count);
    return stops[nearestLookToSpeedIndex(d, n)];
}

[[nodiscard]] inline double nudgeLookToSpeed(LookToDest d, double n, int dir)
{
    int count = 0;
    const double* stops = lookToSpeedStops(d, &count);
    const int idx = nearestLookToSpeedIndex(d, n);
    if (dir > 0) {
        if (n < stops[idx] - 0.001) {
            return stops[idx];
        }
        return stops[qMin(idx + 1, count - 1)];
    }
    if (dir < 0) {
        if (n > stops[idx] + 0.001) {
            return stops[idx];
        }
        return stops[qMax(idx - 1, 0)];
    }
    return stops[idx];
}

[[nodiscard]] inline QString lookToSpeedText(LookToDest d, double n)
{
    const double v = snapLookToSpeed(d, n);
    switch (d) {
    case LookToDest::Mouse:
        return QStringLiteral("%1 px/s").arg(int(v));
    case LookToDest::LeftStick:
    case LookToDest::RightStick:
        return QStringLiteral("%1×").arg(v, 0, 'f', 2);
    case LookToDest::Scroll:
        return QStringLiteral("%1 n/s").arg(v, 0, 'g', 3);
    }
    return QString::number(v);
}

[[nodiscard]] inline LookToMapSettings defaultLookToMapSettings(LookToDest dest)
{
    LookToMapSettings c;
    c.deadzonePx = 80;
    c.rampEndPx = 380;
    c.fullOuterPx = 460;
    c.outerDeadzonePx = 540;
    c.outerDeadzoneEnabled = false;
    c.hubEnabled = true;
    c.accelPerSec = kLtsAccelDefault;
    c.centerDwellMs = 700;
    c.axisMode = LtsScrollMode::Both;
    c.showPause = true;
    c.showInnerDeadzone = true;
    c.showMax = true;
    c.showOuterDeadzone = true;
    c.showBorder = true;
    c.showFill = true;
    switch (dest) {
    case LookToDest::Mouse:
        c.maxSpeed = kLookToMouseSpeedDefault;
        break;
    case LookToDest::LeftStick:
    case LookToDest::RightStick:
        c.maxSpeed = kLookToStickSpeedDefault;
        break;
    case LookToDest::Scroll:
        c.maxSpeed = kLtsSpeedDefault;
        break;
    }
    return c;
}

inline void clampLookToMapSettings(LookToDest dest, LookToMapSettings& c)
{
    c.deadzonePx = qBound(20, c.deadzonePx, 400);
    c.rampEndPx = qBound(c.deadzonePx + 40, c.rampEndPx, 900);
    c.fullOuterPx = qBound(c.rampEndPx, c.fullOuterPx, 1200);
    c.outerDeadzonePx = qBound(c.fullOuterPx + 20, c.outerDeadzonePx, 1600);
    c.accelPerSec = qBound(kLtsAccelMin, c.accelPerSec, kLtsAccelMax);
    c.centerDwellMs = qBound(200, c.centerDwellMs, 2500);
    c.axisMode = ltsScrollModeFromInt(int(c.axisMode));
    c.maxSpeed = snapLookToSpeed(dest, c.maxSpeed);
}

/// 0 inside the deadzone, 0–1 across the ramp, 1 across the 100% annulus,
/// then 0 past `fullOuterPx` when the outer deadzone is on.
[[nodiscard]] inline double lookToGain(double dist, const LookToMapSettings& c)
{
    const double dz = double(qMax(1, c.deadzonePx));
    const double rampEnd = double(qMax(c.deadzonePx + 1, c.rampEndPx));
    const double fullOuter = double(qMax(c.rampEndPx, c.fullOuterPx));
    if (dist <= dz) {
        return 0.0;
    }
    if (dist < rampEnd) {
        return easeLtsFalloff((dist - dz) / (rampEnd - dz));
    }
    if (dist <= fullOuter) {
        return 1.0;
    }
    if (c.outerDeadzoneEnabled) {
        return 0.0;
    }
    return 1.0;
}

[[nodiscard]] inline int lookToMaxRadiusPx(const LookToMapSettings& c)
{
    return c.outerDeadzoneEnabled ? qMax(c.fullOuterPx, c.outerDeadzonePx) : c.fullOuterPx;
}

[[nodiscard]] inline int lookToOverlayRadiusPx(const LookToMapSettings& c)
{
    int r = 0;
    if (c.showInnerDeadzone) {
        r = qMax(r, c.deadzonePx);
    }
    if (c.showMax) {
        r = qMax(r, c.fullOuterPx);
    }
    if (c.outerDeadzoneEnabled && c.showOuterDeadzone) {
        r = qMax(r, c.outerDeadzonePx);
    }
    return qMax(r, 40);
}

[[nodiscard]] inline bool lookToShowsAnyRing(const LookToMapSettings& c)
{
    return c.showInnerDeadzone || c.showMax || c.showOuterDeadzone;
}

inline void applyLegacyLookToIndicator(LookToMapSettings& c, LtsIndicator style)
{
    style = ltsIndicatorFromInt(int(style));
    c.showPause = true;
    c.showBorder = true;
    switch (style) {
    case LtsIndicator::Hollow:
        c.showFill = false;
        c.showInnerDeadzone = true;
        c.showMax = true;
        c.showOuterDeadzone = true;
        break;
    case LtsIndicator::PauseOnly:
        c.showFill = true;
        c.showInnerDeadzone = false;
        c.showMax = false;
        c.showOuterDeadzone = false;
        break;
    case LtsIndicator::Filled:
        c.showFill = true;
        c.showInnerDeadzone = true;
        c.showMax = true;
        c.showOuterDeadzone = true;
        break;
    }
}

/// Inner hysteresis uses the deadzone; the outer deadzone is a hard stop.
[[nodiscard]] inline bool lookToKeepEngaged(bool engaged, double dist, const LookToMapSettings& c)
{
    if (c.outerDeadzoneEnabled && dist > double(c.fullOuterPx)) {
        return false;
    }
    return ltsKeepScrolling(engaged, dist, c.deadzonePx);
}

[[nodiscard]] inline LookToMapSettings lookToMapFromLegacyLts(int deadzonePx, int falloffPx,
                                                              double maxNotches, double accel,
                                                              int centerDwell, LtsIndicator style,
                                                              LtsScrollMode mode)
{
    LookToMapSettings c = defaultLookToMapSettings(LookToDest::Scroll);
    c.deadzonePx = deadzonePx;
    c.rampEndPx = deadzonePx + qMax(40, falloffPx);
    c.fullOuterPx = c.rampEndPx + 80;
    c.outerDeadzonePx = c.fullOuterPx + 80;
    c.maxSpeed = maxNotches;
    c.accelPerSec = accel;
    c.centerDwellMs = centerDwell;
    c.axisMode = mode;
    applyLegacyLookToIndicator(c, style);
    clampLookToMapSettings(LookToDest::Scroll, c);
    return c;
}

} // namespace gazer
