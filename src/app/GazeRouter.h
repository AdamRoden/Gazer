#pragma once

#include "core/GazePoint.h"

namespace gazer {

class AssistSession;
class GazeMouseFollow;
class GazeReticle;
class HeadPoseMapper;
class PageSession;
class ComboMouse;
class LookToScroll;
class MagnifierOverlay;
class MouseDwellMove;

/// Ordered gaze consumers. Board/aim policy comes only from AssistSession.
class GazeRouter {
public:
    void setPages(PageSession* pages) { m_pages = pages; }
    void setAssistSession(AssistSession* session) { m_session = session; }
    void setLookToScroll(LookToScroll* lts) { m_lookToScroll = lts; }
    void setComboMouse(ComboMouse* c) { m_comboMouse = c; }
    void setMouseDwellMove(MouseDwellMove* move) { m_mouseDwell = move; }
    void setMagnifier(MagnifierOverlay* mag) { m_magnifier = mag; }
    void setGazeReticle(GazeReticle* r) { m_gazeReticle = r; }
    void setGazeMouseFollow(GazeMouseFollow* f) { m_gazeFollow = f; }
    void setHeadPoseMapper(HeadPoseMapper* m) { m_headPose = m; }

    void dispatch(const GazePoint& point);

private:
    PageSession* m_pages = nullptr;
    AssistSession* m_session = nullptr;
    LookToScroll* m_lookToScroll = nullptr;
    ComboMouse* m_comboMouse = nullptr;
    MouseDwellMove* m_mouseDwell = nullptr;
    MagnifierOverlay* m_magnifier = nullptr;
    GazeReticle* m_gazeReticle = nullptr;
    GazeMouseFollow* m_gazeFollow = nullptr;
    HeadPoseMapper* m_headPose = nullptr;
};

} // namespace gazer
