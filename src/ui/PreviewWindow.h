#pragma once

#include "core/GazePoint.h"

#include <QWidget>

class QCloseEvent;

namespace gazer {

/// Live gaze crosshair preview (debug / calibration aid).
///
/// Normal window (not click-through overlay). Closing the window hides it so
/// the tray can re-show it; app lifetime is owned by the tray / Application.
class PreviewWindow final : public QWidget {
    Q_OBJECT

public:
    explicit PreviewWindow(QWidget* parent = nullptr);

    void setTrackerName(const QString& name);

public slots:
    void onGazeUpdated(const gazer::GazePoint& point);
    void showAndRaise();

protected:
    void paintEvent(QPaintEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    GazePoint m_gaze;
    QString m_trackerName = QStringLiteral("—");
};

} // namespace gazer
