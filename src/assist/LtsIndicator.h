#pragma once

namespace gazer {

enum class LtsIndicator {
    Filled = 0,
    Hollow = 1,
    PauseOnly = 2,
};

[[nodiscard]] inline LtsIndicator ltsIndicatorFromInt(int v)
{
    if (v <= 0) {
        return LtsIndicator::Filled;
    }
    if (v >= int(LtsIndicator::PauseOnly)) {
        return LtsIndicator::PauseOnly;
    }
    return static_cast<LtsIndicator>(v);
}

[[nodiscard]] inline const char* ltsIndicatorName(LtsIndicator s)
{
    switch (s) {
    case LtsIndicator::Hollow:
        return "Hollow";
    case LtsIndicator::PauseOnly:
        return "Pause";
    case LtsIndicator::Filled:
        return "Filled";
    }
    return "Filled";
}

/// Canonical commands plus `fan`/`orb` aliases (old JSON 0/1 already map via the enum).
struct LtsIndicatorCommand {
    const char* cmd;
    LtsIndicator style;
    const char* status;
};

inline constexpr LtsIndicatorCommand kLtsIndicatorCommands[] = {
    {"settings.lts.indicator.filled", LtsIndicator::Filled, "LTS indicator: Filled"},
    {"settings.lts.indicator.hollow", LtsIndicator::Hollow, "LTS indicator: Hollow"},
    {"settings.lts.indicator.pause", LtsIndicator::PauseOnly, "LTS indicator: Pause only"},
    {"settings.lts.indicator.fan", LtsIndicator::Filled, "LTS indicator: Filled"},
    {"settings.lts.indicator.orb", LtsIndicator::Hollow, "LTS indicator: Hollow"},
};

} // namespace gazer
