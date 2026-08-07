#pragma once

#include "assist/GazeFollowStickiness.h"
#include "core/GazePoint.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPointF>
#include <QWidget>
#include <memory>

namespace gazer {

/// Soft semi-transparent disk at the live gaze point.
/// Position uses magnifier stickiness profile; opacity still tracks speed.
class GazeReticle final : public QObject {
    Q_OBJECT

public:
    explicit GazeReticle(QObject* parent = nullptr);
    ~GazeReticle() override;

    void setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const { return m_enabled; }
    void toggle();
    void setFollowProfile(int profile);
    void onGaze(const GazePoint& point);

signals:
    void enabledChanged(bool enabled);

private:
    class Overlay;
    bool m_enabled = false;
    std::unique_ptr<Overlay> m_overlay;

    GazeFollowStickiness m_stickiness;
    bool m_smoothValid = false;
    QPointF m_smooth;

    QElapsedTimer m_clock;
    qint64 m_lastMs = -1;
    QPointF m_lastPos;
    bool m_havePos = false;
    double m_opacity = 0.25;
};

} // namespace gazer
