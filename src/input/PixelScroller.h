#pragma once

#include <QString>
#include <memory>

namespace gazer {

/// Pixel-level scroll of the window under the cursor (LTS origin).
///
/// Classifies the HWND once, then drains a single logical-pixel remainder:
///   HighResWheel — Chromium / Firefox (subpixel WM_MOUSEWHEEL)
///   Scintilla    — SCI_LINESCROLL / SCI_SETXOFFSET (Notepad++)
///   ListView     — LVM_SCROLL
///   Fallback     — pixel scrollbar, then UIA if 1 px is ≥ 0.5% of range,
///                  then wheel
class PixelScroller {
public:
    PixelScroller();
    ~PixelScroller();
    PixelScroller(const PixelScroller&) = delete;
    PixelScroller& operator=(const PixelScroller&) = delete;

    /// Logical pixels treated as one mouse-wheel notch (speed setting + wheel fallback).
    static constexpr double kPixelsPerNotch = 80.0;

    /// @p dx / @p dy match mouse wheel: positive dy = away from the user (up),
    /// positive dx = right. Values are Qt logical pixels (fractional OK).
    [[nodiscard]] bool scrollBy(double dx, double dy, QString* error = nullptr);

    /// Release an in-progress thumb-drag (deadzone / pause / disable).
    void lift();

    /// Lift plus drop cached UIA / hwnd state (origin moved or LTS off).
    void reset();

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace gazer
