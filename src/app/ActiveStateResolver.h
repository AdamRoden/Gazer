#pragma once

#include <QString>

namespace gazer {

class ActionLoopService;
class AppSettings;
class GazeMouseFollow;
class GazeReticle;
class ComboMouse;
class LookToScroll;
class MagnifierOverlay;
class KeyStateManager;
class MouseAssistState;
class MouseDwellMove;
class ComposeUi;
class SettingsUi;
class SpeechEngine;

/// Inputs for resolving layout item `activeState` accent keys.
struct ActiveStateContext {
    const AppSettings* settings = nullptr;
    const LookToScroll* lookToScroll = nullptr;
    const ComboMouse* comboMouse = nullptr;
    const MouseDwellMove* mouseDwellMove = nullptr;
    const MagnifierOverlay* magnifier = nullptr;
    const GazeReticle* gazeReticle = nullptr;
    const GazeMouseFollow* gazeMouseFollow = nullptr;
    const MouseAssistState* mouseAssist = nullptr;
    const KeyStateManager* keyState = nullptr;
    const ActionLoopService* actionLoops = nullptr;
    const SettingsUi* settingsUi = nullptr;
    const SpeechEngine* speechEngine = nullptr;
    const ComposeUi* composeUi = nullptr;
    bool dwellSuspended = false;
};

/// Shared sticky-loop / toggle accent resolver (boards, settings, assist modes).
[[nodiscard]] bool resolveActiveState(const ActiveStateContext& ctx, const QString& key);

} // namespace gazer
