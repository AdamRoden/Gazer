#pragma once

#include <QString>
#include <QtGlobal>
#include <memory>

namespace gazer {

/// Pixel-level scroll of the window under the cursor (LTS origin).
///
/// Classifies the HWND once, then drains a single logical-pixel remainder:
///   HighResWheel — Chromium / Firefox / IE / WebView2 (ancestor walk)
///   Scintilla    — SCI_LINESCROLL / SCI_SETXOFFSET (Notepad++)
///   ListView     — LVM_SCROLL
///   Fallback     — pixel scrollbar, then UIA if 1 px is ≥ 0.5% of range, then wheel
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

/// Logical px → WM_MOUSEWHEEL units (WHEEL_DELTA = 120 / notch). High-res path
/// emits whole leftover units (OptiKey-style); remainder stays in @p remPx.
inline constexpr int kWheelUnitsPerNotch = 120;

/// Chromium / Firefox / IE class names. `Intermediate D3D Window` is not a match.
[[nodiscard]] inline bool classLooksLikeHighResWheel(QStringView cls)
{
    return cls.startsWith(QLatin1String("Chrome_"), Qt::CaseInsensitive)
        || cls.startsWith(QLatin1String("Mozilla"), Qt::CaseInsensitive)
        || cls.compare(QLatin1String("IEFrame"), Qt::CaseInsensitive) == 0
        || cls.compare(QLatin1String("Internet Explorer_Server"), Qt::CaseInsensitive) == 0;
}

[[nodiscard]] inline int takeWheelUnits(double& remPx, int minAbsUnits = 1)
{
    const int step = qMax(1, minAbsUnits);
    const double k = double(kWheelUnitsPerNotch) / PixelScroller::kPixelsPerNotch;
    const double units = remPx * k;
    if (qAbs(units) < double(step)) {
        return 0;
    }
    int v = int(units);
    v -= v % step;
    if (v == 0) {
        return 0;
    }
    remPx = (units - double(v)) / k;
    return v;
}

} // namespace gazer
