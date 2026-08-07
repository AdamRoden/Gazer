#pragma once

#include "core/GazePoint.h"

#include <QElapsedTimer>
#include <QObject>
#include <QPointF>
#include <QWidget>
#include <memory>

namespace gazer {

/// Assistant tool: soft semi-transparent disk at the live gaze point (150px).
/// Opacity eases toward 5% when gaze holds still and up to 50% when gaze moves fast.
class GazeReticle final : public QObject {
    Q_OBJECT

public:
    explicit GazeReticle(QObject* parent = nullptr);
    ~GazeReticle() override;

    void setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const { return m_enabled; }
    void toggle();
    void onGaze(const GazePoint& point);

signals:
    void enabledChanged(bool enabled);

private:
    class Overlay;
    bool m_enabled = false;
    std::unique_ptr<Overlay> m_overlay;

    QElapsedTimer m_clock;
    qint64 m_lastMs = -1;
    QPointF m_lastPos;
    bool m_havePos = false;
    double m_opacity = 0.25; // smoothed alpha in 0.05–0.50
};

} // namespace gazer
