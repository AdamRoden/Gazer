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
    Max,
    Outer,
};

enum class LookToPart { Pause = 0, Inner, Max, Outer };
inline constexpr int kLookToPartCount = 4;

struct LookToPartChrome {
    bool border = true;
    bool fill = true;
    [[nodiscard]] bool shown() const { return border || fill; }
};

struct LookToPartSpec {
    LookToPart part;
    const char* id;
    const char* label;
    const char* borderJson;
    const char* fillJson;
    const char* borderState;
    const char* fillState;
};

inline constexpr LookToPartSpec kLookToParts[] = {
    {LookToPart::Pause, "pause", "Pause", "borderPause", "fillPause", "lookTo.map.border.pause",
     "lookTo.map.fill.pause"},
    {LookToPart::Inner, "inner", "Inner", "borderInner", "fillInner", "lookTo.map.border.inner",
     "lookTo.map.fill.inner"},
    {LookToPart::Max, "max", "Max", "borderMax", "fillMax", "lookTo.map.border.max",
     "lookTo.map.fill.max"},
    {LookToPart::Outer, "outer", "Outer", "borderOuter", "fillOuter", "lookTo.map.border.outer",
     "lookTo.map.fill.outer"},
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
/// 0–100% until `maxPx`, stays at 100% outside that, and drops to 0 past
/// `outerDeadzonePx` when `outerDeadzoneEnabled`.
struct LookToMapSettings {
    int deadzonePx = 80;
    int maxPx = 380;
    int outerDeadzonePx = 540;
    bool outerDeadzoneEnabled = false;
    bool hubEnabled = true;
    double maxSpeed = kLtsSpeedDefault;
    double accelPerSec = kLtsAccelDefault;
    int centerDwellMs = 700;
    LtsScrollMode axisMode = LtsScrollMode::Both;
    /// Outline and fill for pause, inner, max, and outer. A part is drawn when either is on.
    LookToPartChrome part[kLookToPartCount]{};

    [[nodiscard]] LookToPartChrome& chrome(LookToPart p) { return part[int(p)]; }
    [[nodiscard]] const LookToPartChrome& chrome(LookToPart p) const { return part[int(p)]; }
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
    c.maxPx = qBound(c.deadzonePx + 40, c.maxPx, 900);
    c.outerDeadzonePx = qBound(c.maxPx + 20, c.outerDeadzonePx, 1600);
    c.accelPerSec = qBound(kLtsAccelMin, c.accelPerSec, kLtsAccelMax);
    c.centerDwellMs = qBound(200, c.centerDwellMs, 2500);
    c.axisMode = ltsScrollModeFromInt(int(c.axisMode));
    c.maxSpeed = snapLookToSpeed(dest, c.maxSpeed);
}

/// 0 inside the deadzone, 0–1 across the ramp, 1 outside the ramp, then 0 past
/// the outer deadzone when that edge is on.
[[nodiscard]] inline double lookToGain(double dist, const LookToMapSettings& c)
{
    const double dz = double(qMax(1, c.deadzonePx));
    const double maxR = double(qMax(c.deadzonePx + 1, c.maxPx));
    const double outer = double(qMax(c.maxPx + 1, c.outerDeadzonePx));
    if (dist <= dz) {
        return 0.0;
    }
    if (dist < maxR) {
        return easeLtsFalloff((dist - dz) / (maxR - dz));
    }
    if (c.outerDeadzoneEnabled && dist > outer) {
        return 0.0;
    }
    return 1.0;
}

[[nodiscard]] inline bool lookToPartShown(const LookToMapSettings& c, LookToPart p)
{
    return c.chrome(p).shown();
}

/// Outline width shared by the preview rings and the live orb.
[[nodiscard]] inline double lookToRingStrokePx(double radius)
{
    return qBound(2.4, radius * 0.06, 6.0);
}

[[nodiscard]] inline int lookToOverlayRadiusPx(const LookToMapSettings& c)
{
    int r = 0;
    if (lookToPartShown(c, LookToPart::Inner)) {
        r = qMax(r, c.deadzonePx);
    }
    if (lookToPartShown(c, LookToPart::Max)) {
        r = qMax(r, c.maxPx);
    }
    if (c.outerDeadzoneEnabled && lookToPartShown(c, LookToPart::Outer)) {
        r = qMax(r, c.outerDeadzonePx);
    }
    return qMax(r, 40);
}

[[nodiscard]] inline bool lookToShowsAnyRing(const LookToMapSettings& c)
{
    return lookToPartShown(c, LookToPart::Inner) || lookToPartShown(c, LookToPart::Max)
           || lookToPartShown(c, LookToPart::Outer);
}

/// Old global border/fill times which parts were visible.
inline void applyLookToPartStyle(LookToMapSettings& c, bool pause, bool inner, bool max, bool outer,
                                 bool border, bool fill)
{
    const bool shown[kLookToPartCount] = {pause, inner, max, outer};
    for (int i = 0; i < kLookToPartCount; ++i) {
        c.part[i].border = shown[i] && border;
        c.part[i].fill = shown[i] && fill;
    }
}

inline void applyLegacyLookToIndicator(LookToMapSettings& c, LtsIndicator style)
{
    style = ltsIndicatorFromInt(int(style));
    switch (style) {
    case LtsIndicator::Hollow:
        applyLookToPartStyle(c, true, true, true, true, true, false);
        break;
    case LtsIndicator::PauseOnly:
        applyLookToPartStyle(c, true, false, false, false, true, true);
        break;
    case LtsIndicator::Filled:
        applyLookToPartStyle(c, true, true, true, true, true, true);
        break;
    }
}

/// Inner hysteresis uses the deadzone; the outer deadzone is a hard stop.
[[nodiscard]] inline bool lookToKeepEngaged(bool engaged, double dist, const LookToMapSettings& c)
{
    if (c.outerDeadzoneEnabled && dist > double(c.outerDeadzonePx)) {
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
    c.maxPx = deadzonePx + qMax(40, falloffPx);
    c.outerDeadzonePx = c.maxPx + 160;
    c.maxSpeed = maxNotches;
    c.accelPerSec = accel;
    c.centerDwellMs = centerDwell;
    c.axisMode = mode;
    applyLegacyLookToIndicator(c, style);
    clampLookToMapSettings(LookToDest::Scroll, c);
    return c;
}

} // namespace gazer
