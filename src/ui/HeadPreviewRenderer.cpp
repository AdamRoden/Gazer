#include "ui/HeadPreviewRenderer.h"
#include "ui/PreviewGeometry.h"
#include "utils/Log.h"

#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QMatrix4x4>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QRect>
#include <QOpenGLShaderProgram>
#include <QScreen>
#include <QSurfaceFormat>
#include <QVector>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace gazer {

using PreviewGeom::Vec3;
using PreviewGeom::applyLivePose;
using PreviewGeom::appendEyes;
using PreviewGeom::appendShadow;
using PreviewGeom::emitTri;
using PreviewGeom::kEyeRadiusMax;
using PreviewGeom::kEyeRadiusMin;
using PreviewGeom::kEyeRadiusScale;
using PreviewGeom::kEyeYNudge;
using PreviewGeom::kEyeZRecess;
using PreviewGeom::kFragCompat;
using PreviewGeom::kFragCore;
using PreviewGeom::kVertCompat;
using PreviewGeom::kVertCore;
using PreviewGeom::headMeshTz;
using PreviewGeom::kVertStrideBytes;
using PreviewGeom::kVertStrideFloats;
using PreviewGeom::livePoseMatrix;

HeadPreviewRenderer::HeadPreviewRenderer() = default;

HeadPreviewRenderer::~HeadPreviewRenderer()
{
    if (m_ctx && m_surface) {
        m_ctx->makeCurrent(m_surface);
        m_meshVbo.destroy();
        m_dynVbo.destroy();
        delete m_prog;
        m_prog = nullptr;
        m_ctx->doneCurrent();
    }
    delete m_ctx;
    delete m_surface;
}

void HeadPreviewRenderer::setGaze(const GazePoint& g)
{
    m_gaze = g;
    updateEyeLookFromGaze();
}

bool HeadPreviewRenderer::ensureContext()
{
    if (m_glReady && m_ctx && m_surface) {
        return m_ctx->makeCurrent(m_surface);
    }
    QSurfaceFormat fmt;
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    fmt.setRenderableType(QSurfaceFormat::OpenGL);

    m_surface = new QOffscreenSurface();
    m_surface->setFormat(fmt);
    m_surface->create();
    if (!m_surface->isValid()) {
        m_glError = QStringLiteral("Offscreen surface failed");
        GAZER_WARN << m_glError;
        return false;
    }
    m_ctx = new QOpenGLContext();
    m_ctx->setFormat(fmt);
    if (!m_ctx->create() || !m_ctx->makeCurrent(m_surface)) {
        m_glError = QStringLiteral("Offscreen GL context failed");
        GAZER_WARN << m_glError;
        return false;
    }
    initializeOpenGLFunctions();
    m_glReady = buildProgram();
    if (!m_meshVbo.create() || !m_dynVbo.create()) {
        m_glReady = false;
        m_glError = QStringLiteral("Could not create GL buffers");
        GAZER_WARN << m_glError;
        return false;
    }
    m_meshVbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_dynVbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    m_meshUploaded = false;
    uploadMeshVbo();
    return m_glReady;
}

void HeadPreviewRenderer::ensureMeshLoaded()
{
    if (m_meshTried) {
        return;
    }
    m_meshTried = true;
    const QStringList candidates = {
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("resources/models/head.obj")),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../resources/models/head.obj")),
        QStringLiteral("resources/models/head.obj"),
    };
    QString err;
    for (const QString& path : candidates) {
        if (!StlMesh::loadFromFile(path, m_mesh, &err)) {
            continue;
        }
        for (StlMesh::Tri& t : m_mesh.tris) {
            auto flipY = [](QVector3D v) { return QVector3D(v.x(), -v.y(), v.z()); };
            const QVector3D a = flipY(t.a);
            const QVector3D b = flipY(t.b);
            const QVector3D c = flipY(t.c);
            t.a = a;
            t.b = c;
            t.c = b;
        }
        m_mesh.symmetrizeLeftFromRight();
        estimateEyeSockets();
        GAZER_INFO << "Head mesh (in-page):" << path << "tris" << m_mesh.tris.size();
        return;
    }
    m_meshError = err.isEmpty() ? QStringLiteral("head.obj not found") : err;
    GAZER_WARN << "Head mesh load failed:" << m_meshError;
}

