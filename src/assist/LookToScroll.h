#pragma once

#include "assist/ComboMouseHit.h"
#include "assist/LtsIndicator.h"
#include "assist/LtsMenu.h"
#include "assist/LtsScrollMode.h"
#include "core/GazePoint.h"
#include "input/PixelScroller.h"
#include "layout/DwellStateMachine.h"
#include "layout/InvalidGazeGrace.h"
#include "ui/Theme.h"

#include <QColor>
#include <QElapsedTimer>
#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>
#include <memory>

namespace gazer {

class PieOverlay;

inline constexpr double kLtsHubVisualDiameterFrac = 0.05;
inline constexpr double kLtsHubDwellDiameterFrac = 0.08;
/// Gaze must leave the plus this long before it collapses to the overlay hub.
inline constexpr double kLtsPlusDismissGraceSec = 0.18;

/// Circular deadzone around the cursor. Gaze outside scrolls (quadratic ease +
/// accel). Once engaged, speed is at least `kLtsMinEngagedPxPerSec` until gaze
/// is clearly back inside the ring (hysteresis).
/// Dwell the hub (`kLtsHubDwellDiameterFrac` of screen height) to pause and open a
/// ComboMouse-style pie (speed, axis mode, reset, quit; inner ring resumes).
/// Looking away closes that pie and shows the overlay hub with a pause icon; looking
/// at the hub opens the pie again (resume dwell starts on the inner ring).
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
    [[nodiscard]] LtsScrollMode scrollMode() const { return m_scrollMode; }

    void setDeadzonePx(int px);
    void setFalloffPx(int px);
    void setMaxNotchesPerSec(double n);
    /// Extra rate multiplier per second of continuous outside-deadzone gaze.
    void setAccelPerSec(double a);
    void setCenterDwellMs(int ms);
    void setAccent(const QColor& c);
    void setTheme(const ThemeColors& theme) { m_theme = theme; }
    void setRadii(int innerPx, int sharedPx, int outerPx);
    void setAnnulusColors(const QColor& inner, const QColor& outer);
    void setScanGraceMs(int ms);
    void setDwellGraceMs(int ms);
    void setDwellMs(int ms);
    void setDwellSequence(const QVector<int>& ms);
    void setActiveWhenOverBoard(bool allow);
    void setIndicatorStyle(LtsIndicator style);
    void setScrollMode(LtsScrollMode mode);

    void resumeScroll();
    void nudgeMaxSpeed(int dir);
    void cycleScrollMode();
    void requestReset();
    /// Cancel an in-flight Reset place-cursor; resume at the existing origin.
    void cancelOriginPlace();

    /// True while the command pie is up and gaze is on it.
    [[nodiscard]] bool containsGaze(const GazePoint& point) const;

    /// @p pauseInput when true: hide overlay and ignore scroll (over board / full-screen aim).
    void onGaze(const GazePoint& point, bool pauseInput);

signals:
    void enabledChanged(bool enabled);
    void scrollSuspendedChanged(bool suspended);
    void maxNotchesPerSecChanged(double notchesPerSec);
    void scrollModeChanged(LtsScrollMode mode);
    /// Reset on the plus menu: host should arm mouse Move-to to pick a new scroll origin.
    void placeScrollPointRequested();
    void scrolled(int deltaV, int deltaH);

public slots:
    void toggle();

private:
    class RingOverlay;

    void updateOverlay(const QPoint& center, double gazeDist, double dirX, double dirY,
                       bool active, double centerProg);
    void showPausedHub();
    void hideOverlay();
    void hidePlus();
    void pinCursorToOrigin();
    void pauseAtHub();
    void openPlus(bool leaveGate);
    void closePlus();
    void updatePausedMenu(const GazePoint& point);
    void pushPlusOverlay(const ComboMouseHit::Layout& L, ComboMouseHit::Band band,
                         ComboMouseHit::Slice slice, double dwellProg);
    void firePlusAction(LtsMenuAction action);
    [[nodiscard]] ComboMouseHit::Layout plusLayout() const;
    [[nodiscard]] QRectF plusScreenRect() const;
    [[nodiscard]] QPoint originPoint() const;
    [[nodiscard]] double screenHeightPx() const;
    [[nodiscard]] double hubVisualRadiusPx() const;
    [[nodiscard]] double hubDwellRadiusPx() const;
    [[nodiscard]] static QString plusHitId(ComboMouseHit::Band band, ComboMouseHit::Slice slice);

    bool m_enabled = false;
    bool m_allowOverBoard = false;
    bool m_scrollSuspended = false;
    bool m_scrollEngaged = false;
    bool m_plusOpen = false;
    bool m_replacing = false;
    bool m_hasOrigin = false;
    QPoint m_origin;
    int m_deadzonePx = 110;
    int m_falloffPx = 360;
    double m_maxNotchesPerSec = 5.0;
    double m_accelPerSec = 0.45; // +45%/s outside, capped
    double m_accelMax = 3.5;
    int m_centerDwellMs = 650;
    LtsIndicator m_indicatorStyle = LtsIndicator::Filled;
    LtsScrollMode m_scrollMode = LtsScrollMode::Both;
    int m_intervalMs = 16;

    QElapsedTimer m_clock;
    qint64 m_lastTickMs = -1;    // scroll emission gate
    qint64 m_lastSampleMs = -1;  // center-dwell / decay dt (every sample)
    InvalidGazeGrace m_invalidGrace;
    /// Continuous time gaze has been outside the deadzone (for acceleration).
    double m_outsideSec = 0.0;
    double m_centerProgress = 0.0;
    double m_lookAwaySec = 0.0;
    QString m_plusGateId;

    double m_innerPx = ComboMouseHit::kHoleRadiusPx;
    double m_sharedPx = ComboMouseHit::kRingOuterPx;
    double m_outerPx = ComboMouseHit::kPieOuterPx;
    QColor m_innerColor = ComboMouseHit::kDefaultInnerFill;
    QColor m_outerColor = ComboMouseHit::kDefaultOuterFill;
    QColor m_accent;
    ThemeColors m_theme = ThemeColors::darkPreset();
    DwellStateMachine m_plusDwell;
    ComboMouseHit::Layout m_plusLayout;
    ComboMouseHit::Band m_plusBand = ComboMouseHit::Band::Deadzone;
    ComboMouseHit::Slice m_plusSlice = ComboMouseHit::Slice::Right;

    std::unique_ptr<RingOverlay> m_overlay;
    std::unique_ptr<PieOverlay> m_plus;
    PixelScroller m_scroller;
};

} // namespace gazer
