#pragma once

#include "assist/GazeFollowStickiness.h"
#include "core/GazePoint.h"

#include <QPixmap>
#include <QPointF>
#include <QTimer>
#include <QWidget>

namespace gazer {

/// Floating zoom lens following gaze (screen capture + scale).
class MagnifierOverlay final : public QWidget {
    Q_OBJECT

public:
    explicit MagnifierOverlay(QWidget* parent = nullptr);

    void setEnabledLens(bool enabled);
    [[nodiscard]] bool isEnabledLens() const { return m_enabled; }

    void setZoom(double factor);
    void setLensSize(int px);
    void setFollowProfile(int profile);
    [[nodiscard]] double zoom() const { return m_zoom; }
    [[nodiscard]] int lensSize() const { return m_lensSize; }
    [[nodiscard]] int followProfile() const { return m_followProfile; }

    void onGaze(const GazePoint& point);

public slots:
    void toggle();

signals:
    void enabledChanged(bool enabled);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void refreshCapture(const QPoint& screenCenter);
    void reposition(const QPoint& screenCenter);

    bool m_enabled = false;
    double m_zoom = 2.0;
    int m_lensSize = 440;
    int m_sourceRadius = 110;
    QPixmap m_capture;
    QPoint m_pendingCenter;
    bool m_pendingCenterValid = false;
    QTimer m_captureTimer;

    bool m_smoothValid = false;
    QPointF m_smoothCenter;
    QPoint m_lastPlaced;

    GazeFollowStickiness m_stickiness;
    double m_placeEpsilonPx = 0.75;
    int m_followProfile = 1;
};

} // namespace gazer
