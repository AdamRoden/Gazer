#pragma once

#include "core/GazePoint.h"

namespace gazer {

class DockRevealOverlay;
class LayoutInstanceManager;
class LookToScroll;
class MagnifierOverlay;
class MouseDwellMove;

/// Ordered gaze consumers — keeps Application free of priority special-cases.
class GazeRouter {
public:
    void setInstances(LayoutInstanceManager* instances) { m_instances = instances; }
    void setDockReveal(DockRevealOverlay* dock) { m_dockReveal = dock; }
    void setLookToScroll(LookToScroll* lts) { m_lookToScroll = lts; }
    void setMouseDwellMove(MouseDwellMove* move) { m_mouseDwell = move; }
    void setMagnifier(MagnifierOverlay* mag) { m_magnifier = mag; }

    void dispatch(const GazePoint& point);

private:
    LayoutInstanceManager* m_instances = nullptr;
    DockRevealOverlay* m_dockReveal = nullptr;
    LookToScroll* m_lookToScroll = nullptr;
    MouseDwellMove* m_mouseDwell = nullptr;
    MagnifierOverlay* m_magnifier = nullptr;
};

} // namespace gazer
