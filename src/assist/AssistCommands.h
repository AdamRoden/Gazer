#pragma once

#include <QString>
#include <functional>

namespace gazer {

class ActionLoopService;
class AppSettings;
class AssistSession;
class CommandRegistry;
class GazeMouseFollow;
class GazeReticle;
class ComboMouse;
class LookToScroll;
class MagnifierOverlay;
class MouseAssistState;
class MouseDwellMove;
class PageSession;

/// Heap-owned command wiring context (must outlive all command/signal handlers).
struct AssistCommandContext {
    CommandRegistry* commands = nullptr;
    AssistSession* session = nullptr;
    PageSession* pages = nullptr;
    LookToScroll* lookToScroll = nullptr;
    ComboMouse* comboMouse = nullptr;
    MouseDwellMove* mouseDwellMove = nullptr;
    MagnifierOverlay* magnifier = nullptr;
    GazeReticle* gazeReticle = nullptr;
    GazeMouseFollow* gazeMouseFollow = nullptr;
    MouseAssistState* mouseAssist = nullptr;
    ActionLoopService* actionLoops = nullptr;
    AppSettings* settings = nullptr;
    std::function<void(bool persist)> applySettings;
    std::function<void()> refreshActiveIndicators;
    std::function<void(const QString&)> notifyStatus;
    std::function<void(bool)> setDwellSuspended;
    std::function<bool()> isDwellSuspended;
};

void registerAssistCommands(AssistCommandContext& ctx);

} // namespace gazer
