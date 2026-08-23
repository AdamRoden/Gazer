#include "ui/PreviewWindow.h"

#include "utils/Log.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QMatrix4x4>
#include <QOpenGLShaderProgram>
#include <QPainter>
#include <QScreen>
#include <QSurfaceFormat>
#include <QVector>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace gazer {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kHudH = 48;
constexpr int kVertStrideFloats = 9;
constexpr int kVertStrideBytes = kVertStrideFloats * int(sizeof(float));

struct Vec3 {
    double x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(double X, double Y, double Z) : x(X), y(Y), z(Z) {}
    explicit Vec3(const QVector3D& v) : x(v.x()), y(v.y()), z(v.z()) {}
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
    [[nodiscard]] double dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    [[nodiscard]] Vec3 cross(const Vec3& o) const
    {
        return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
    }
    [[nodiscard]] Vec3 normalized() const
    {
        const double len = std::sqrt(x * x + y * y + z * z);
        return len < 1e-12 ? Vec3{} : *this * (1.0 / len);
    }
};

/// Apply head pose: pitch uses −pitch so nod matches screen-up; lateral X is mirrored.
Vec3 applyLivePose(const Vec3& v, double yawDeg, double pitchDeg, double rollDeg, double tx,
                   double ty, double tz)
{
    const double y = qDegreesToRadians(yawDeg);
    const double p = qDegreesToRadians(-pitchDeg);
    const double r = qDegreesToRadians(rollDeg);
    const double cy = std::cos(y), sy = std::sin(y);
    const double cp = std::cos(p), sp = std::sin(p);
    const double cr = std::cos(r), sr = std::sin(r);

    Vec3 a{v.x, v.y * cp - v.z * sp, v.y * sp + v.z * cp};
    Vec3 b{a.x * cy + a.z * sy, a.y, -a.x * sy + a.z * cy};
    Vec3 c{b.x * cr - b.y * sr, b.x * sr + b.y * cr, b.z};
    c.x += -tx;
    c.y += ty;
    c.z += tz;
    return c;
}

/// Inverse of applyLivePose rotation only (no translation).
Vec3 inverseLiveRot(const Vec3& v, double yawDeg, double pitchDeg, double rollDeg)
{
    const double y = qDegreesToRadians(yawDeg);
    const double p = qDegreesToRadians(-pitchDeg);
    const double r = qDegreesToRadians(rollDeg);
    const double cy = std::cos(y), sy = std::sin(y);
    const double cp = std::cos(p), sp = std::sin(p);
    const double cr = std::cos(r), sr = std::sin(r);
    Vec3 b{v.x * cr + v.y * sr, -v.x * sr + v.y * cr, v.z};
    Vec3 a{b.x * cy - b.z * sy, b.y, b.x * sy + b.z * cy};
    return {a.x, a.y * cp + a.z * sp, -a.y * sp + a.z * cp};
}

QMatrix4x4 livePoseMatrix(double yawDeg, double pitchDeg, double rollDeg, double tx, double ty,
                          double tz)
{
    QMatrix4x4 m;
    m.setToIdentity();
    m.translate(float(-tx), float(ty), float(tz));
    m.rotate(float(rollDeg), 0.f, 0.f, 1.f);
    m.rotate(float(yawDeg), 0.f, 1.f, 0.f);
    m.rotate(float(-pitchDeg), 1.f, 0.f, 0.f);
    return m;
}

void emitVert(QVector<float>& out, const Vec3& p, const Vec3& n, float r, float g, float b)
{
    out.push_back(float(p.x));
    out.push_back(float(p.y));
    out.push_back(float(p.z));
    out.push_back(float(n.x));
    out.push_back(float(n.y));
    out.push_back(float(n.z));
    out.push_back(r);
    out.push_back(g);
    out.push_back(b);
}

void emitTri(QVector<float>& out, const Vec3& a, const Vec3& b, const Vec3& c, float r, float g,
             float bch)
{
    const Vec3 n = (b - a).cross(c - a).normalized();
    emitVert(out, a, n, r, g, bch);
    emitVert(out, b, n, r, g, bch);
    emitVert(out, c, n, r, g, bch);
}

