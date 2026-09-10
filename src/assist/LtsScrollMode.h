#pragma once

namespace gazer {

enum class LtsScrollMode {
    Vertical = 0,
    Horizontal = 1,
    Both = 2,
};

[[nodiscard]] inline LtsScrollMode ltsScrollModeFromInt(int v)
{
    if (v <= int(LtsScrollMode::Vertical)) {
        return LtsScrollMode::Vertical;
    }
    if (v >= int(LtsScrollMode::Both)) {
        return LtsScrollMode::Both;
    }
    return static_cast<LtsScrollMode>(v);
}

[[nodiscard]] inline LtsScrollMode cycleLtsScrollMode(LtsScrollMode m)
{
    return LtsScrollMode((int(m) + 1) % 3);
}

[[nodiscard]] inline const char* ltsScrollModeName(LtsScrollMode m)
{
    switch (m) {
    case LtsScrollMode::Horizontal:
        return "Horizontal";
    case LtsScrollMode::Both:
        return "Both";
    case LtsScrollMode::Vertical:
        return "Vertical";
    }
    return "Vertical";
}

[[nodiscard]] inline const char* ltsScrollModeIcon(LtsScrollMode m)
{
    switch (m) {
    case LtsScrollMode::Horizontal:
        return "lookToScrollHorizontal";
    case LtsScrollMode::Both:
        return "lookToScroll";
    case LtsScrollMode::Vertical:
        return "lookToScrollVertical";
    }
    return "lookToScrollVertical";
}

inline void applyLtsScrollMode(LtsScrollMode m, double& v, double& h)
{
    switch (m) {
    case LtsScrollMode::Vertical:
        h = 0.0;
        break;
    case LtsScrollMode::Horizontal:
        v = 0.0;
        break;
    case LtsScrollMode::Both:
        break;
    }
}

} // namespace gazer
