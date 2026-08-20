#pragma once

namespace gazer {

enum class LtsIndicator {
    Fan = 0,
    Orb = 1,
    PauseOnly = 2,
};

[[nodiscard]] inline LtsIndicator ltsIndicatorFromInt(int v)
{
    if (v <= 0) {
        return LtsIndicator::Fan;
    }
    if (v >= int(LtsIndicator::PauseOnly)) {
        return LtsIndicator::PauseOnly;
    }
    return static_cast<LtsIndicator>(v);
}

[[nodiscard]] inline const char* ltsIndicatorName(LtsIndicator s)
{
    switch (s) {
    case LtsIndicator::Orb:
        return "Orb";
    case LtsIndicator::PauseOnly:
        return "Pause";
    case LtsIndicator::Fan:
        return "Fan";
    }
    return "Fan";
}

} // namespace gazer
