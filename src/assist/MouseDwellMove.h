#pragma once

#include "assist/GazeDwellTracker.h"
#include "core/GazePoint.h"
#include "layout/InvalidGazeGrace.h"
#include "ui/ProgressVisuals.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPixmap>
#include <QPointF>
#include <QRect>
#include <memory>

namespace gazer {

/// Dwell gaze to warp the OS cursor. Optional two-step static magnify pick.
/// Look↕Scroll placement always uses direct dwell (never mag-pick).
class MouseDwellMove final : public QObject {
    Q_OBJECT

public:
    enum class ArmPurpose {
        CursorMove,           ///< Normal move-to (honors mag-pick setting).
        LookToScrollPlace,    ///< Place scroll origin for LTS (always direct).
        CursorMoveClickLoop,  ///< Same as CursorMove, then left-click and re-arm until off.
        CursorMoveLeftClick,  ///< Move-to, then one left click and disarm.
        CursorMoveRightClick  ///< Move-to, then one right click and disarm.
    };

    explicit MouseDwellMove(QObject* parent = nullptr);
    ~MouseDwellMove() override;

    void setArmed(bool armed, ArmPurpose purpose = ArmPurpose::CursorMove);
    [[nodiscard]] bool isArmed() const { return m_armed; }
    [[nodiscard]] ArmPurpose armPurpose() const { return m_purpose; }
    [[nodiscard]] bool isLookToScrollPlace() const
    {
        return m_armed && m_purpose == ArmPurpose::LookToScrollPlace;
    }
    [[nodiscard]] bool isClickLoop() const
    {
        return m_armed && m_purpose == ArmPurpose::CursorMoveClickLoop;
    }
    void toggle();

    /// Pause dwell + select-timeout (click loop over a board). Does not disarm.
    void setPaused(bool paused);
    [[nodiscard]] bool isPaused() const { return m_paused; }

    /// Ignore gaze until it leaves @p screenRect (activator cell). Then start dwell.
    void gateUntilGazeLeaves(const QRect& screenRect);

    void setDwellMs(int ms);
    void setMagPickDwellMs(int ms);
    void setMagPickStyle(int flags);
    void setMousePickStyle(int flags);
    /// Cancel arm (including click-loop) if no target selected within this many ms. 0 = off.
    void setSelectTimeoutMs(int ms);
    void setStableRadiusPx(int px);
    void setFreezeRadiusPx(int px);
    void setCancelRadiusPx(int px);
    void setProgressVisuals(const ProgressVisuals& visuals);
    void setMagPickEnabled(bool enabled);
    [[nodiscard]] bool isMagPickEnabled() const { return m_magPickEnabled; }
    [[nodiscard]] bool isMagPointPhase() const;
    void setMagPickZoom(double z);
    void setMagPickSourcePx(int px);
    /// When true, static zoom window is centered on the first-dwell point (clamped).
    void setMagPickCenterOnDwell(bool on);
    [[nodiscard]] bool isMagPickCenterOnDwell() const { return m_magPickCenterOnDwell; }

    /// Boards are left by GazeRouter while armed; no overBoard cancel path.
    void onGaze(const GazePoint& point);

signals:
    void armedChanged(bool armed);
    void magPointPhaseChanged(bool active);
    void movedTo(QPoint pos);
    void progressChanged(double progress);

private:
    class CursorOverlay;
    class MagPickOverlay;
    enum class Phase { Idle, Direct, MagRegion, MagPoint };

    void resetDwell();
    void hideUi();
    void setPhase(Phase phase);
    void beginMagPick(const QPoint& center);
    void finishMagPoint(const QPointF& gaze);
    /// After a successful move: click+rearm for click-loop, else disarm.
    void completeMoveCycle(const QPoint& target);
    void markSelectDeadline();
    [[nodiscard]] bool selectTimedOut(qint64 nowMs) const;
    [[nodiscard]] bool useMagPickThisArm() const;
    void applyDwellForPhase();
    void syncPickOverlayStyle();
    [[nodiscard]] int styleForPhase() const;

    bool m_armed = false;
    bool m_paused = false;
    ArmPurpose m_purpose = ArmPurpose::CursorMove;
    int m_moveDwellMs = 700;
    int m_magPickDwellMs = 700;
    int m_magPickStyle = 1;
    int m_mousePickStyle = 1;
    bool m_magPickEnabled = false;
    bool m_magPickCenterOnDwell = true;
    double m_magZoom = 2.5;
    int m_magSourcePx = 220;
    Phase m_phase = Phase::Idle;
    /// 0 = disabled. Otherwise cancel arm if no selection by m_selectDeadlineMs.
    int m_selectTimeoutMs = 5000;
    qint64 m_selectDeadlineMs = -1;
    QRect m_gateRect;

    GazeDwellTracker m_dwell;
    QElapsedTimer m_clock;
    qint64 m_lastSampleMs = -1;
    InvalidGazeGrace m_invalidGrace;
    ProgressVisuals m_progressVisuals;

    QPixmap m_magPixmap;
    QRect m_magSourceRect;
    QRect m_magDisplayRect;
    QPointF m_lastMagGaze;

    std::unique_ptr<CursorOverlay> m_cursor;
    std::unique_ptr<MagPickOverlay> m_magOverlay;
};

} // namespace gazer
