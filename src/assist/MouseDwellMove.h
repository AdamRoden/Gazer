#pragma once

#include "assist/ForesightMemory.h"
#include "assist/GazeDwellTracker.h"
#include "assist/MagLayout.h"
#include "core/GazePoint.h"
#include "layout/InvalidGazeGrace.h"
#include "ui/ProgressVisuals.h"

#include <QElapsedTimer>
#include <QObject>
#include <QtGlobal>
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
        ComboMousePlace,      ///< Place ComboMouse origin (always direct).
        CursorMoveClickLoop,  ///< Same as CursorMove, then left-click and re-arm until off.
        CursorMoveLeftClick,  ///< Move-to, then one left click and disarm.
        CursorMoveRightClick, ///< Move-to, then one right click and disarm.
        CursorMoveMiddleClick ///< Move-to, then one middle click and disarm.
    };

    explicit MouseDwellMove(QObject* parent = nullptr);
    ~MouseDwellMove() override;

    struct ArmZoom {
        enum class Kind { Settings, Direct, Level, Foresight, ForesightBonus };
        Kind kind = Kind::Settings;
        double level = 0.0;

        ArmZoom() = default;
        ArmZoom(Kind k, double n) : kind(k), level(n) {}

        static ArmZoom settings() { return {}; }
        static ArmZoom direct() { return {Kind::Direct, 0.0}; }
        static ArmZoom at(double n) { return {Kind::Level, n}; }
        static ArmZoom foresight() { return {Kind::Foresight, 0.0}; }
        static ArmZoom foresightBonus() { return {Kind::ForesightBonus, 0.0}; }

        [[nodiscard]] bool operator==(const ArmZoom& o) const
        {
            return kind == o.kind && level == o.level;
        }

        [[nodiscard]] bool useMagPick(bool settingsEnabled) const
        {
            switch (kind) {
            case Kind::Direct:
                return false;
            case Kind::Level:
                return level > 0.0;
            case Kind::Foresight:
            case Kind::ForesightBonus:
                return true;
            case Kind::Settings:
                break;
            }
            return settingsEnabled;
        }

        [[nodiscard]] bool wantsForesight(bool settingsEnabled) const
        {
            switch (kind) {
            case Kind::Foresight:
            case Kind::ForesightBonus:
                return true;
            case Kind::Settings:
                return settingsEnabled;
            case Kind::Direct:
            case Kind::Level:
                break;
            }
            return false;
        }

        [[nodiscard]] bool wantsBonus(bool settingsBonus) const
        {
            switch (kind) {
            case Kind::ForesightBonus:
                return true;
            case Kind::Settings:
                return settingsBonus;
            case Kind::Foresight:
            case Kind::Direct:
            case Kind::Level:
                break;
            }
            return false;
        }

        [[nodiscard]] double resolvedLevel(double fallback) const
        {
            return level > 0.0 ? level : fallback;
        }
    };

    void setArmed(bool armed, ArmPurpose purpose = ArmPurpose::CursorMove);
    void setArmed(bool armed, ArmPurpose purpose, ArmZoom zoom);
    void toggleArmed(ArmPurpose purpose);
    void toggleArmed(ArmPurpose purpose, ArmZoom zoom);
    [[nodiscard]] bool isArmed() const { return m_armed; }
    [[nodiscard]] ArmPurpose armPurpose() const { return m_purpose; }
    [[nodiscard]] bool isLookToScrollPlace() const
    {
        return m_armed && m_purpose == ArmPurpose::LookToScrollPlace;
    }
    [[nodiscard]] bool isComboMousePlace() const
    {
        return m_armed && m_purpose == ArmPurpose::ComboMousePlace;
    }
    [[nodiscard]] bool isClickLoop() const
    {
        return m_armed && m_purpose == ArmPurpose::CursorMoveClickLoop;
    }
    void setPaused(bool paused);
    [[nodiscard]] bool isPaused() const { return m_paused; }

    void gateUntilGazeLeaves(const QRect& screenRect);
    /// Hold this long after gaze leaves the gate before pick dwell begins (blink grace).
    void setGateGraceMs(int ms);

    void setDwellMs(int ms);
    void setMagPickDwellMs(int ms);
    void setMagPickStyle(int flags);
    void setMousePickStyle(int flags);
    /// Timeout plus the current phase's dwell. 0 when timeout is off.
    [[nodiscard]] static int selectDeadlineBudgetMs(int timeoutMs, int phaseDwellMs)
    {
        if (timeoutMs <= 0) {
            return 0;
        }
        return timeoutMs + qMax(0, phaseDwellMs);
    }
    void setSelectTimeoutMs(int ms);
    void setStableRadiusPx(int px);
    void setFreezeRadiusPx(int px);
    void setCancelRadiusPx(int px);
    void setProgressVisuals(const ProgressVisuals& visuals);
    void setMagPickEnabled(bool enabled);
    [[nodiscard]] bool isMagPickEnabled() const { return m_magPickEnabled; }
    [[nodiscard]] bool isMagPointPhase() const;
    /// True while mag-pick is up and gaze is inside the zoom window.
    [[nodiscard]] bool containsGaze(const GazePoint& point) const;
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
    void setForesightHoldMs(int ms);
    void setForesightSecondZoom(bool on);
    [[nodiscard]] bool isForesightSecondZoom() const { return m_ForesightSecondZoom; }

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
    [[nodiscard]] bool armWantsForesight() const;
    [[nodiscard]] bool armWantsBonusZoom() const;
    void applyDwellForPhase();
    void syncPickOverlayStyle();
    [[nodiscard]] int styleForPhase() const;
    static const char* purposeName(ArmPurpose purpose);

    bool m_armed = false;
    bool m_paused = false;
    ArmPurpose m_purpose = ArmPurpose::CursorMove;
    ArmZoom m_armZoom;
    int m_moveDwellMs = 700;
    int m_magPickDwellMs = 700;
    int m_magPickStyle = 1;
    int m_mousePickStyle = 1;
    bool m_magPickEnabled = false;
    bool m_magPickCenterOnDwell = true;
    bool m_magPickFullScreen = false;
    bool m_ForesightSecondZoom = false;
    double m_pickZoom = 4.0;
    int m_pickWindowPx = 880;
    bool m_pickWindowRound = false;
    Phase m_phase = Phase::Idle;
    int m_selectTimeoutMs = 5000;
    qint64 m_selectDeadlineMs = -1;
    QRect m_gateRect;
    int m_gateGraceMs = 180;
    qint64 m_gateLeftMs = -1;

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
    bool m_magGazeInside = true;

    ForesightMemory m_foresight;

    std::unique_ptr<CursorOverlay> m_cursor;
    std::unique_ptr<MagPickOverlay> m_magOverlay;
};

} // namespace gazer
