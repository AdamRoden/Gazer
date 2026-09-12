#pragma once

#include "core/GazePoint.h"
#include "core/HeadPose.h"
#include "ui/Theme.h"

#include <QWidget>

class QCloseEvent;
class QPaintEvent;

namespace gazer {

class HeadPreviewRenderer;

/// Tray head-pose preview. GL lives in HeadPreviewRenderer; this paints the HUD.
class PreviewWindow final : public QWidget {
    Q_OBJECT

public:
    explicit PreviewWindow(QWidget* parent = nullptr);
    ~PreviewWindow() override = default;

    void setRenderer(HeadPreviewRenderer* renderer) { m_gl = renderer; }
    void setTrackerName(const QString& name);
    void setTheme(const ThemeColors& theme);

public slots:
    void onGazeUpdated(const gazer::GazePoint& point);
    void onHeadPoseUpdated(const gazer::HeadPose& pose);
    void showAndRaise();

protected:
    void paintEvent(QPaintEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void paintOverlay(QPainter& p, const QRect& headArea);

    HeadPreviewRenderer* m_gl = nullptr;
    GazePoint m_gaze;
    HeadPose m_head;
    QString m_trackerName = QStringLiteral("—");
    ThemeColors m_theme = ThemeColors::darkPreset();
};

} // namespace gazer
