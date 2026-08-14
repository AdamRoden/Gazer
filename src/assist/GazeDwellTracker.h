#pragma once

#include <QPointF>
#include <QtMath>

namespace gazer {

/// Shared progressive gaze dwell with smooth follow + stable/freeze/cancel radii.
/// Used by mouse-move and mag-pick (and any future dwell-to-confirm tools).
class GazeDwellTracker {
public:
    void setDwellMs(int ms) { m_dwellMs = qMax(50, ms); }
    void setStableRadiusPx(int px)
    {
        m_stableRadiusPx = qMax(8, px);
        m_freezeRadiusPx = qMax(m_stableRadiusPx, m_freezeRadiusPx);
        m_cancelRadiusPx = qMax(m_freezeRadiusPx, m_cancelRadiusPx);
    }
    void setFreezeRadiusPx(int px)
    {
        m_freezeRadiusPx = qMax(m_stableRadiusPx, px);
        m_cancelRadiusPx = qMax(m_freezeRadiusPx, m_cancelRadiusPx);
    }
    void setCancelRadiusPx(int px) { m_cancelRadiusPx = qMax(m_freezeRadiusPx, px); }
    void setFollowAlpha(double a) { m_followAlpha = qBound(0.05, a, 1.0); }
    void setCommitTrackAlpha(double a) { m_commitTrackAlpha = qBound(0.0, a, 1.0); }
    void setReverseScale(double s) { m_reverseScale = qMax(0.5, s); }

    void reset()
    {
        m_tracking = false;
        m_progress = 0.0;
        m_smoothPos = {};
        m_commitPos = {};
    }

    [[nodiscard]] double progress() const { return m_progress; }
    [[nodiscard]] QPointF smoothPos() const { return m_smoothPos; }
    [[nodiscard]] QPointF commitPos() const { return m_commitPos; }
    [[nodiscard]] bool isTracking() const { return m_tracking; }

    /// Returns true when progress reaches 1.0 this sample.
    bool sample(const QPointF& gaze, double dtSec)
    {
        if (!m_tracking) {
            m_tracking = true;
            m_smoothPos = gaze;
            m_commitPos = gaze;
            m_progress = 0.0;
            return false;
        }

        m_smoothPos.setX(m_smoothPos.x() * (1.0 - m_followAlpha) + gaze.x() * m_followAlpha);
        m_smoothPos.setY(m_smoothPos.y() * (1.0 - m_followAlpha) + gaze.y() * m_followAlpha);

        const double dx = m_smoothPos.x() - m_commitPos.x();
        const double dy = m_smoothPos.y() - m_commitPos.y();
        const double dist = qSqrt(dx * dx + dy * dy);

        if (dist <= double(m_stableRadiusPx)) {
            m_commitPos.setX(m_commitPos.x() * (1.0 - m_commitTrackAlpha)
                             + m_smoothPos.x() * m_commitTrackAlpha);
            m_commitPos.setY(m_commitPos.y() * (1.0 - m_commitTrackAlpha)
                             + m_smoothPos.y() * m_commitTrackAlpha);
            m_progress = qBound(0.0, m_progress + dtSec * 1000.0 / double(m_dwellMs), 1.0);
        } else {
            // Drifted off the commit point. Do not freeze incomplete progress — the
            // follow indicator has already moved, so a held ring feels stalled.
            // Bleed progress and walk commit toward the live point so a new dwell
            // can start as soon as the user settles.
            const double bleed =
                dist <= double(m_freezeRadiusPx)
                    ? m_reverseScale
                    : dist <= double(m_cancelRadiusPx) ? m_reverseScale * 1.8
                                                       : m_reverseScale * 2.6;
            m_progress = qBound(0.0, m_progress - dtSec * 1000.0 / double(m_dwellMs) * bleed, 1.0);
            const double pull = dist <= double(m_freezeRadiusPx) ? 0.28 : 0.42;
            m_commitPos.setX(m_commitPos.x() * (1.0 - pull) + m_smoothPos.x() * pull);
            m_commitPos.setY(m_commitPos.y() * (1.0 - pull) + m_smoothPos.y() * pull);
            if (m_progress <= 0.001) {
                reanchor();
            }
        }
        return m_progress >= 1.0;
    }

private:
    void reanchor()
    {
        m_commitPos = m_smoothPos;
        m_progress = 0.0;
    }

    int m_dwellMs = 700;
    // Generous defaults: Tobii noise + natural drift often exceeds ~50px.
    int m_stableRadiusPx = 72;
    int m_freezeRadiusPx = 100;
    int m_cancelRadiusPx = 180;
    double m_followAlpha = 0.36;
    double m_commitTrackAlpha = 0.16;
    double m_reverseScale = 1.6;
    bool m_tracking = false;
    double m_progress = 0.0;
    QPointF m_smoothPos;
    QPointF m_commitPos;
};

} // namespace gazer
