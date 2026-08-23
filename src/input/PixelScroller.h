#pragma once

#include <QString>
#include <memory>

namespace gazer {

/// Pixel-level scroll of the window under the cursor (LTS origin).
///
/// WM_MOUSEWHEEL is quantized to whole lines by most Win32 / Office / editor
/// apps. This tries, in order:
///   1. Native pixel APIs (ListView LVM_SCROLL, pixel-sized scrollbars)
///   2. UI Automation ScrollPattern (fractional percent)
///   3. High-resolution mouse wheel (browsers and other high-res-aware apps)
class PixelScroller {
public:
    PixelScroller();
    ~PixelScroller();
    PixelScroller(const PixelScroller&) = delete;
    PixelScroller& operator=(const PixelScroller&) = delete;

    /// Logical pixels treated as one mouse-wheel notch (speed setting + wheel fallback).
    static constexpr double kPixelsPerNotch = 80.0;

    /// @p dx / @p dy match mouse wheel: positive dy = away from the user (up),
    /// positive dx = right. Values are Qt logical pixels.
    [[nodiscard]] bool scrollBy(int dx, int dy, QString* error = nullptr);

    /// Release an in-progress thumb-drag (deadzone / pause / disable).
    void lift();

    /// Lift plus drop cached UIA / hwnd state (origin moved or LTS off).
    void reset();

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace gazer
