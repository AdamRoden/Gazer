#pragma once

#include "core/GazePoint.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPoint>
#include <QPointF>

namespace gazer {

/// Assistant tool: OS cursor smoothly follows gaze (paused over boards).
class GazeMouseFollow final : public QObject {
    Q_OBJECT

public:
    explicit GazeMouseFollow(QObject* parent = nullptr);

    void setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const { return m_enabled; }
    void toggle();
    void setSmoothAlpha(double a);
    /// @p pauseInput when true (over board / full-screen aim): do not move cursor.
    void onGaze(const GazePoint& point, bool pauseInput);

signals:
    void enabledChanged(bool enabled);

private:
    bool m_enabled = false;
    bool m_hasPos = false;
    double m_alpha = 0.35;
    QPointF m_smooth;
    QElapsedTimer m_clock;
    qint64 m_lastInjectMs = -1;
    QPoint m_lastInjected;
};

} // namespace gazer

