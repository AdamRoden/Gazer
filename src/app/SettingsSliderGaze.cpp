#include "app/SettingsUi.h"

namespace gazer {

void SettingsUi::onGaze(const GazePoint& point)
{
    if (m_curveScrub) {
        feedCurveGaze(point);
    }
}

} // namespace gazer
