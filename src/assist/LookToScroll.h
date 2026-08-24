#pragma once

#include "assist/LtsIndicator.h"
#include "core/GazePoint.h"
#include "input/PixelScroller.h"

#include <QColor>
#include <QElapsedTimer>
#include <QObject>
#include <QPoint>
#include <QWidget>
#include <memory>

namespace gazer {

inline constexpr double kLtsHubVisualDiameterFrac = 0.05;
inline constexpr double kLtsHubDwellDiameterFrac = 0.08;

/// Circular deadzone around the cursor. Gaze outside scrolls (cubic ease + accel).
/// Dwell the hub (`kLtsHubDwellDiameterFrac` of screen height) to pause and open `lts_menu`.
/// Painted hub diameter is `kLtsHubVisualDiameterFrac` of screen height.
class LookToScroll final : public QObject {
    Q_OBJECT

public:
    explicit LookToScroll(QObject* parent = nullptr);
    ~LookToScroll() override;

    void setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const { return m_enabled; }
    /// Scroll paused while LTS stays armed (hub pause, or re-place after Reset).
    [[nodiscard]] bool isScrollSuspended() const { return m_scrollSuspended; }
    void setScrollSuspended(bool suspended);

    /// Warp the OS cursor here and use it as the deadzone / wheel origin.
    void setScrollOrigin(const QPoint& pos);
    [[nodiscard]] QPoint scrollOrigin() const { return m_origin; }
    [[nodiscard]] bool hasScrollOrigin() const { return m_hasOrigin; }
    [[nodiscard]] int deadzonePx() const { return m_deadzonePx; }
    [[nodiscard]] double maxNotchesPerSec() const { return m_maxNotchesPerSec; }

    void setDeadzonePx(int px);
    void setFalloffPx(int px);
    void setMaxNotchesPerSec(double n);
    /// Extra rate multiplier per second of continuous outside-deadzone gaze.
    void setAccelPerSec(double a);
    void setCenterDwellMs(int ms);
    void setAccent(const QColor& c);
    void setActiveWhenOverBoard(bool allow);
    void setIndicatorStyle(LtsIndicator style);

    void resumeScroll();
    void nudgeMaxSpeed(int dir);
    void requestReset();
    /// Cancel an in-flight Reset place-cursor; resume at the existing origin.
    void cancelOriginPlace();

    /// @p pauseInput when true: hide overlay and ignore scroll (over board / full-screen aim).
    void onGaze(const GazePoint& point, bool pauseInput);

signals:
    void enabledChanged(bool enabled);
    void scrollSuspendedChanged(bool suspended);
    void maxNotchesPerSecChanged(double notchesPerSec);
    /// Pause: host should open `lts_menu` centered on this screen point.
    void menuOpenRequested(QPoint origin);
    void menuCloseRequested();
    /// Reset on the plus menu: host should arm mouse Move-to to pick a new scroll origin.
    void placeScrollPointRequested();
    void scrolled(int deltaV, int deltaH);

public slots:
    void toggle();

private:
    class RingOverlay;

    void updateOverlay(const QPoint& center, double gazeDist, double dirX, double dirY,
                       bool active, double centerProg);
    void hideOverlay();
    void pinCursorToOrigin();
    void pauseAtHub();
    [[nodiscard]] QPoint originPoint() const;
    [[nodiscard]] double screenHeightPx() const;
    [[nodiscard]] double hubVisualRadiusPx() const;
    [[nodiscard]] double hubDwellRadiusPx() const;
    [[nodiscard]] static double easeNearDeadzone(double t);

    bool m_enabled = false;
    bool m_allowOverBoard = false;
    bool m_scrollSuspended = false;
    bool m_replacing = false;
    bool m_hasOrigin = false;
    QPoint m_origin;
    int m_deadzonePx = 110;
    int m_falloffPx = 360;
    double m_maxNotchesPerSec = 5.0;
    double m_accelPerSec = 0.45; // +45%/s outside, capped
    double m_accelMax = 3.5;
    int m_centerDwellMs = 650;
    LtsIndicator m_indicatorStyle = LtsIndicator::Fan;
    int m_intervalMs = 16;

    QElapsedTimer m_clock;
    qint64 m_lastTickMs = -1;    // scroll emission gate
    qint64 m_lastSampleMs = -1;  // center-dwell / decay dt (every sample)
    double m_accumV = 0.0;
    double m_accumH = 0.0;
    /// Continuous time gaze has been outside the deadzone (for acceleration).
    double m_outsideSec = 0.0;
    double m_centerProgress = 0.0;

    std::unique_ptr<RingOverlay> m_overlay;
    PixelScroller m_scroller;
};

} // namespace gazer
