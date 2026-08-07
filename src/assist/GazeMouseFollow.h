#pragma once

#include "assist/GazeFollowStickiness.h"
#include "core/GazePoint.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPoint>
#include <QPointF>

namespace gazer {

/// OS cursor follows gaze using the same stickiness profile as the magnifier.
class GazeMouseFollow final : public QObject {
    Q_OBJECT

public:
    explicit GazeMouseFollow(QObject* parent = nullptr);

    void setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const { return m_enabled; }
    void toggle();
    void setFollowProfile(int profile);
    void setSmoothAlpha(double a); // legacy fixed alpha (ignored when stickiness used)
    void onGaze(const GazePoint& point, bool pauseInput = false);

signals:
    void enabledChanged(bool enabled);

private:
    bool m_enabled = false;
    bool m_hasPos = false;
    GazeFollowStickiness m_stickiness;
    QPointF m_smooth;
    QElapsedTimer m_clock;
    qint64 m_lastInjectMs = -1;
    QPoint m_lastInjected;
};

} // namespace gazer
