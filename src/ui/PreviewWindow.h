#pragma once

#include "core/GazePoint.h"
#include "core/HeadPose.h"
#include "ui/StlMesh.h"
#include "ui/Theme.h"

#include <QVector3D>
#include <QWidget>

class QCloseEvent;

namespace gazer {

/// Live head-pose preview: mask mesh + gaze-driven eyes.
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
    void estimateEyeSockets();
    void updateEyeLookFromGaze();
    void paintHead(QPainter& p, const QRect& area);

    GazePoint m_gaze;
    HeadPose m_head;
    QString m_trackerName = QStringLiteral("—");
    ThemeColors m_theme = ThemeColors::darkPreset();

    StlMesh m_mesh;
    bool m_meshTried = false;
    QString m_meshError;

    /// Head-local eye centers: symmetric about X=0 (mask bounds center), same Y/Z.
    QVector3D m_eyeLeft{-0.22f, 0.26f, -0.02f};
    QVector3D m_eyeRight{0.22f, 0.26f, -0.02f};
    float m_eyeRadius = 0.17f;

    /// Smoothed screen-gaze look (−1…1, right/up positive) and lid open (0…1).
    double m_lookX = 0.0;
    double m_lookY = 0.0;
    double m_eyeOpen = 1.0;
};

} // namespace gazer
