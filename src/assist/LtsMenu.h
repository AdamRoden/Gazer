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

/// Clockwise from 12 o'clock: Faster, Reset, Quit, CycleMode, Slower.
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

[[nodiscard]] inline LtsMenuAction ltsMenuActionFromHit(ComboMouseHit::Band band,
                                                        ComboMouseHit::Slice slice)
{
    if (band == ComboMouseHit::Band::Deadzone || band == ComboMouseHit::Band::Drift) {
        return LtsMenuAction::Resume;
    }
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

} // namespace gazer