// --- Eye geometry (head-local units after mesh normalize) ---
constexpr float kEyeYNudge = 0.0f;
constexpr float kEyeZRecess = 0.28f;
constexpr float kEyeRadiusScale = 1.0f;
constexpr float kEyeRadiusMin = 0.15f;
constexpr float kEyeRadiusMax = 0.3f;
/// Iris disc radius as a fraction of eyeball radius (plane∩sphere = that circle).
constexpr double kIrisRadiusOfEye = 0.3;
/// Pupil radius as a fraction of iris radius.
constexpr double kPupilOfIris = 0.35;
constexpr double kLidClosed = 0.16;
constexpr double kGazeGain = 0.5;

// Compatibility (GL 2.1 / ES 2.0) — no #version; Qt injects one when needed.
constexpr const char* kVertCompat = R"(
attribute vec3 aPos;
attribute vec3 aNrm;
attribute vec3 aBase;
uniform mat4 uPose;
uniform vec2 uOrigin;
uniform vec2 uViewport;
uniform float uScale;
uniform float uFocal;
varying vec3 vNrm;
varying vec3 vBase;
void main() {
    vec4 world = uPose * vec4(aPos, 1.0);
    float d = max(0.4, (uFocal - world.z) / uFocal);
    vec2 screen = vec2(uOrigin.x + world.x * uScale / d,
                       uOrigin.y - world.y * uScale / d);
    vec2 ndc = vec2(screen.x / uViewport.x * 2.0 - 1.0,
                    1.0 - screen.y / uViewport.y * 2.0);
    float z = clamp(0.35 - world.z * 0.25, -0.95, 0.95);
    gl_Position = vec4(ndc, z, 1.0);
    vNrm = mat3(uPose) * aNrm;
    vBase = aBase;
}
)";

constexpr const char* kFragCompat = R"(
#ifdef GL_ES
precision mediump float;
#endif
varying vec3 vNrm;
varying vec3 vBase;
uniform vec3 uLightDir;
uniform vec3 uFillDir;
uniform float uLit;
uniform float uAlpha;
void main() {
    if (uLit < 0.5) {
        gl_FragColor = vec4(vBase, uAlpha);
        return;
    }
    vec3 n = normalize(vNrm);
    if (n.z < -0.02) discard;
    float key = max(0.0, dot(n, uLightDir));
    float fill = max(0.0, dot(n, uFillDir)) * 0.32;
    float ndl = 0.20 + 0.68 * key + fill;
    vec3 c = vBase * ndl + vec3(18.0, 18.0, 22.0) / 255.0 * key;
    gl_FragColor = vec4(c, uAlpha);
}
)";

constexpr const char* kVertCore = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNrm;
layout(location = 2) in vec3 aBase;
uniform mat4 uPose;
uniform vec2 uOrigin;
uniform vec2 uViewport;
uniform float uScale;
uniform float uFocal;
out vec3 vNrm;
out vec3 vBase;
void main() {
    vec4 world = uPose * vec4(aPos, 1.0);
    float d = max(0.4, (uFocal - world.z) / uFocal);
    vec2 screen = vec2(uOrigin.x + world.x * uScale / d,
                       uOrigin.y - world.y * uScale / d);
    vec2 ndc = vec2(screen.x / uViewport.x * 2.0 - 1.0,
                    1.0 - screen.y / uViewport.y * 2.0);
    float z = clamp(0.35 - world.z * 0.25, -0.95, 0.95);
    gl_Position = vec4(ndc, z, 1.0);
    vNrm = mat3(uPose) * aNrm;
    vBase = aBase;
}
)";

constexpr const char* kFragCore = R"(
#version 330 core
in vec3 vNrm;
in vec3 vBase;
uniform vec3 uLightDir;
uniform vec3 uFillDir;
uniform float uLit;
uniform float uAlpha;
out vec4 fragColor;
void main() {
    if (uLit < 0.5) {
        fragColor = vec4(vBase, uAlpha);
        return;
    }
    vec3 n = normalize(vNrm);
    if (n.z < -0.02) discard;
    float key = max(0.0, dot(n, uLightDir));
    float fill = max(0.0, dot(n, uFillDir)) * 0.32;
    float ndl = 0.20 + 0.68 * key + fill;
    vec3 c = vBase * ndl + vec3(18.0, 18.0, 22.0) / 255.0 * key;
    fragColor = vec4(c, uAlpha);
}
)";

