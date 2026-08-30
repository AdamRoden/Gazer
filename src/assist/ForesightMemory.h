#pragma once

#include "assist/GazeDwellTracker.h"
#include "core/GazePoint.h"

#include <QPoint>
#include <QPointF>
#include <QtMath>
#include <optional>

namespace gazer {

/// Remembers a desktop dwell for a short window so Move-to can magnify immediately.
class ForesightMemory {
public:
    static constexpr int kHoldMs = 2000;

    void setEnabled(bool on)
    {
        m_enabled = on;
        if (!on) {
            clear();
            m_dwell.reset();
        }
    }
    [[nodiscard]] bool isEnabled() const { return m_enabled; }

    void setDwellMs(int ms)
    {
        m_dwellMs = qBound(100, ms, 2500);
        m_dwell.setDwellMs(m_dwellMs);
    }

    void clear()
    {
        m_havePoint = false;
        m_holding = false;
        m_storedMs = -1;
        m_point = {};
    }

    void sample(const GazePoint& point, bool overUi, qint64 nowMs)
    {
        if (!m_enabled) {
            return;
        }
        if (overUi) {
            m_dwell.reset();
            m_holding = false;
            m_lastSampleMs = -1;
            return;
        }
        if (!point.valid) {
            return;
        }

        if (m_havePoint && m_storedMs >= 0 && nowMs - m_storedMs > kHoldMs) {
            clear();
        }

        const double dtSec =
            m_lastSampleMs < 0 ? 0.016
                               : qBound(0.004, (nowMs - m_lastSampleMs) / 1000.0, 0.08);
        m_lastSampleMs = nowMs;

        if (m_dwell.sample(QPointF(point.x, point.y), dtSec)) {
            if (!m_holding || !m_havePoint) {
                m_point = QPoint(qRound(m_dwell.commitPos().x()), qRound(m_dwell.commitPos().y()));
                m_havePoint = true;
            }
            m_storedMs = nowMs;
            m_holding = true;
        } else {
            m_holding = false;
        }
    }

    [[nodiscard]] std::optional<QPoint> peek(qint64 nowMs) const
    {
        if (!m_enabled || !m_havePoint) {
            return std::nullopt;
        }
        if (m_storedMs < 0 || nowMs - m_storedMs > kHoldMs) {
            return std::nullopt;
        }
        return m_point;
    }

private:
    GazeDwellTracker m_dwell;
    bool m_enabled = false;
    bool m_havePoint = false;
    bool m_holding = false;
    int m_dwellMs = 400;
    qint64 m_lastSampleMs = -1;
    qint64 m_storedMs = -1;
    QPoint m_point;
};

} // namespace gazer
