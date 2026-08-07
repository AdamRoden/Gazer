#include "ui/PreviewWindow.h"

#include "utils/Log.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QDir>
#include <QPaintEvent>
#include <QPainter>
#include <QVector>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace gazer {

namespace {

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

/// Live pose: head faces you and tracks motion in the natural direction
/// (same sense as your yaw/pitch/roll — not inverted).
Vec3 applyLivePose(const Vec3& v, double yawDeg, double pitchDeg, double rollDeg, double tx,
                   double ty, double tz)
{
    // Match tracker sense: positive yaw/pitch/roll move the mesh the same way you move.
    const double y = qDegreesToRadians(yawDeg);
    const double p = qDegreesToRadians(-pitchDeg); // screen Y is up; nod matches view
    const double r = qDegreesToRadians(rollDeg);
    const double cy = std::cos(y), sy = std::sin(y);
    const double cp = std::cos(p), sp = std::sin(p);
    const double cr = std::cos(r), sr = std::sin(r);

    Vec3 a{v.x, v.y * cp - v.z * sp, v.y * sp + v.z * cp};
    Vec3 b{a.x * cy + a.z * sy, a.y, -a.x * sy + a.z * cy};
    Vec3 c{b.x * cr - b.y * sr, b.x * sr + b.y * cr, b.z};
    c.x += -tx; // lateral translation matches tracker sense (was opposite)
    c.y += ty;
    c.z += tz;
    return c;
}

struct DrawTri {
    Vec3 a, b, c;
    QColor color;
    double depth = 0;
};

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
    update();
}

void PreviewWindow::onHeadPoseUpdated(const gazer::HeadPose& pose)
{
    m_head = pose;
    update();
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
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("resources/models/head.stl")),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../resources/models/head.stl")),
        QStringLiteral("resources/models/head.stl"),
    };

    QString err;
    bool ok = false;
    for (const QString& path : candidates) {
        if (StlMesh::loadFromFile(path, m_mesh, &err)) {
            ok = true;
            GAZER_INFO << "Head mesh:" << path << "tris" << m_mesh.tris.size();
            // Netfabb STL is Z-up; after Y-up convert the face pointed left (−X).
            // Pitch −90° (Z-up → Y-up), then +90° yaw so the face looks at the camera (+Z).
            m_mesh.bakeRotation(0.f, -90.f, 0.f);
            m_mesh.bakeRotation(90.f, 0.f, 0.f);
            // Fix asymmetric face: left side is a mirror of the right.
            m_mesh.symmetrizeLeftFromRight();
            break;
        }
    }
    if (!ok) {
        m_meshError = err.isEmpty() ? QStringLiteral("head.stl not found") : err;
        GAZER_WARN << "Head mesh load failed:" << m_meshError;
    }
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
                   QStringLiteral("Could not load resources/models/head.stl\n%1").arg(m_meshError));
        return;
    }

    // Live pose (mirror view — looks at you, copies your motion)
    const double yaw = m_head.rotationValid ? m_head.yaw : 0.0;
    const double pitch = m_head.rotationValid ? m_head.pitch : 0.0;
    const double roll = m_head.rotationValid ? m_head.roll : 0.0;
    const double tx = m_head.positionValid ? m_head.x * 0.04 : 0.0;
    const double ty = m_head.positionValid ? m_head.y * 0.04 : 0.0;
    const double tz = m_head.positionValid ? (m_head.z - 55.0) * 0.025 : 0.0;

    const double scale = qMin(area.width(), area.height()) * 0.48;
    const double focal = 3.4;
    const QPointF origin(area.center().x(), area.center().y() + area.height() * 0.04);

    auto project = [&](const Vec3& world) -> QPointF {
        const double zCam = world.z + focal;
        const double d = qMax(0.4, zCam / focal);
        return {origin.x() + world.x * scale / d, origin.y() - world.y * scale / d};
    };

    const Vec3 lightDir = Vec3{-0.35, 0.5, 0.85}.normalized();
    const Vec3 fillDir = Vec3{0.55, 0.15, 0.25}.normalized();

    QVector<DrawTri> draw;
    draw.reserve(m_mesh.tris.size());

    for (const StlMesh::Tri& src : m_mesh.tris) {
        DrawTri t;
        t.a = applyLivePose(Vec3(src.a), yaw, pitch, roll, tx, ty, tz);
        t.b = applyLivePose(Vec3(src.b), yaw, pitch, roll, tx, ty, tz);
        t.c = applyLivePose(Vec3(src.c), yaw, pitch, roll, tx, ty, tz);

        const Vec3 n = (t.b - t.a).cross(t.c - t.a).normalized();
        // Camera looks toward −Z; keep faces toward camera.
        if (n.z < -0.02) {
            continue;
        }

        const double key = qMax(0.0, n.dot(lightDir));
        const double fill = qMax(0.0, n.dot(fillDir)) * 0.32;
        const double amb = 0.20;
        const double ndl = amb + 0.68 * key + fill;

        // Cool mannequin grey (reference-like)
        const int r = qBound(0, int(175 * ndl + 22 * key), 255);
        const int g = qBound(0, int(178 * ndl + 26 * key), 255);
        const int b = qBound(0, int(188 * ndl + 36 * key), 255);
        t.color = QColor(r, g, b);
        t.depth = (t.a.z + t.b.z + t.c.z) / 3.0;
        draw.push_back(t);
    }

    std::sort(draw.begin(), draw.end(),
              [](const DrawTri& a, const DrawTri& b) { return a.depth < b.depth; });

    // Soft ground contact shadow
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

    // Axis gizmo (mirrored pose)
    const QPointF g0(area.right() - 64, area.bottom() - 48);
    auto ax = [&](double x, double y, double z) {
        const Vec3 w = applyLivePose(Vec3{x, y, z}, yaw, pitch, roll, 0, 0, 0);
        const double d = qMax(0.5, (w.z + 2.2) / 2.2);
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
    p.fillRect(rect(), QColor(14, 14, 16));

    const int hudH = 48;
    paintHead(p, rect().adjusted(0, hudH, 0, 0));

    p.setPen(QColor(220, 224, 230));
    p.setFont(QFont(QStringLiteral("Segoe UI"), 11));
    p.drawText(QRect(12, 4, width() - 24, 18), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("Tracker: %1  ·  mesh %2")
                   .arg(m_trackerName)
                   .arg(m_mesh.isEmpty() ? QStringLiteral("—")
                                         : QStringLiteral("%1 tris").arg(m_mesh.tris.size())));

    p.setFont(QFont(QStringLiteral("Consolas"), 10));
    p.setPen(QColor(150, 156, 165));
    const QString line2 =
        m_head.valid()
            ? QStringLiteral("Yaw %1°  Pitch %2°  Roll %3°   X %4  Y %5  Z %6 cm")
                  .arg(m_head.yaw, 0, 'f', 1)
                  .arg(m_head.pitch, 0, 'f', 1)
                  .arg(m_head.roll, 0, 'f', 1)
                  .arg(m_head.x, 0, 'f', 1)
                  .arg(m_head.y, 0, 'f', 1)
                  .arg(m_head.z, 0, 'f', 1)
            : QStringLiteral("Head pose: no sample — mesh still shown at rest");
    p.drawText(QRect(12, 24, width() - 24, 18), Qt::AlignLeft | Qt::AlignVCenter, line2);
}

} // namespace gazer
