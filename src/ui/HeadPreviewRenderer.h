#pragma once

#include "core/GazePoint.h"
#include "core/HeadPose.h"
#include "ui/StlMesh.h"
#include "ui/Theme.h"

#include <QImage>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QSize>
#include <QString>
#include <QVector3D>

class QOffscreenSurface;
class QOpenGLContext;
class QOpenGLFramebufferObject;
class QOpenGLShaderProgram;

namespace gazer {

/// Offscreen copy of the tray head-preview mesh (same OBJ, shaders, eyes).
class HeadPreviewRenderer final : protected QOpenGLFunctions {
public:
    HeadPreviewRenderer();
    ~HeadPreviewRenderer();

    void setTheme(const ThemeColors& theme) { m_theme = theme; }
    void setGaze(const GazePoint& g);
    void setPose(const HeadPose& p)
    {
        m_head = p;
        m_relativePosition = false;
    }
    void setRelativePose(const HeadPose& p)
    {
        m_head = p;
        m_relativePosition = true;
    }

    [[nodiscard]] QImage render(const QSize& logical, qreal dpr = 1.0);
    [[nodiscard]] QString meshError() const { return m_meshError; }
    [[nodiscard]] int triCount();

private:
    bool ensureContext();
    void ensureMeshLoaded();
    void estimateEyeSockets();
    void updateEyeLookFromGaze();
    bool buildProgram();
    void uploadMeshVbo();
    void bindVertexLayout();
    void drawHeadGl(const QSize& viewport, const QRect& area);

    GazePoint m_gaze;
    HeadPose m_head;
    bool m_relativePosition = false;
    ThemeColors m_theme = ThemeColors::darkPreset();

    StlMesh m_mesh;
    bool m_meshTried = false;
    QString m_meshError;

    QVector3D m_eyeLeft{-0.22f, 0.26f, -0.02f};
    QVector3D m_eyeRight{0.22f, 0.26f, -0.02f};
    float m_eyeRadius = 0.17f;
    double m_lookX = 0.0;
    double m_lookY = 0.0;
    double m_eyeOpen = 1.0;

    QOffscreenSurface* m_surface = nullptr;
    QOpenGLContext* m_ctx = nullptr;
    QOpenGLShaderProgram* m_prog = nullptr;
    QOpenGLBuffer m_meshVbo{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer m_dynVbo{QOpenGLBuffer::VertexBuffer};
    int m_meshVertexCount = 0;
    bool m_glReady = false;
    bool m_meshUploaded = false;
    QString m_glError;
};

} // namespace gazer
