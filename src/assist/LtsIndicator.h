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

} // namespace gazer
