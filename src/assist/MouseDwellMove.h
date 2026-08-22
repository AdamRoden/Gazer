#pragma once

#include "assist/ForesightMemory.h"
#include "assist/GazeDwellTracker.h"
#include "assist/MagLayout.h"
#include "core/GazePoint.h"
#include "layout/InvalidGazeGrace.h"
#include "ui/ProgressVisuals.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPixmap>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <memory>

namespace gazer {

/// Dwell gaze to warp the OS cursor. Optional static magnify pick and Foresight
/// (remember a recent desktop dwell and magnify immediately).
/// Look↕Scroll placement always uses direct dwell (never mag-pick / foresight).
class MouseDwellMove final : public QObject {
    Q_OBJECT

public:
    enum class ArmPurpose {
        CursorMove,           ///< Normal move-to (honors mag-pick setting).
        LookToScrollPlace,    ///< Place scroll origin for LTS (always direct).
        CursorMoveClickLoop,  ///< Same as CursorMove, then left-click and re-arm until off.
        CursorMoveLeftClick,  ///< Move-to, then one left click and disarm.
        CursorMoveRightClick, ///< Move-to, then one right click and disarm.
        CursorMoveMiddleClick ///< Move-to, then one middle click and disarm.
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

    void setPaused(bool paused);
    [[nodiscard]] bool isPaused() const { return m_paused; }

    void gateUntilGazeLeaves(const QRect& screenRect);

    void setDwellMs(int ms);
    void setMagPickDwellMs(int ms);
    void setMagPickStyle(int flags);
    void setMousePickStyle(int flags);
    void setSelectTimeoutMs(int ms);
    void setStableRadiusPx(int px);
    void setFreezeRadiusPx(int px);
    void setCancelRadiusPx(int px);
    void setProgressVisuals(const ProgressVisuals& visuals);
    void setMagPickEnabled(bool enabled);
    [[nodiscard]] bool isMagPickEnabled() const { return m_magPickEnabled; }
    [[nodiscard]] bool isMagPointPhase() const;
    void setPickZoom(double z);
    void setPickWindowPx(int px);
    void setPickWindowRound(bool on);
    [[nodiscard]] bool isPickWindowRound() const { return m_pickWindowRound; }
    void setMagPickCenterOnDwell(bool on);
    [[nodiscard]] bool isMagPickCenterOnDwell() const { return m_magPickCenterOnDwell; }
    void setMagPickFullScreen(bool on);
    [[nodiscard]] bool isMagPickFullScreen() const { return m_magPickFullScreen; }

    void setForesightEnabled(bool enabled);
    [[nodiscard]] bool isForesightEnabled() const { return m_foresight.isEnabled(); }
    void setForesightDwellMs(int ms);
    void setForesightDoubleZoom(bool on);
    [[nodiscard]] bool isForesightDoubleZoom() const { return m_foresightDoubleZoom; }

    void onBackgroundGaze(const GazePoint& point, bool overUi);
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
    void startAimPhase();
    [[nodiscard]] bool beginMagPick(const MagPresentation& spec, bool outsideSelectsNewRegion);
    [[nodiscard]] MagPresentation makePreClickSpec(const QPoint& center) const;
    [[nodiscard]] MagPresentation makeForesightSpec(const QPoint& srcCenter,
                                                    const QPoint& destCenter, int destSide) const;
    [[nodiscard]] int destSideFor(QScreen* screen) const;
    [[nodiscard]] QPoint mapDisplayToSource(const QPointF& gaze) const;
    [[nodiscard]] bool gazeInZoomWindow(const QPointF& gaze) const;
    void finishMagPoint(const QPointF& gaze);
    void placeCursor(const QPoint& target);
    void onGazeInZoom(const QPointF& g, double dtSec);
    void onGazeAim(const QPointF& g, double dtSec);
    void completeMoveCycle(const QPoint& target);
    void markSelectDeadline();
    [[nodiscard]] bool selectTimedOut(qint64 nowMs) const;
    [[nodiscard]] bool useMagPickThisArm() const;
    void applyDwellForPhase();
    void syncPickOverlayStyle();
    [[nodiscard]] int styleForPhase() const;
    static const char* purposeName(ArmPurpose purpose);

    bool m_armed = false;
    bool m_paused = false;
    ArmPurpose m_purpose = ArmPurpose::CursorMove;
    int m_moveDwellMs = 700;
    int m_magPickDwellMs = 700;
    int m_magPickStyle = 1;
    int m_mousePickStyle = 1;
    bool m_magPickEnabled = false;
    bool m_magPickCenterOnDwell = true;
    bool m_magPickFullScreen = false;
    bool m_foresightDoubleZoom = false;
    double m_pickZoom = 4.0;
    int m_pickWindowPx = 880;
    bool m_pickWindowRound = false;
    Phase m_phase = Phase::Idle;
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
    MagPresentation m_mag;
    bool m_outsideSelectsNewRegion = false;
    QPointF m_lastMagGaze;
    bool m_magGazeInside = true;

    ForesightMemory m_foresight;

    std::unique_ptr<CursorOverlay> m_cursor;
    std::unique_ptr<MagPickOverlay> m_magOverlay;
};

} // namespace gazer
