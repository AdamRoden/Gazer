#include "ui/StlMesh.h"

#include "utils/Log.h"

#include <QFile>
#include <QRegularExpression>
#include <QtMath>
#include <cmath>
#include <cstring>
#include <utility>

namespace gazer {

namespace {

QVector3D rotateYpr(const QVector3D& v, float yawDeg, float pitchDeg, float rollDeg)
{
    const float y = qDegreesToRadians(yawDeg);
    const float p = qDegreesToRadians(pitchDeg);
    const float r = qDegreesToRadians(rollDeg);
    const float cy = std::cos(y), sy = std::sin(y);
    const float cp = std::cos(p), sp = std::sin(p);
    const float cr = std::cos(r), sr = std::sin(r);
    QVector3D a(v.x(), v.y() * cp - v.z() * sp, v.y() * sp + v.z() * cp);
    QVector3D b(a.x() * cy + a.z() * sy, a.y(), -a.x() * sy + a.z() * cy);
    return {b.x() * cr - b.y() * sr, b.x() * sr + b.y() * cr, b.z()};
}

void recomputeNormals(StlMesh& m)
{
    for (StlMesh::Tri& t : m.tris) {
        t.normal = QVector3D::crossProduct(t.b - t.a, t.c - t.a).normalized();
    }
}

void normalizeMesh(StlMesh& m)
{
    if (m.tris.isEmpty()) {
        return;
    }
    m.boundsMin = m.tris.first().a;
    m.boundsMax = m.tris.first().a;
    auto consider = [&](const QVector3D& p) {
        m.boundsMin.setX(qMin(m.boundsMin.x(), p.x()));
        m.boundsMin.setY(qMin(m.boundsMin.y(), p.y()));
        m.boundsMin.setZ(qMin(m.boundsMin.z(), p.z()));
        m.boundsMax.setX(qMax(m.boundsMax.x(), p.x()));
        m.boundsMax.setY(qMax(m.boundsMax.y(), p.y()));
        m.boundsMax.setZ(qMax(m.boundsMax.z(), p.z()));
    };
    for (const StlMesh::Tri& t : m.tris) {
        consider(t.a);
        consider(t.b);
        consider(t.c);
    }
    m.center = (m.boundsMin + m.boundsMax) * 0.5f;
    const QVector3D ext = m.boundsMax - m.boundsMin;
    const float maxExt = qMax(ext.x(), qMax(ext.y(), ext.z()));
    const float s = maxExt > 1e-6f ? (2.0f / maxExt) : 1.0f;
    m.radius = 0.f;
    for (StlMesh::Tri& t : m.tris) {
        t.a = (t.a - m.center) * s;
        t.b = (t.b - m.center) * s;
        t.c = (t.c - m.center) * s;
        m.radius = qMax(m.radius, t.a.length());
        m.radius = qMax(m.radius, t.b.length());
        m.radius = qMax(m.radius, t.c.length());
    }
    m.center = {};
    recomputeNormals(m);
}

bool loadBinary(const QByteArray& data, StlMesh& out, QString* error)
{
    if (data.size() < 84) {
        if (error) {
            *error = QStringLiteral("STL too small");
        }
        return false;
    }
    quint32 count = 0;
    std::memcpy(&count, data.constData() + 80, 4);
    const qint64 need = 84 + qint64(count) * 50;
    if (data.size() < need) {
        if (error) {
            *error = QStringLiteral("STL truncated (need %1 bytes for %2 tris)")
                         .arg(need)
                         .arg(count);
        }
        return false;
    }
    out.tris.resize(int(count));
    const char* p = data.constData() + 84;
    for (quint32 i = 0; i < count; ++i) {
        float f[12];
        std::memcpy(f, p, 48);
        p += 50;
        StlMesh::Tri& t = out.tris[int(i)];
        t.normal = QVector3D(f[0], f[1], f[2]);
        t.a = QVector3D(f[3], f[4], f[5]);
        t.b = QVector3D(f[6], f[7], f[8]);
        t.c = QVector3D(f[9], f[10], f[11]);
    }
    return true;
}

bool loadAscii(const QByteArray& data, StlMesh& out, QString* error)
{
    const QString text = QString::fromLatin1(data);
    const QStringList lines =
        text.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
    StlMesh::Tri cur;
    int vi = 0;
    for (QString line : lines) {
        line = line.trimmed();
        if (!line.startsWith(QLatin1String("vertex"), Qt::CaseInsensitive)) {
            continue;
        }
        const QStringList parts =
            line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (parts.size() < 4) {
            continue;
        }
        const QVector3D v(parts[1].toFloat(), parts[2].toFloat(), parts[3].toFloat());
        if (vi == 0) {
            cur.a = v;
        } else if (vi == 1) {
            cur.b = v;
        } else {
            cur.c = v;
            out.tris.push_back(cur);
            vi = -1;
        }
        ++vi;
    }
    if (out.tris.isEmpty()) {
        if (error) {
            *error = QStringLiteral("No triangles in ASCII STL");
        }
        return false;
    }
    return true;
}

} // namespace

bool StlMesh::loadFromFile(const QString& path, StlMesh& out, QString* error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Cannot open STL: %1").arg(path);
        }
        return false;
    }
    const QByteArray data = f.readAll();
    out = {};

    const bool looksAscii = data.startsWith("solid") && data.contains("facet")
                            && data.contains("vertex");
    const bool ok = looksAscii ? loadAscii(data, out, error) : loadBinary(data, out, error);
    if (!ok) {
        return false;
    }
    normalizeMesh(out);
    GAZER_INFO << "STL loaded" << path << "tris" << out.tris.size() << "radius" << out.radius;
    return true;
}

