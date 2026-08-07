#pragma once

#include "assist/GazeDwellTracker.h"
#include "core/GazePoint.h"
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
        CursorMoveClickLoop   ///< Same as CursorMove, then left-click and re-arm until off.
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

    void setDwellMs(int ms);
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
    [[nodiscard]] bool useMagPickThisArm() const;

    bool m_armed = false;
    ArmPurpose m_purpose = ArmPurpose::CursorMove;
    bool m_magPickEnabled = false;
    bool m_magPickCenterOnDwell = true;
    double m_magZoom = 2.5;
    int m_magSourcePx = 220;
    Phase m_phase = Phase::Idle;

    GazeDwellTracker m_dwell;
    QElapsedTimer m_clock;
    qint64 m_lastSampleMs = -1;
    /// Grace after a brief invalid gaze sample before hard-resetting progress.
    qint64 m_invalidSinceMs = -1;
    int m_invalidGraceMs = 220;
    ProgressVisuals m_progressVisuals;

    QPixmap m_magPixmap;
    QRect m_magSourceRect;
    QRect m_magDisplayRect;
    QPointF m_lastMagGaze;

    std::unique_ptr<CursorOverlay> m_cursor;
    std::unique_ptr<MagPickOverlay> m_magOverlay;
};

} // namespace gazer