void HeadPreviewRenderer::estimateEyeSockets()
{
    if (m_mesh.isEmpty()) {
        return;
    }
    auto meanZInDisk = [&](float ex, float ey, float er) -> float {
        double sum = 0.0;
        int n = 0;
        const float er2 = er * er;
        for (const StlMesh::Tri& t : m_mesh.tris) {
            const QVector3D c = (t.a + t.b + t.c) / 3.0f;
            const float dx = c.x() - ex;
            const float dy = c.y() - ey;
            if (dx * dx + dy * dy <= er2 && c.z() > -0.15f) {
                sum += c.z();
                ++n;
            }
        }
        return n > 8 ? float(sum / n) : 999.f;
    };
    float bestOffL = 0.22f, bestLY = 0.24f, bestLZ = 999.f;
    float bestOffR = 0.22f, bestRY = 0.24f, bestRZ = 999.f;
    for (float off = 0.12f; off <= 0.40f; off += 0.02f) {
        for (float y = 0.06f; y <= 0.40f; y += 0.02f) {
            const float zl = meanZInDisk(-off, y, 0.09f);
            const float zr = meanZInDisk(off, y, 0.09f);
            if (zl < bestLZ) {
                bestLZ = zl;
                bestOffL = off;
                bestLY = y;
            }
            if (zr < bestRZ) {
                bestRZ = zr;
                bestOffR = off;
                bestRY = y;
            }
        }
    }
    if (bestLZ >= 50.f || bestRZ >= 50.f) {
        m_eyeLeft = QVector3D(-0.22f, 0.26f, -0.02f);
        m_eyeRight = QVector3D(0.22f, 0.26f, -0.02f);
        m_eyeRadius = 0.17f;
        return;
    }
    const float halfSep = 0.5f * (bestOffL + bestOffR);
    const float y = 0.5f * (bestLY + bestRY) + kEyeYNudge;
    const float zL = meanZInDisk(-halfSep, y, 0.09f);
    const float zR = meanZInDisk(halfSep, y, 0.09f);
    const float z = 0.5f * ((zL < 50.f ? zL : bestLZ) + (zR < 50.f ? zR : bestRZ)) - kEyeZRecess;
    m_eyeLeft = QVector3D(-halfSep, y, z);
    m_eyeRight = QVector3D(halfSep, y, z);
    m_eyeRadius = qBound(kEyeRadiusMin, halfSep * kEyeRadiusScale, kEyeRadiusMax);
}

void HeadPreviewRenderer::updateEyeLookFromGaze()
{
    double targetX = 0.0;
    double targetY = 0.0;
    double targetOpen = 0.12;
    if (m_gaze.valid) {
        QRect desk;
        for (QScreen* s : QGuiApplication::screens()) {
            desk = desk.united(s->geometry());
        }
        if (desk.isEmpty()) {
            desk = QRect(0, 0, 1920, 1080);
        }
        const double nx = (m_gaze.x - desk.center().x()) / qMax(1.0, desk.width() * 0.5);
        const double ny = (m_gaze.y - desk.center().y()) / qMax(1.0, desk.height() * 0.5);
        targetX = qBound(-1.15, nx, 1.15);
        targetY = qBound(-1.15, -ny, 1.15);
        targetOpen = 1.0;
    }
    constexpr double kLook = 0.42;
    constexpr double kLid = 0.55;
    m_lookX += (targetX - m_lookX) * kLook;
    m_lookY += (targetY - m_lookY) * kLook;
    m_eyeOpen += (targetOpen - m_eyeOpen) * kLid;
}

bool HeadPreviewRenderer::buildProgram()
{
    auto tryLink = [this](const char* vs, const char* fs) -> bool {
        auto* prog = new QOpenGLShaderProgram();
        if (!prog->addShaderFromSourceCode(QOpenGLShader::Vertex, vs)
            || !prog->addShaderFromSourceCode(QOpenGLShader::Fragment, fs)) {
            m_glError = prog->log();
            delete prog;
            return false;
        }
        prog->bindAttributeLocation(QStringLiteral("aPos"), 0);
        prog->bindAttributeLocation(QStringLiteral("aNrm"), 1);
        prog->bindAttributeLocation(QStringLiteral("aBase"), 2);
        if (!prog->link()) {
            m_glError = prog->log();
            delete prog;
            return false;
        }
        delete m_prog;
        m_prog = prog;
        m_glError.clear();
        return true;
    };
    if (tryLink(kVertCompat, kFragCompat) || tryLink(kVertCore, kFragCore)) {
        return true;
    }
    GAZER_WARN << "In-page head preview shader failed:" << m_glError;
    return false;
}

void HeadPreviewRenderer::uploadMeshVbo()
{
    if (!m_glReady || m_meshUploaded || m_mesh.isEmpty() || !m_meshVbo.isCreated()) {
        return;
    }
    QVector<float> verts;
    verts.reserve(m_mesh.tris.size() * kVertStrideFloats * 3);
    constexpr float br = 175.f / 255.f, bg = 178.f / 255.f, bb = 188.f / 255.f;
    for (const StlMesh::Tri& t : m_mesh.tris) {
        emitTri(verts, Vec3(t.a), Vec3(t.b), Vec3(t.c), br, bg, bb);
    }
    m_meshVertexCount = verts.size() / kVertStrideFloats;
    m_meshVbo.bind();
    m_meshVbo.allocate(verts.constData(), verts.size() * int(sizeof(float)));
    m_meshVbo.release();
    m_meshUploaded = true;
}

