#pragma once

#include "core/GazePoint.h"
#include "ui/ProgressVisuals.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPointF>
#include <QWidget>
#include <memory>

namespace gazer {

/// After arming, dwell gaze (outside boards) warps the OS cursor to that point.
///
/// Reticle follows gaze smoothly for small moves while the progress ring fills.
/// Larger drift freezes progress; still larger drift reverses progress — movement
/// is never hard-snapped/reset the way a pure "stable radius" dwell does.
class MouseDwellMove final : public QObject {
    Q_OBJECT

public:
    explicit MouseDwellMove(QObject* parent = nullptr);
    ~MouseDwellMove() override;

    void setArmed(bool armed);
    [[nodiscard]] bool isArmed() const { return m_armed; }
    void toggle();

    void setDwellMs(int ms);
    /// Radius where progress still fills (reticle may drift slowly inside this).
    void setStableRadiusPx(int px);
    /// Beyond this, progress freezes (reticle still follows smoothly).
    void setFreezeRadiusPx(int px);
    /// Beyond this, progress reverses toward zero.
    void setCancelRadiusPx(int px);
    void setProgressVisuals(const ProgressVisuals& visuals);

    /// overBoard: ignore dwell on Gazer boards so keys don't warp the cursor.
    void onGaze(const GazePoint& point, bool overBoard);

signals:
    void armedChanged(bool armed);
    void movedTo(QPoint pos);
    void progressChanged(double progress); // 0..1

private:
    class ReticleOverlay;
    void resetDwell();
    void updateReticle(const QPointF& screen, double progress);

    bool m_armed = false;
    int m_dwellMs = 700;
    /// Progress still accrues; commit point slowly tracks gaze.
    int m_stableRadiusPx = 56;
    /// Progress holds; reticle keeps following.
    int m_freezeRadiusPx = 100;
    /// Progress decays; at 0 commit re-homes to current gaze.
    int m_cancelRadiusPx = 160;
    /// EMA for reticle position (smooth small moves).
    double m_followAlpha = 0.28;
    /// How fast commit center tracks gaze while inside stable radius.
    double m_commitTrackAlpha = 0.08;
    /// Progress reverse rate scale (1 = full dwell time to drain).
    double m_reverseScale = 1.35;

    QElapsedTimer m_clock;
    qint64 m_lastSampleMs = -1;
    bool m_tracking = false;
    QPointF m_smoothPos;   // reticle / fire target (smooth)
    QPointF m_commitPos;   // dwell "center" for distance checks
    double m_progress = 0.0;
    ProgressVisuals m_progressVisuals;

    std::unique_ptr<ReticleOverlay> m_reticle;
};

} // namespace gazer
