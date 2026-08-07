#pragma once

#include "core/GazePoint.h"
#include "core/HeadPose.h"
#include "ui/StlMesh.h"
#include "ui/Theme.h"

#include <QWidget>

class QCloseEvent;

namespace gazer {

/// Live head-pose preview: loads resources/models/head.stl and mirrors user motion.
class PreviewWindow final : public QWidget {
    Q_OBJECT

public:
    explicit PreviewWindow(QWidget* parent = nullptr);

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
    void ensureMeshLoaded();
    void paintHead(QPainter& p, const QRect& area);

    GazePoint m_gaze;
    HeadPose m_head;
    QString m_trackerName = QStringLiteral("—");
    ThemeColors m_theme = ThemeColors::darkPreset();

    StlMesh m_mesh;
    bool m_meshTried = false;
    QString m_meshError;
};

} // namespace gazer
