#pragma once

#include "assist/ComboMouseHit.h"
#include "assist/LookToMap.h"
#include "assist/LtsMenu.h"
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
#include <functional>
#include <memory>

namespace gazer {

class InputService;
class LookToOverlay;
class PieOverlay;

inline constexpr double kLtsHubVisualDiameterFrac = 0.05;
inline constexpr double kLtsHubDwellDiameterFrac = 0.08;
/// Gaze must leave the plus this long before it collapses to the overlay hub.
inline constexpr double kLtsPlusDismissGraceSec = 0.18;

/// Analog gaze disk: look away from an origin to drive scroll, mouse, or a
/// stick. Four instances can run at once (`LookToMaps`). Circular deadzone,
/// 0–100% ramp, 100% plateau, optional outer deadzone. Optional hub pie for
/// speed, place, axis, and quit.
class LookToScroll final : public QObject {
    Q_OBJECT

public:
    explicit LookToScroll(QObject* parent = nullptr);
    ~LookToScroll() override;

    void setDest(LookToDest dest);
    [[nodiscard]] LookToDest dest() const { return m_dest; }

    void setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const { return m_enabled; }
    /// Output paused while the map stays armed (hub pause, or re-place after Reset).
    [[nodiscard]] bool isScrollSuspended() const { return m_scrollSuspended; }
    void setScrollSuspended(bool suspended);

    void setConfig(const LookToMapSettings& cfg);
    [[nodiscard]] LookToMapSettings config() const;

    /// Warp the OS cursor here and use it as the deadzone / wheel origin.
    void setScrollOrigin(const QPoint& pos);
    [[nodiscard]] QPoint scrollOrigin() const { return m_origin; }
    [[nodiscard]] bool hasScrollOrigin() const { return m_hasOrigin; }
    [[nodiscard]] int deadzonePx() const { return m_cfg.deadzonePx; }
    [[nodiscard]] double maxNotchesPerSec() const { return m_cfg.maxSpeed; }
    [[nodiscard]] LtsScrollMode scrollMode() const { return m_cfg.axisMode; }
    [[nodiscard]] bool hubEnabled() const { return m_cfg.hubEnabled; }
    [[nodiscard]] bool isPieOpen() const { return m_plusOpen; }

    void setAccent(const QColor& c);
    void setTheme(const ThemeColors& theme) { m_theme = theme; }
    void setRadii(int innerPx, int sharedPx, int outerPx);
    void setAnnulusColors(const QColor& inner, const QColor& outer);
    void setScanGraceMs(int ms);
    void setDwellGraceMs(int ms);
    void setDwellSequence(const QVector<int>& ms);
    void setScrollMode(LtsScrollMode mode);
    void setPinCursorEnabled(bool on) { m_pinCursor = on; }
    void setInput(InputService* input) { m_input = input; }
    using NotifyFn = std::function<void(const QString&)>;
    void setNotifyFn(NotifyFn fn) { m_notify = std::move(fn); }

    void resumeScroll();
    void nudgeMaxSpeed(int dir);
    void cycleScrollMode();
    void requestReset();
    /// Cancel an in-flight Reset place-cursor; resume at the existing origin.
    void cancelOriginPlace();

    /// True while the command pie is up and gaze is on it.
    [[nodiscard]] bool containsGaze(const GazePoint& point) const;

    /// @p pauseInput: over board / full-screen aim. Invalid-gaze grace before lift.
    void onGaze(const GazePoint& point, bool pauseInput);

signals:
    void enabledChanged(bool enabled);
    void scrollSuspendedChanged(bool suspended);
    void maxNotchesPerSecChanged(double notchesPerSec);
    void scrollModeChanged(LtsScrollMode mode);
    /// Reset on the plus menu: host should arm mouse Move-to to pick a new origin.
    void placeScrollPointRequested();
    void scrolled(int deltaV, int deltaH);

public slots:
    void toggle();

private:
    void updateOverlay(const QPoint& center, const QPointF& gaze, double gain, double centerProg,
                       bool paused);
    void showPausedHub();
    void hideOverlay();
    void hidePlus();
    void pinCursorToOrigin();
    void pauseAtHub();
    void openPlus();
    void closePlus();
    void updatePausedMenu(const GazePoint& point);
    void pushPlusOverlay(const ComboMouseHit::Layout& L, ComboMouseHit::Band band,
                         ComboMouseHit::Slice slice, double dwellProg);
    void firePlusAction(LtsMenuAction action);
    void applyOutput(double nx, double ny, double gain, double sampleDt);
    void liftOutput();
    void zeroJoystick();
    void warnJoystick(const QString& err);
    [[nodiscard]] ComboMouseHit::Layout plusLayout() const;
    [[nodiscard]] QRectF plusScreenRect() const;
    [[nodiscard]] QPoint originPoint() const;
    [[nodiscard]] double screenHeightPx() const;
    [[nodiscard]] double hubVisualRadiusPx() const;
    [[nodiscard]] double hubDwellRadiusPx() const;
    [[nodiscard]] static QString plusHitId(ComboMouseHit::Band band, ComboMouseHit::Slice slice);
    [[nodiscard]] QString hubSpeedLabel() const;

    LookToDest m_dest = LookToDest::Scroll;
    LookToMapSettings m_cfg = defaultLookToMapSettings(LookToDest::Scroll);
    bool m_enabled = false;
    bool m_scrollSuspended = false;
    bool m_scrollEngaged = false;
    bool m_plusOpen = false;
    bool m_replacing = false;
    bool m_hasOrigin = false;
    bool m_pinCursor = false;
    QPoint m_origin;

    QElapsedTimer m_clock;
    qint64 m_lastSampleMs = -1;  // center-dwell / output dt (every sample)
    InvalidGazeGrace m_invalidGrace;
    /// Accel time per axis. Resets when that axis offset goes to ~0.
    double m_accelSecV = 0.0;
    double m_accelSecH = 0.0;
    double m_centerProgress = 0.0;
    double m_lookAwaySec = 0.0;
    double m_mouseRemX = 0.0;
    double m_mouseRemY = 0.0;
    bool m_driveJoyX = false;
    bool m_driveJoyY = false;

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

    InputService* m_input = nullptr;
    NotifyFn m_notify;
    bool m_joyWarned = false;
    std::unique_ptr<LookToOverlay> m_overlay;
    std::unique_ptr<PieOverlay> m_plus;
    PixelScroller m_scroller;
};

} // namespace gazer
