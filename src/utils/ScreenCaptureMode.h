#pragma once

namespace gazer {

/// What a screenshot of the desktop includes.
/// All — boards and assist overlays (for documentation).
/// Pages — boards only; look-to, the reticle, and the lens stay off the image.
/// None — boards and those overlays are omitted.
enum class ScreenCaptureMode : int { All = 0, Pages = 1, None = 2 };

/// @p overlay is true for assist tool windows and false for the page host.
[[nodiscard]] inline bool excludeFromScreenCapture(ScreenCaptureMode mode, bool overlay)
{
    switch (mode) {
    case ScreenCaptureMode::All:
        return false;
    case ScreenCaptureMode::None:
        return true;
    case ScreenCaptureMode::Pages:
        return overlay;
    }
    return overlay;
}

} // namespace gazer
