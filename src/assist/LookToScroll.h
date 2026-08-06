#pragma once

#include "core/GazePoint.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPoint>
#include <QWidget>
#include <memory>

namespace gazer {

/// Circular deadzone around the mouse cursor. Gaze outside scrolls with speed
/// proportional to distance past the deadzone — soft near the ring (cubic ease),
/// continuous high-resolution wheel deltas (smooth, not jumpy whole notches).
class LookToScroll final : public QObject {
    Q_OBJECT

public:
    explicit LookToScroll(QObject* parent = nullptr);
    ~LookToScroll() override;

    void setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const { return m_enabled; }

    void setDeadzonePx(int px);
    void setFalloffPx(int px);
    /// Peak scroll rate in notches/second at full falloff (smooth via sub-notch deltas).
    void setMaxNotchesPerSec(double n);
    void setActiveWhenOverBoard(bool allow);

    void onGaze(const GazePoint& point, bool overBoard);

signals:
    void enabledChanged(bool enabled);
    void scrolled(int deltaV, int deltaH); // raw wheel units (WHEEL_DELTA=120)

public slots:
    void toggle();

private:
    class RingOverlay;

    void updateOverlay(const QPoint& center, double gazeDist, bool active);
    void hideOverlay();
    /// Map normalized distance past deadzone [0,1] → speed factor [0,1]. Soft near ring.
    [[nodiscard]] static double easeNearDeadzone(double t);

    bool m_enabled = false;
    bool m_allowOverBoard = false;
    /// Larger deadzone = less accidental scroll while aiming near the cursor.
    int m_deadzonePx = 110;
    /// Distance from deadzone edge to full speed.
    int m_falloffPx = 360;
    /// Peak vertical/horizontal rate at t=1 (notches per second).
    double m_maxNotchesPerSec = 6.0;
    /// Sample / inject interval (ms). Lower = smoother continuous scroll.
    int m_intervalMs = 16;

    QElapsedTimer m_clock;
    qint64 m_lastTickMs = -1;
    /// Accumulated fractional wheel units (WHEEL_DELTA = 120 per notch).
    double m_accumV = 0.0;
    double m_accumH = 0.0;

    std::unique_ptr<RingOverlay> m_overlay;
};

} // namespace gazer
