#pragma once

#include "assist/ComboMouseHit.h"
#include "assist/LtsScrollMode.h"

#include <QString>

namespace gazer {

enum class LtsMenuAction {
    None = 0,
    Resume,
    Faster,
    Slower,
    Reset,
    Quit,
    CycleMode,
};

/// ComboMouse Slice index → pie action. Resume is the `lts.resume` board command, not a pie hit.
inline constexpr LtsMenuAction kLtsSliceActions[ComboMouseHit::kSliceCount] = {
    LtsMenuAction::Faster,
    LtsMenuAction::Reset,
    LtsMenuAction::Quit,
    LtsMenuAction::CycleMode,
    LtsMenuAction::Slower,
};

inline constexpr const char* kLtsSliceIcons[ComboMouseHit::kSliceCount] = {
    "LookToScrollSpeedFast",
    "mouseMove",
    "close",
    "",
    "LookToScrollSpeedSlow",
};

/// Pie hits: slice → action. Hole / ring / miss → None (Resume is not a pie activator).
[[nodiscard]] inline LtsMenuAction ltsMenuActionFromHit(ComboMouseHit::Band band,
                                                        ComboMouseHit::Slice slice)
{
    if (band != ComboMouseHit::Band::Slice) {
        return LtsMenuAction::None;
    }
    const int i = int(slice);
    if (i < 0 || i >= ComboMouseHit::kSliceCount) {
        return LtsMenuAction::None;
    }
    return kLtsSliceActions[i];
}

[[nodiscard]] inline const char* ltsMenuActionId(LtsMenuAction a)
{
    switch (a) {
    case LtsMenuAction::Resume:
        return "resume";
    case LtsMenuAction::Faster:
        return "faster";
    case LtsMenuAction::Slower:
        return "slower";
    case LtsMenuAction::Reset:
        return "reset";
    case LtsMenuAction::Quit:
        return "quit";
    case LtsMenuAction::CycleMode:
        return "cycle";
    case LtsMenuAction::None:
        return "";
    }
    return "";
}

[[nodiscard]] inline LtsMenuAction ltsMenuActionFromId(const QString& id)
{
    if (id == QLatin1String("resume")) {
        return LtsMenuAction::Resume;
    }
    if (id == QLatin1String("faster")) {
        return LtsMenuAction::Faster;
    }
    if (id == QLatin1String("slower")) {
        return LtsMenuAction::Slower;
    }
    if (id == QLatin1String("reset")) {
        return LtsMenuAction::Reset;
    }
    if (id == QLatin1String("quit")) {
        return LtsMenuAction::Quit;
    }
    if (id == QLatin1String("cycle")) {
        return LtsMenuAction::CycleMode;
    }
    return LtsMenuAction::None;
}

inline void fillLtsSliceIcons(LtsScrollMode mode, const char* out[ComboMouseHit::kSliceCount])
{
    for (int i = 0; i < ComboMouseHit::kSliceCount; ++i) {
        out[i] = kLtsSliceActions[i] == LtsMenuAction::CycleMode ? ltsScrollModeIcon(mode)
                                                                : kLtsSliceIcons[i];
    }
}

[[nodiscard]] constexpr ComboMouseHit::Slice sliceForLtsAction(LtsMenuAction a)
{
    for (int i = 0; i < ComboMouseHit::kSliceCount; ++i) {
        if (kLtsSliceActions[i] == a) {
            return ComboMouseHit::Slice(i);
        }
    }
    return ComboMouseHit::Slice::Right;
}

[[nodiscard]] constexpr ComboMouseHit::ComboPack ltsPack(double fillStartDeg, bool clockwise,
                                                         double outerScale, LtsMenuAction a,
                                                         LtsMenuAction b, LtsMenuAction c,
                                                         LtsMenuAction d, LtsMenuAction e)
{
    return {fillStartDeg,
            clockwise,
            outerScale,
            {sliceForLtsAction(a), sliceForLtsAction(b), sliceForLtsAction(c), sliceForLtsAction(d),
             sliceForLtsAction(e)}};
}

/// LTS pie rows indexed by ComboMouseHit::Region. The nine rows are the spec.
inline constexpr ComboMouseHit::ComboPack kLtsPack[ComboMouseHit::kRegionCount] = {
    ltsPack(0.0, true, 1.0, LtsMenuAction::Faster, LtsMenuAction::Reset, LtsMenuAction::Quit,
            LtsMenuAction::CycleMode, LtsMenuAction::Slower),
    ltsPack(0.0, true, 1.0, LtsMenuAction::Slower, LtsMenuAction::Faster, LtsMenuAction::CycleMode,
            LtsMenuAction::Reset, LtsMenuAction::Quit),
    ltsPack(180.0, true, 1.0, LtsMenuAction::Quit, LtsMenuAction::Reset, LtsMenuAction::Slower,
            LtsMenuAction::Faster, LtsMenuAction::CycleMode),
    ltsPack(90.0, true, 1.0, LtsMenuAction::Quit, LtsMenuAction::Reset, LtsMenuAction::CycleMode,
            LtsMenuAction::Faster, LtsMenuAction::Slower),
    ltsPack(270.0, true, 1.0, LtsMenuAction::Slower, LtsMenuAction::Faster, LtsMenuAction::CycleMode,
            LtsMenuAction::Reset, LtsMenuAction::Quit),
    ltsPack(90.0, true, ComboMouseHit::kCornerOuterScale, LtsMenuAction::Quit, LtsMenuAction::Reset,
            LtsMenuAction::CycleMode, LtsMenuAction::Faster, LtsMenuAction::Slower),
    ltsPack(180.0, true, ComboMouseHit::kCornerOuterScale, LtsMenuAction::Quit, LtsMenuAction::Reset,
            LtsMenuAction::CycleMode, LtsMenuAction::Faster, LtsMenuAction::Slower),
    ltsPack(270.0, true, ComboMouseHit::kCornerOuterScale, LtsMenuAction::Slower,
            LtsMenuAction::Faster, LtsMenuAction::CycleMode, LtsMenuAction::Reset,
            LtsMenuAction::Quit),
    ltsPack(0.0, true, ComboMouseHit::kCornerOuterScale, LtsMenuAction::Slower, LtsMenuAction::Faster,
            LtsMenuAction::CycleMode, LtsMenuAction::Reset, LtsMenuAction::Quit),
};

[[nodiscard]] inline ComboMouseHit::Layout makeLtsLayout(QPointF origin, const QRectF& screen,
                                                         double deadzone, double ringOuter,
                                                         double fullPieOuter)
{
    return ComboMouseHit::makePackedLayout(origin, screen, deadzone, ringOuter, fullPieOuter,
                                           kLtsPack);
}

} // namespace gazer