void appendShadow(QVector<float>& out)
{
    constexpr int kSegs = 32;
    const Vec3 center{0.0, -0.95, 0.0};
    const double rx = 0.48;
    const double ry = 0.11;
    Vec3 prev{rx, -0.95, 0.0};
    for (int i = 1; i <= kSegs; ++i) {
        const double a = (2.0 * kPi * i) / kSegs;
        const Vec3 rim{rx * std::cos(a), -0.95 - ry * std::sin(a), 0.0};
        emitTri(out, center, prev, rim, 0.f, 0.f, 0.f);
        prev = rim;
    }
}

void appendEyes(QVector<float>& out, const QVector3D& eyeLeft, const QVector3D& eyeRight,
                float eyeRadius, double lookX, double lookY, double eyeOpen, double yaw,
                double pitch, double roll)
{
    const double open = qBound(0.0, eyeOpen, 1.0);
    lookX = qBound(-1.0, lookX, 1.0);
    lookY = qBound(-1.0, lookY, 1.0);
    const double R = double(eyeRadius);

    const Vec3 worldLook = Vec3{lookX * kGazeGain, lookY * kGazeGain, 1.0}.normalized();
    Vec3 lookDir = inverseLiveRot(worldLook, yaw, pitch, roll).normalized();
    if (lookDir.dot(lookDir) < 1e-8) {
        lookDir = Vec3{0, 0, 1};
    }

    constexpr int kStacks = 14;
    constexpr int kSlices = 28;
    constexpr int kDiscSegs = 32;

    constexpr float scleraR = 236.f / 255.f, scleraG = 238.f / 255.f, scleraB = 242.f / 255.f;
    constexpr float irisR = 40.f / 255.f, irisG = 95.f / 255.f, irisB = 150.f / 255.f;
    constexpr float pupilR = 8.f / 255.f, pupilG = 8.f / 255.f, pupilB = 12.f / 255.f;

    auto appendEye = [&](const QVector3D& centerQ) {
        const Vec3 C(centerQ);
        const Vec3 f = lookDir;

        Vec3 rAxis = Vec3{0, 1, 0}.cross(f);
        if (rAxis.dot(rAxis) < 1e-8) {
            rAxis = Vec3{1, 0, 0}.cross(f);
        }
        rAxis = rAxis.normalized();
        const Vec3 uAxis = f.cross(rAxis).normalized();

        const double irisRad = R * kIrisRadiusOfEye;
        const double dCut = std::sqrt(std::max(0.0, R * R - irisRad * irisRad));
        const double thetaCut = std::acos(qBound(-1.0, dCut / R, 1.0));

        auto sphPt = [&](double theta, double phi) -> Vec3 {
            const double st = std::sin(theta);
            const double ct = std::cos(theta);
            const double cp = std::cos(phi);
            const double sp = std::sin(phi);
            return C + (rAxis * (R * st * cp) + uAxis * (R * st * sp) + f * (R * ct));
        };

        auto pushSclera = [&](const Vec3& a, const Vec3& b, const Vec3& c) {
            emitTri(out, a, b, c, scleraR, scleraG, scleraB);
        };

        if (open < kLidClosed) {
            const Vec3 north = C + f * R;
            const Vec3 south = C - f * R;
            for (int i = 0; i < kStacks; ++i) {
                const double t0 = kPi * (double(i) / kStacks);
                const double t1 = kPi * (double(i + 1) / kStacks);
                for (int j = 0; j < kSlices; ++j) {
                    const double p0 = (2.0 * kPi) * (double(j) / kSlices);
                    const double p1 = (2.0 * kPi) * (double(j + 1) / kSlices);
                    if (i == 0) {
                        pushSclera(north, sphPt(t1, p0), sphPt(t1, p1));
                        continue;
                    }
                    if (i + 1 == kStacks) {
                        pushSclera(south, sphPt(t0, p1), sphPt(t0, p0));
                        continue;
                    }
                    pushSclera(sphPt(t0, p0), sphPt(t1, p0), sphPt(t1, p1));
                    pushSclera(sphPt(t0, p0), sphPt(t1, p1), sphPt(t0, p1));
                }
            }
            return;
        }

        constexpr double kRimBack = 0.012;
        const double thetaStart = std::min(kPi - 1e-3, thetaCut + kRimBack);
        const Vec3 south = C - f * R;
        const int nLat = kStacks;
        for (int i = 0; i < nLat; ++i) {
            const double t0 = thetaStart + (kPi - thetaStart) * (double(i) / nLat);
            const double t1 = thetaStart + (kPi - thetaStart) * (double(i + 1) / nLat);
            for (int j = 0; j < kSlices; ++j) {
                const double p0 = (2.0 * kPi) * (double(j) / kSlices);
                const double p1 = (2.0 * kPi) * (double(j + 1) / kSlices);
                if (i + 1 == nLat) {
                    pushSclera(south, sphPt(t0, p1), sphPt(t0, p0));
                    continue;
                }
                pushSclera(sphPt(t0, p0), sphPt(t1, p0), sphPt(t1, p1));
                pushSclera(sphPt(t0, p0), sphPt(t1, p1), sphPt(t0, p1));
            }
        }

        const Vec3 planeO = C + f * dCut;
        const double pupilRad = irisRad * kPupilOfIris;

        auto discPoint = [&](double radius, double angle) -> Vec3 {
            return planeO + rAxis * (radius * std::cos(angle))
                   + uAxis * (radius * open * std::sin(angle));
        };

        auto pushDisc = [&](double radius, float cr, float cg, float cb) {
            Vec3 prev = discPoint(radius, 0.0);
            for (int s = 1; s <= kDiscSegs; ++s) {
                const double a = (2.0 * kPi * s) / kDiscSegs;
                const Vec3 rim = discPoint(radius, a);
                emitTri(out, planeO, prev, rim, cr, cg, cb);
                emitTri(out, planeO, rim, prev, cr, cg, cb);
                prev = rim;
            }
        };

        auto pushRing = [&](double rOuter, double rInner, float cr, float cg, float cb) {
            for (int s = 0; s < kDiscSegs; ++s) {
                const double a0 = (2.0 * kPi * s) / kDiscSegs;
                const double a1 = (2.0 * kPi * (s + 1) / kDiscSegs);
                const Vec3 o0 = discPoint(rOuter, a0);
                const Vec3 o1 = discPoint(rOuter, a1);
                const Vec3 i0 = discPoint(rInner, a0);
                const Vec3 i1 = discPoint(rInner, a1);
                emitTri(out, i0, o0, o1, cr, cg, cb);
                emitTri(out, i0, o1, i1, cr, cg, cb);
                emitTri(out, i0, o1, o0, cr, cg, cb);
                emitTri(out, i0, i1, o1, cr, cg, cb);
            }
        };

        pushRing(irisRad, pupilRad, irisR, irisG, irisB);
        pushDisc(pupilRad, pupilR, pupilG, pupilB);
    };

    appendEye(eyeLeft);
    appendEye(eyeRight);
}

} // namespace

