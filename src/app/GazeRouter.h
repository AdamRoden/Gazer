#pragma once

#include "core/GazePoint.h"

namespace gazer {

class AssistSession;
class DockRevealOverlay;
class GazeMouseFollow;
class GazeReticle;
class LayoutInstanceManager;
class LookToScroll;
class MagnifierOverlay;
class MouseDwellMove;

/// Ordered gaze consumers. Board/aim policy comes only from AssistSession.
class GazeRouter {
public:
    void setInstances(LayoutInstanceManager* instances) { m_instances = instances; }
    void setDockReveal(DockRevealOverlay* dock) { m_dockReveal = dock; }
    void setAssistSession(AssistSession* session) { m_session = session; }
    void setLookToScroll(LookToScroll* lts) { m_lookToScroll = lts; }
    void setMouseDwellMove(MouseDwellMove* move) { m_mouseDwell = move; }
    void setMagnifier(MagnifierOverlay* mag) { m_magnifier = mag; }
    void setGazeReticle(GazeReticle* r) { m_gazeReticle = r; }
    void setGazeMouseFollow(GazeMouseFollow* f) { m_gazeFollow = f; }

    void dispatch(const GazePoint& point);

private:
    LayoutInstanceManager* m_instances = nullptr;
    DockRevealOverlay* m_dockReveal = nullptr;
    AssistSession* m_session = nullptr;
    LookToScroll* m_lookToScroll = nullptr;
    MouseDwellMove* m_mouseDwell = nullptr;
    MagnifierOverlay* m_magnifier = nullptr;
    GazeReticle* m_gazeReticle = nullptr;
    GazeMouseFollow* m_gazeFollow = nullptr;
};

} // namespace gazer
