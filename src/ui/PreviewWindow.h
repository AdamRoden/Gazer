#pragma once

#include "core/GazePoint.h"
#include "core/HeadPose.h"
#include "ui/StlMesh.h"
#include "ui/Theme.h"

#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QVector3D>

class QCloseEvent;
class QOpenGLShaderProgram;
class QPainter;

namespace gazer {

/// Live head-pose preview: mask mesh + gaze-driven eyes.
/// Mesh is drawn on the GPU; QPainter is only used for the HUD/gizmo overlay.
class PreviewWindow final : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit PreviewWindow(QWidget* parent = nullptr);
    ~PreviewWindow() override;

    void setTrackerName(const QString& name);
    void setTheme(const ThemeColors& theme);

public slots:
    void onGazeUpdated(const gazer::GazePoint& point);
    void onHeadPoseUpdated(const gazer::HeadPose& pose);
    void showAndRaise();

protected:
    void initializeGL() override;
    void paintGL() override;
    void closeEvent(QCloseEvent* event) override;

private:
    void ensureMeshLoaded();
    void estimateEyeSockets();
    void updateEyeLookFromGaze();
    bool buildProgram();
    void uploadMeshVbo();
    void bindVertexLayout();
    void drawHeadGl(const QRect& area);
    void paintOverlay(QPainter& p, const QRect& headArea);

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

    QOpenGLShaderProgram* m_prog = nullptr;
    QOpenGLBuffer m_meshVbo{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer m_dynVbo{QOpenGLBuffer::VertexBuffer};
    int m_meshVertexCount = 0;
    bool m_glReady = false;
    bool m_meshUploaded = false;
    QString m_glError;
};

} // namespace gazer
