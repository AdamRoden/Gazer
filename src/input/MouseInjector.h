#pragma once

#include <QString>

namespace gazer {

/// Windows SendInput mouse injection.
class MouseInjector {
public:
    /// button: "left" | "right" | "middle"
    [[nodiscard]] static bool click(const QString& button, QString* error = nullptr);

    /// Two rapid left/right/middle clicks (double-click).
    [[nodiscard]] static bool doubleClick(const QString& button, QString* error = nullptr);

    /// Button down only (for drag start / hold).
    [[nodiscard]] static bool buttonDown(const QString& button, QString* error = nullptr);

    /// Button up only (for drag end / release).
    [[nodiscard]] static bool buttonUp(const QString& button, QString* error = nullptr);

    /// Relative move in pixels.
    [[nodiscard]] static bool moveBy(int dx, int dy, QString* error = nullptr);

    /// Absolute move in virtual-desktop pixels (multi-monitor aware).
    [[nodiscard]] static bool moveTo(int screenX, int screenY, QString* error = nullptr);

    /// Vertical wheel; positive = away from user (up). One notch = WHEEL_DELTA (120).
    [[nodiscard]] static bool scroll(int notches, QString* error = nullptr);

    /// Horizontal wheel; positive = right. One notch = WHEEL_DELTA (120).
    [[nodiscard]] static bool scrollHorizontal(int notches, QString* error = nullptr);

    /// High-resolution vertical wheel in raw mouseData units (WHEEL_DELTA = 120 per notch).
    /// Punches the board host so a key-sourced gesture reaches the window under the cursor.
    [[nodiscard]] static bool scrollDelta(int wheelDelta, QString* error = nullptr);

    /// High-resolution horizontal wheel in raw mouseData units.
    [[nodiscard]] static bool scrollHorizontalDelta(int wheelDelta, QString* error = nullptr);

    /// SendInput wheel in raw mouseData units. No OverlayInputPassThrough —
    /// the dummy MOUSEEVENTF_MOVE used to flush hit-testing cancels Chromium
    /// scroll latching on nested pages.
    [[nodiscard]] static bool scrollWheelRaw(int horizontal, int vertical, QString* error = nullptr);
};

} // namespace gazer
