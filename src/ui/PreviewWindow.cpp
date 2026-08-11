#include "ui/PreviewWindow.h"

#include "utils/Log.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QPaintEvent>
#include <QPainter>
#include <QScreen>
#include <QVector>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace gazer {

namespace {

constexpr double kPi = 3.14159265358979323846;

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

struct DrawTri {
    Vec3 a, b, c;
    QColor color;
    double depth = 0;
};

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

} // namespace

PreviewWindow::PreviewWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("Gazer — Head Preview"));
    setMinimumSize(480, 400);
    resize(720, 600);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void PreviewWindow::setTrackerName(const QString& name)
{
    m_trackerName = name;
    update();
}

void PreviewWindow::setTheme(const ThemeColors& theme)
{
    m_theme = theme;
    update();
}

void PreviewWindow::onGazeUpdated(const gazer::GazePoint& point)
{
    m_gaze = point;
    updateEyeLookFromGaze();
    update();
}

void PreviewWindow::onHeadPoseUpdated(const gazer::HeadPose& pose)
{
    m_head = pose;
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

void PreviewWindow::paintHead(QPainter& p, const QRect& area)
{
    ensureMeshLoaded();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(area, QColor(14, 14, 16));

    if (m_mesh.isEmpty()) {
        p.setPen(QColor(220, 100, 100));
        p.setFont(QFont(QStringLiteral("Segoe UI"), 12));
        p.drawText(area, Qt::AlignCenter,
                   QStringLiteral("Could not load resources/models/head.obj\n%1").arg(m_meshError));
        return;
    }

    const double yaw = m_head.rotationValid ? m_head.yaw : 0.0;
    const double pitch = m_head.rotationValid ? m_head.pitch : 0.0;
    const double roll = m_head.rotationValid ? m_head.roll : 0.0;
    const double tx = m_head.positionValid ? m_head.x * 0.04 : 0.0;
    const double ty = m_head.positionValid ? m_head.y * 0.04 : 0.0;
    const double tz = m_head.positionValid ? (m_head.z - 55.0) * 0.025 : 0.0;

    const double scale = qMin(area.width(), area.height()) * 0.30;
    const double focal = 3.4;
    const QPointF origin(area.center().x(), area.center().y() + area.height() * 0.04);

    // Camera on +Z looking toward −Z: larger world.z is nearer → larger on screen.
    auto project = [&](const Vec3& world) -> QPointF {
        const double zCam = focal - world.z;
        const double d = qMax(0.4, zCam / focal);
        return {origin.x() + world.x * scale / d, origin.y() - world.y * scale / d};
    };

    const Vec3 lightDir = Vec3{-0.35, 0.5, 0.85}.normalized();
    const Vec3 fillDir = Vec3{0.55, 0.15, 0.25}.normalized();

    auto shade = [&](const Vec3& n, const QColor& base) -> QColor {
        const double key = qMax(0.0, n.dot(lightDir));
        const double fill = qMax(0.0, n.dot(fillDir)) * 0.32;
        const double ndl = 0.20 + 0.68 * key + fill;
        return QColor(qBound(0, int(base.red() * ndl + 18 * key), 255),
                      qBound(0, int(base.green() * ndl + 18 * key), 255),
                      qBound(0, int(base.blue() * ndl + 22 * key), 255));
    };

    QVector<DrawTri> draw;
    draw.reserve(m_mesh.tris.size() + 2500);

    auto pushTri = [&](const Vec3& a, const Vec3& b, const Vec3& c, const QColor& base,
                       double depthBias = 0.0) {
        DrawTri t;
        t.a = applyLivePose(a, yaw, pitch, roll, tx, ty, tz);
        t.b = applyLivePose(b, yaw, pitch, roll, tx, ty, tz);
        t.c = applyLivePose(c, yaw, pitch, roll, tx, ty, tz);
        const Vec3 n = (t.b - t.a).cross(t.c - t.a).normalized();
        if (n.z < -0.02) {
            return; // back-face cull (camera looks toward −Z)
        }
        t.color = shade(n, base);
        t.depth = (t.a.z + t.b.z + t.c.z) / 3.0 + depthBias;
        draw.push_back(t);
    };

    // Mask.
    for (const StlMesh::Tri& src : m_mesh.tris) {
        pushTri(Vec3(src.a), Vec3(src.b), Vec3(src.c), QColor(175, 178, 188));
    }

    // Eyes: perfect sphere (UV aligned to lookDir) with the look-side cap removed.
    // Iris = planar disc = exact plane∩sphere circle (radius 40% of R). Pupil on same plane.
    {
        const double open = qBound(0.0, m_eyeOpen, 1.0);
        const double lookX = qBound(-1.0, m_lookX, 1.0);
        const double lookY = qBound(-1.0, m_lookY, 1.0);
        const double R = double(m_eyeRadius);

        const Vec3 worldLook = Vec3{lookX * kGazeGain, lookY * kGazeGain, 1.0}.normalized();
        Vec3 lookDir = inverseLiveRot(worldLook, yaw, pitch, roll).normalized();
        if (lookDir.dot(lookDir) < 1e-8) {
            lookDir = Vec3{0, 0, 1};
        }

        // Sphere tessellation: stacks from cut latitude → back pole; slices around look axis.
        constexpr int kStacks = 14;
        constexpr int kSlices = 28;
        constexpr int kDiscSegs = 32;

        const QColor scleraColor(236, 238, 242);
        const QColor irisColor(40, 95, 150);
        const QColor pupilColor(8, 8, 12);

        auto appendEye = [&](const QVector3D& centerQ) {
            const Vec3 C(centerQ);
            const Vec3 f = lookDir;

            // Orthonormal frame: +f = look (iris faces this way).
            Vec3 rAxis = Vec3{0, 1, 0}.cross(f);
            if (rAxis.dot(rAxis) < 1e-8) {
                rAxis = Vec3{1, 0, 0}.cross(f);
            }
            rAxis = rAxis.normalized();
            const Vec3 uAxis = f.cross(rAxis).normalized();

            // Plane ⟂ f at distance d from C so plane∩sphere is a circle of radius irisR.
            //   irisR = 0.4 R
            //   d = sqrt(R² − irisR²) = R √(1 − 0.16) = R √0.84
            const double irisR = R * kIrisRadiusOfEye;
            const double dCut = std::sqrt(std::max(0.0, R * R - irisR * irisR));
            const double thetaCut = std::acos(qBound(-1.0, dCut / R, 1.0)); // from +f pole

            // Point on unit sphere in look-aligned spherical coords:
            // theta=0 at +f, theta=pi at −f; phi around the look axis.
            auto sphPt = [&](double theta, double phi) -> Vec3 {
                const double st = std::sin(theta);
                const double ct = std::cos(theta);
                const double cp = std::cos(phi);
                const double sp = std::sin(phi);
                // Local: x along r, y along u, z along f
                return C + (rAxis * (R * st * cp) + uAxis * (R * st * sp) + f * (R * ct));
            };

            auto pushSclera = [&](const Vec3& a, const Vec3& b, const Vec3& c) {
                pushTri(a, b, c, scleraColor);
            };

            if (open < kLidClosed) {
                // Lids closed: full closed sphere (no iris window).
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

            // Open: sphere from just behind the cut latitude → back pole.
            // Start slightly past thetaCut so the rim sits a hair behind the iris plane
            // (avoids coplanar z-fight without a huge depth bias that would leap in front
            // of the mask).
            constexpr double kRimBack = 0.012; // radians past the cut
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

            // Iris = planar annulus (outer = plane∩sphere = 0.4 R); pupil cut out of center.
            // No depth bias — must share real depth with sclera/mask.
            const Vec3 planeO = C + f * dCut;
            const double pupilR = irisR * kPupilOfIris;

            auto discPoint = [&](double radius, double angle) -> Vec3 {
                return planeO + rAxis * (radius * std::cos(angle))
                       + uAxis * (radius * open * std::sin(angle));
            };

            auto pushDisc = [&](double radius, const QColor& color) {
                Vec3 prev = discPoint(radius, 0.0);
                for (int s = 1; s <= kDiscSegs; ++s) {
                    const double a = (2.0 * kPi * s) / kDiscSegs;
                    const Vec3 rim = discPoint(radius, a);
                    pushTri(planeO, prev, rim, color);
                    pushTri(planeO, rim, prev, color);
                    prev = rim;
                }
            };

            // Iris ring: outer irisR, inner pupilR (pupil area cut out).
            auto pushRing = [&](double rOuter, double rInner, const QColor& color) {
                for (int s = 0; s < kDiscSegs; ++s) {
                    const double a0 = (2.0 * kPi * s) / kDiscSegs;
                    const double a1 = (2.0 * kPi * (s + 1)) / kDiscSegs;
                    const Vec3 o0 = discPoint(rOuter, a0);
                    const Vec3 o1 = discPoint(rOuter, a1);
                    const Vec3 i0 = discPoint(rInner, a0);
                    const Vec3 i1 = discPoint(rInner, a1);
                    pushTri(i0, o0, o1, color);
                    pushTri(i0, o1, i1, color);
                    pushTri(i0, o1, o0, color);
                    pushTri(i0, i1, o1, color);
                }
            };

            pushRing(irisR, pupilR, irisColor);
            pushDisc(pupilR, pupilColor);
        };

        appendEye(m_eyeLeft);
        appendEye(m_eyeRight);
    }

    std::sort(draw.begin(), draw.end(),
              [](const DrawTri& a, const DrawTri& b) { return a.depth < b.depth; });

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 75));
    p.drawEllipse(QPointF(origin.x(), origin.y() + scale * 0.95), scale * 0.48, scale * 0.11);

    for (const DrawTri& t : draw) {
        QPolygonF poly;
        poly << project(t.a) << project(t.b) << project(t.c);
        p.setBrush(t.color);
        p.setPen(QPen(t.color.darker(114), 0.55));
        p.drawPolygon(poly);
    }

    // Axis gizmo (pose only, no translation).
    const QPointF g0(area.right() - 64, area.bottom() - 48);
    auto ax = [&](double x, double y, double z) {
        const Vec3 w = applyLivePose(Vec3{x, y, z}, yaw, pitch, roll, 0, 0, 0);
        const double d = qMax(0.5, (2.2 - w.z) / 2.2);
        return QPointF(g0.x() + w.x * 26 / d, g0.y() - w.y * 26 / d);
    };
    p.setPen(QPen(QColor(220, 80, 80), 2));
    p.drawLine(g0, ax(1, 0, 0));
    p.setPen(QPen(QColor(80, 200, 100), 2));
    p.drawLine(g0, ax(0, 1, 0));
    p.setPen(QPen(QColor(80, 140, 255), 2));
    p.drawLine(g0, ax(0, 0, 1));
}

void PreviewWindow::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.fillRect(rect(), m_theme.bgMain.isValid() ? m_theme.bgMain : QColor(14, 14, 16));

    const int hudH = 48;
    // Head view keeps its own dark stage; HUD uses theme colors.
    paintHead(p, rect().adjusted(0, hudH, 0, 0));

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

} // namespace gazer