void StlMesh::bakeRotation(float yawDeg, float pitchDeg, float rollDeg)
{
    for (Tri& t : tris) {
        t.a = rotateYpr(t.a, yawDeg, pitchDeg, rollDeg);
        t.b = rotateYpr(t.b, yawDeg, pitchDeg, rollDeg);
        t.c = rotateYpr(t.c, yawDeg, pitchDeg, rollDeg);
    }
    QVector3D c;
    for (const Tri& t : tris) {
        c += t.a + t.b + t.c;
    }
    c /= float(tris.size() * 3);
    float r = 0;
    for (Tri& t : tris) {
        t.a -= c;
        t.b -= c;
        t.c -= c;
        r = qMax(r, t.a.length());
        r = qMax(r, t.b.length());
        r = qMax(r, t.c.length());
    }
    if (r > 1e-6f) {
        const float s = 1.0f / r;
        for (Tri& t : tris) {
            t.a *= s;
            t.b *= s;
            t.c *= s;
        }
        radius = 1.f;
    }
    recomputeNormals(*this);
}

void StlMesh::symmetrizeLeftFromRight()
{
    if (tris.isEmpty()) {
        return;
    }

    auto mirrorX = [](QVector3D v) {
        v.setX(-v.x());
        return v;
    };

    QVector<Tri> out;
    out.reserve(tris.size() * 2);

    for (const Tri& t : tris) {
        const float cx = (t.a.x() + t.b.x() + t.c.x()) / 3.0f;

        // Keep right side and midplane (x >= 0). Drop left (replaced by mirrors).
        if (cx < -1e-4f) {
            continue;
        }

        // Snap near-midplane verts to x=0 for a clean seam.
        Tri right = t;
        auto snap = [](QVector3D& v) {
            if (std::abs(v.x()) < 1e-3f) {
                v.setX(0.f);
            }
        };
        snap(right.a);
        snap(right.b);
        snap(right.c);
        out.push_back(right);

        // Mirror of right → left (skip pure midplane so we don't double faces).
        if (cx > 1e-3f) {
            Tri left;
            // Flip X and reverse winding so normals stay outward.
            left.a = mirrorX(right.a);
            left.b = mirrorX(right.c);
            left.c = mirrorX(right.b);
            out.push_back(left);
        }
    }

    if (out.isEmpty()) {
        return;
    }
    tris = std::move(out);
    recomputeNormals(*this);
    GAZER_INFO << "STL symmetrized L←R tris" << tris.size();
}

} // namespace gazer
