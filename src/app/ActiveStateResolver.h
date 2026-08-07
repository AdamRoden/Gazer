#pragma once

#include <QString>

namespace gazer {

class ActionLoopService;
class AppSettings;
class GazeMouseFollow;
class GazeReticle;
class LayoutInstanceManager;
class LookToScroll;
class MagnifierOverlay;
class MouseAssistState;
class MouseDwellMove;

/// Inputs for resolving layout item `activeState` accent keys.
struct ActiveStateContext {
    const AppSettings* settings = nullptr;
    const LayoutInstanceManager* instances = nullptr;
    const LookToScroll* lookToScroll = nullptr;
    const MouseDwellMove* mouseDwellMove = nullptr;
    const MagnifierOverlay* magnifier = nullptr;
    const GazeReticle* gazeReticle = nullptr;
    const GazeMouseFollow* gazeMouseFollow = nullptr;
    const MouseAssistState* mouseAssist = nullptr;
    const ActionLoopService* actionLoops = nullptr;
};

/// Shared sticky-loop / toggle accent resolver (boards, settings, assist modes).
[[nodiscard]] bool resolveActiveState(const ActiveStateContext& ctx, const QString& key);

} // namespace gazer