PreviewWindow::PreviewWindow(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setWindowTitle(QStringLiteral("Gazer — Head Preview"));
    setMinimumSize(480, 400);
    resize(900, 720);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setUpdateBehavior(QOpenGLWidget::NoPartialUpdate);

    QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
    fmt.setDepthBufferSize(24);
    fmt.setSamples(4);
    setFormat(fmt);
}

PreviewWindow::~PreviewWindow()
{
    makeCurrent();
    m_meshVbo.destroy();
    m_dynVbo.destroy();
    delete m_prog;
    m_prog = nullptr;
    doneCurrent();
}

void PreviewWindow::setTrackerName(const QString& name)
{
    m_trackerName = name;
    if (isVisible()) {
        update();
    }
}

void PreviewWindow::setTheme(const ThemeColors& theme)
{
    m_theme = theme;
    if (isVisible()) {
        update();
    }
}

void PreviewWindow::onGazeUpdated(const gazer::GazePoint& point)
{
    m_gaze = point;
    if (!isVisible()) {
        return;
    }
    updateEyeLookFromGaze();
    update();
}

void PreviewWindow::onHeadPoseUpdated(const gazer::HeadPose& pose)
{
    m_head = pose;
    if (!isVisible()) {
        return;
    }
    update();
}

void PreviewWindow::updateEyeLookFromGaze()
{
    double targetX = 0.0;
    double targetY = 0.0;
    double targetOpen = 0.12; // nearly closed when tracking lost

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
        targetY = qBound(-1.15, -ny, 1.15); // screen Y down → head Y up
        targetOpen = 1.0;
    }

    constexpr double kLook = 0.42;
    constexpr double kLid = 0.55;
    m_lookX += (targetX - m_lookX) * kLook;
    m_lookY += (targetY - m_lookY) * kLook;
    m_eyeOpen += (targetOpen - m_eyeOpen) * kLid;
}