void HeadPreviewRenderer::bindVertexLayout()
{
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, kVertStrideBytes, nullptr);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, kVertStrideBytes,
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, kVertStrideBytes,
                          reinterpret_cast<void*>(6 * sizeof(float)));
}

void HeadPreviewRenderer::drawHeadGl(const QSize& viewport, const QRect& area)
{
    const double yaw = m_head.rotationValid ? m_head.yaw : 0.0;
    const double pitch = m_head.rotationValid ? m_head.pitch : 0.0;
    const double roll = m_head.rotationValid ? m_head.roll : 0.0;
    const double tx = m_head.positionValid ? m_head.x * 0.04 : 0.0;
    const double ty = m_head.positionValid ? m_head.y * 0.04 : 0.0;
    const double tz =
        m_head.positionValid ? headMeshTz(m_head.z, m_relativePosition) : 0.0;

    const float scale = float(qMin(area.width(), area.height()) * 0.30);
    const float focal = 3.4f;
    const QVector2D origin(float(area.center().x()),
                           float(area.center().y() + area.height() * 0.04));
    const QMatrix4x4 pose = livePoseMatrix(yaw, pitch, roll, tx, ty, tz);

    m_prog->bind();
    m_prog->setUniformValue("uOrigin", origin);
    m_prog->setUniformValue("uViewport", QVector2D(float(viewport.width()), float(viewport.height())));
    m_prog->setUniformValue("uScale", scale);
    m_prog->setUniformValue("uFocal", focal);
    m_prog->setUniformValue("uLightDir", QVector3D(-0.35f, 0.5f, 0.85f).normalized());
    m_prog->setUniformValue("uFillDir", QVector3D(0.55f, 0.15f, 0.25f).normalized());

    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_prog->setUniformValue("uPose", QMatrix4x4());
    m_prog->setUniformValue("uLit", 0.f);
    m_prog->setUniformValue("uAlpha", 75.f / 255.f);

    QVector<float> dyn;
    dyn.reserve(32 * 3 * kVertStrideFloats + 4000);
    appendShadow(dyn);
    const int shadowVerts = dyn.size() / kVertStrideFloats;
    m_dynVbo.bind();
    m_dynVbo.allocate(dyn.constData(), dyn.size() * int(sizeof(float)));
    bindVertexLayout();
    glDrawArrays(GL_TRIANGLES, 0, shadowVerts);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    m_prog->setUniformValue("uPose", pose);
    m_prog->setUniformValue("uLit", 1.f);
    m_prog->setUniformValue("uAlpha", 1.f);

    if (m_meshVertexCount > 0) {
        m_meshVbo.bind();
        bindVertexLayout();
        glDrawArrays(GL_TRIANGLES, 0, m_meshVertexCount);
    }

    dyn.clear();
    appendEyes(dyn, m_eyeLeft, m_eyeRight, m_eyeRadius, m_lookX, m_lookY, m_eyeOpen, yaw, pitch,
               roll);
    const int eyeVerts = dyn.size() / kVertStrideFloats;
    if (eyeVerts > 0) {
        m_dynVbo.bind();
        m_dynVbo.allocate(dyn.constData(), dyn.size() * int(sizeof(float)));
        bindVertexLayout();
        glDrawArrays(GL_TRIANGLES, 0, eyeVerts);
    }

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    m_dynVbo.release();
    m_meshVbo.release();
    m_prog->release();
}

int HeadPreviewRenderer::triCount()
{
    ensureMeshLoaded();
    return m_mesh.tris.size();
}

QImage HeadPreviewRenderer::render(const QSize& logical, qreal dpr)
{
    ensureMeshLoaded();
    if (logical.width() < 8 || logical.height() < 8) {
        return {};
    }
    if (!ensureContext()) {
        return {};
    }
    uploadMeshVbo();
    const int w = qMax(8, int(std::lround(logical.width() * dpr)));
    const int h = qMax(8, int(std::lround(logical.height() * dpr)));
    QOpenGLFramebufferObjectFormat ff;
    ff.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    QOpenGLFramebufferObject fbo(QSize(w, h), ff);
    if (!fbo.isValid()) {
        return {};
    }
    fbo.bind();
    const QColor bg = m_theme.bgMain.isValid() ? m_theme.bgMain : QColor(14, 14, 16);
    glViewport(0, 0, w, h);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glClearColor(bg.redF(), bg.greenF(), bg.blueF(), 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!m_mesh.isEmpty() && m_glReady && m_prog) {
        drawHeadGl(QSize(w, h), QRect(0, 0, w, h));
    }
    fbo.release();
    QImage img = fbo.toImage();
    if (dpr > 1.01) {
        img.setDevicePixelRatio(dpr);
    }
    return img;
}

} // namespace gazer
