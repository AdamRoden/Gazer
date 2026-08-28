#pragma once

#include <QtGlobal>

namespace gazer {

enum class GazeFollowProfile {
    Slow = 0,
    Sticky = 1,
    Smooth = 2,
    Snappy = 3,
};

[[nodiscard]] inline GazeFollowProfile gazeFollowProfileFromInt(int v)
{
    return static_cast<GazeFollowProfile>(
        qBound(int(GazeFollowProfile::Slow), v, int(GazeFollowProfile::Snappy)));
}

[[nodiscard]] inline const char* gazeFollowProfileName(GazeFollowProfile p)
{
    switch (p) {
    case GazeFollowProfile::Slow:
        return "Slow";
    case GazeFollowProfile::Smooth:
        return "Smooth";
    case GazeFollowProfile::Snappy:
        return "Snappy";
    case GazeFollowProfile::Sticky:
        break;
    }
    return "Sticky";
}

} // namespace gazer