void PreviewWindow::showAndRaise()
{
    ensureMeshLoaded();
    showNormal();
    raise();
    activateWindow();
    updateEyeLookFromGaze();
    update();
}

void PreviewWindow::closeEvent(QCloseEvent* event)
{
    hide();
    event->ignore();
}

void PreviewWindow::ensureMeshLoaded()
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
    bool ok = false;
    for (const QString& path : candidates) {
        if (!StlMesh::loadFromFile(path, m_mesh, &err)) {
            continue;
        }
        ok = true;
        // Reflect through XZ (Y → −Y) so chin/forehead match preview up; fix winding.
        for (StlMesh::Tri& t : m_mesh.tris) {
            auto flipY = [](QVector3D v) { return QVector3D(v.x(), -v.y(), v.z()); };
            const QVector3D a = flipY(t.a);
            const QVector3D b = flipY(t.b);
            const QVector3D c = flipY(t.c);
            t.a = a;
            t.b = c;
            t.c = b;
        }
        // Force left = mirror of right so Z (and Y) match across the face.
        m_mesh.symmetrizeLeftFromRight();
        estimateEyeSockets();
        GAZER_INFO << "Head mesh:" << path << "tris" << m_mesh.tris.size()
                   << "eyeL" << m_eyeLeft.x() << m_eyeLeft.y() << m_eyeLeft.z()
                   << "eyeR" << m_eyeRight.x() << m_eyeRight.y() << m_eyeRight.z()
                   << "eyeRad" << m_eyeRadius;
        break;
    }
    if (!ok) {
        m_meshError = err.isEmpty() ? QStringLiteral("head.obj not found") : err;
        GAZER_WARN << "Head mesh load failed:" << m_meshError;
    }
}

void PreviewWindow::estimateEyeSockets()
{
    if (m_mesh.isEmpty()) {
        return;
    }

    // Mesh AABB is centered at origin after normalize — eyes are symmetric about X=0.
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

    GAZER_INFO << "Eyes halfSep" << halfSep << "y" << y << "z" << z << "r" << m_eyeRadius
               << "socketZ L/R" << zL << zR;
}

bool PreviewWindow::buildProgram()
{
    auto tryLink = [this](const char* vs, const char* fs) -> bool {
        auto* prog = new QOpenGLShaderProgram(this);
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
    GAZER_WARN << "Head preview shader failed:" << m_glError;
    return false;
}

void PreviewWindow::initializeGL()
{
    initializeOpenGLFunctions();
    m_glReady = buildProgram();

    if (!m_meshVbo.create() || !m_dynVbo.create()) {
        m_glReady = false;
        m_glError = QStringLiteral("Could not create GL buffers");
        GAZER_WARN << m_glError;
        return;
    }
    m_meshVbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_dynVbo.setUsagePattern(QOpenGLBuffer::DynamicDraw);
    m_meshUploaded = false;
    uploadMeshVbo();
}

void PreviewWindow::uploadMeshVbo()
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
    GAZER_INFO << "Head preview GPU mesh" << m_meshVertexCount << "verts";
}

void PreviewWindow::bindVertexLayout()
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

void PreviewWindow::drawHeadGl(const QRect& area)
{
    const double yaw = m_head.rotationValid ? m_head.yaw : 0.0;
    const double pitch = m_head.rotationValid ? m_head.pitch : 0.0;
    const double roll = m_head.rotationValid ? m_head.roll : 0.0;
    const double tx = m_head.positionValid ? m_head.x * 0.04 : 0.0;
    const double ty = m_head.positionValid ? m_head.y * 0.04 : 0.0;
    const double tz = m_head.positionValid ? (m_head.z - 55.0) * 0.025 : 0.0;

    const float scale = float(qMin(area.width(), area.height()) * 0.30);
    const float focal = 3.4f;
    const QVector2D origin(float(area.center().x()),
                           float(area.center().y() + area.height() * 0.04));
    const QMatrix4x4 pose = livePoseMatrix(yaw, pitch, roll, tx, ty, tz);

    m_prog->bind();
    m_prog->setUniformValue("uOrigin", origin);
    m_prog->setUniformValue("uViewport", QVector2D(float(width()), float(height())));
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

void PreviewWindow::paintOverlay(QPainter& p, const QRect& headArea)
{
    const double yaw = m_head.rotationValid ? m_head.yaw : 0.0;
    const double pitch = m_head.rotationValid ? m_head.pitch : 0.0;
    const double roll = m_head.rotationValid ? m_head.roll : 0.0;

    const QPointF g0(headArea.right() - 64, headArea.bottom() - 48);
    auto ax = [&](double x, double y, double z) {
        const Vec3 w = applyLivePose(Vec3{x, y, z}, yaw, pitch, roll, 0, 0, 0);
        const double d = qMax(0.5, (2.2 - w.z) / 2.2);
        return QPointF(g0.x() + w.x * 26 / d, g0.y() - w.y * 26 / d);
    };
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(220, 80, 80), 2));
    p.drawLine(g0, ax(1, 0, 0));
    p.setPen(QPen(QColor(80, 200, 100), 2));
    p.drawLine(g0, ax(0, 1, 0));
    p.setPen(QPen(QColor(80, 140, 255), 2));
    p.drawLine(g0, ax(0, 0, 1));

    p.setPen(m_theme.text.isValid() ? m_theme.text : QColor(220, 224, 230));
    p.setFont(QFont(QStringLiteral("Segoe UI"), 11));
    p.drawText(QRect(12, 4, width() - 24, 18), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("Tracker: %1  ·  mesh %2")
                   .arg(m_trackerName)
                   .arg(m_mesh.isEmpty() ? QStringLiteral("—")
                                         : QStringLiteral("%1 tris").arg(m_mesh.tris.size())));

    p.setFont(QFont(QStringLiteral("Consolas"), 10));
    p.setPen(m_theme.textSecondary.isValid() ? m_theme.textSecondary : QColor(150, 156, 165));
    const QString gazeBit = m_gaze.valid ? QStringLiteral("gaze live") : QStringLiteral("gaze lost");
    const QString line2 =
        m_head.valid()
            ? QStringLiteral("Yaw %1°  Pitch %2°  Roll %3°   X %4  Y %5  Z %6 cm  ·  %7")
                  .arg(m_head.yaw, 0, 'f', 1)
                  .arg(m_head.pitch, 0, 'f', 1)
                  .arg(m_head.roll, 0, 'f', 1)
                  .arg(m_head.x, 0, 'f', 1)
                  .arg(m_head.y, 0, 'f', 1)
                  .arg(m_head.z, 0, 'f', 1)
                  .arg(gazeBit)
            : QStringLiteral("Head pose: no sample — mesh still shown at rest  ·  %1").arg(gazeBit);
    p.drawText(QRect(12, 24, width() - 24, 18), Qt::AlignLeft | Qt::AlignVCenter, line2);
}

void PreviewWindow::paintGL()
{
    ensureMeshLoaded();
    uploadMeshVbo();

    const QColor bg = m_theme.bgMain.isValid() ? m_theme.bgMain : QColor(14, 14, 16);
    const QRect headArea = rect().adjusted(0, kHudH, 0, 0);
    const qreal dpr = devicePixelRatioF();
    const int vw = std::max(1, int(std::lround(width() * dpr)));
    const int vh = std::max(1, int(std::lround(height() * dpr)));
    const int headH = std::max(1, int(std::lround((height() - kHudH) * dpr)));

    QPainter painter(this);
    painter.beginNativePainting();

    glViewport(0, 0, vw, vh);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glClearColor(bg.redF(), bg.greenF(), bg.blueF(), 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, vw, headH);
    glClearColor(14.f / 255.f, 14.f / 255.f, 16.f / 255.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);

    if (m_mesh.isEmpty()) {
        painter.endNativePainting();
        painter.setPen(QColor(220, 100, 100));
        painter.setFont(QFont(QStringLiteral("Segoe UI"), 12));
        painter.drawText(headArea, Qt::AlignCenter,
                         QStringLiteral("Could not load resources/models/head.obj\n%1")
                             .arg(m_meshError));
        paintOverlay(painter, headArea);
        return;
    }

    if (!m_glReady || !m_prog) {
        painter.endNativePainting();
        painter.setPen(QColor(220, 100, 100));
        painter.setFont(QFont(QStringLiteral("Segoe UI"), 12));
        painter.drawText(headArea, Qt::AlignCenter,
                         QStringLiteral("GPU preview unavailable\n%1").arg(m_glError));
        paintOverlay(painter, headArea);
        return;
    }

    drawHeadGl(headArea);
    painter.endNativePainting();
    paintOverlay(painter, headArea);
}

} // namespace gazer
