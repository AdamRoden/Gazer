#include "ui/StlMesh.h"

#include "utils/Log.h"

#include <QFile>
#include <QTextStream>
#include <cmath>

namespace gazer {

namespace {

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
    m.recomputeNormals();
}

/// Parse one OBJ face index token: `v`, `v/vt`, `v//vn`, or `v/vt/vn` (1-based).
bool parseFaceIndex(const QString& token, int vertCount, int* vertIndex, QString* error)
{
    const QStringList parts = token.split(QLatin1Char('/'));
    if (parts.isEmpty() || parts[0].isEmpty()) {
        if (error) {
            *error = QStringLiteral("Empty face index");
        }
        return false;
    }
    bool ok = false;
    int idx = parts[0].toInt(&ok);
    if (!ok) {
        if (error) {
            *error = QStringLiteral("Bad face index: %1").arg(token);
        }
        return false;
    }
    if (idx < 0) {
        idx = vertCount + idx + 1;
    }
    if (idx < 1 || idx > vertCount) {
        if (error) {
            *error = QStringLiteral("Face index out of range: %1").arg(token);
        }
        return false;
    }
    *vertIndex = idx - 1;
    return true;
}

bool loadObj(const QString& path, StlMesh& out, QString* error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            *error = QStringLiteral("Cannot open OBJ: %1").arg(path);
        }
        return false;
    }

    QVector<QVector3D> verts;
    verts.reserve(8192);
    out = {};

    QTextStream in(&f);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }

        if (line.startsWith(QLatin1String("v "))) {
            const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (parts.size() < 4) {
                continue;
            }
            verts.push_back(QVector3D(parts[1].toFloat(), parts[2].toFloat(), parts[3].toFloat()));
            continue;
        }

        if (!line.startsWith(QLatin1String("f "))) {
            continue;
        }

        const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.size() < 4) {
            continue;
        }

        QVector<int> indices;
        indices.reserve(parts.size() - 1);
        for (int i = 1; i < parts.size(); ++i) {
            int vi = 0;
            if (!parseFaceIndex(parts[i], verts.size(), &vi, error)) {
                return false;
            }
            indices.push_back(vi);
        }

        // Fan triangulation for tris / quads / n-gons.
        for (int i = 1; i + 1 < indices.size(); ++i) {
            StlMesh::Tri t;
            t.a = verts[indices[0]];
            t.b = verts[indices[i]];
            t.c = verts[indices[i + 1]];
            out.tris.push_back(t);
        }
    }

    if (out.tris.isEmpty()) {
        if (error) {
            *error = verts.isEmpty() ? QStringLiteral("No vertices in OBJ")
                                     : QStringLiteral("No faces in OBJ");
        }
        return false;
    }

    normalizeMesh(out);
    GAZER_INFO << "OBJ loaded" << path << "verts" << verts.size() << "tris" << out.tris.size()
               << "radius" << out.radius;
    return true;
}

} // namespace

bool StlMesh::loadFromFile(const QString& path, StlMesh& out, QString* error)
{
    return loadObj(path, out, error);
}

void StlMesh::recomputeNormals()
{
    for (Tri& t : tris) {
        t.normal = QVector3D::crossProduct(t.b - t.a, t.c - t.a).normalized();
    }
}

void StlMesh::clipKeepPositiveX()
{
    if (tris.isEmpty()) {
        return;
    }

    constexpr float kEps = 1e-4f;

    auto snap = [](QVector3D v) {
        if (std::abs(v.x()) < kEps) {
            v.setX(0.f);
        }
        return v;
    };

    // -1 = left of plane, 0 = on plane, +1 = right (kept side).
    auto sideOf = [](const QVector3D& v) -> int {
        if (v.x() > kEps) {
            return 1;
        }
        if (v.x() < -kEps) {
            return -1;
        }
        return 0;
    };

    auto intersectEdge = [&](const QVector3D& a, const QVector3D& b) -> QVector3D {
        const float dx = b.x() - a.x();
        if (std::abs(dx) < 1e-12f) {
            return snap(a);
        }
        const float t = (0.f - a.x()) / dx;
        QVector3D p = a + t * (b - a);
        p.setX(0.f);
        return p;
    };

    auto emitTri = [](QVector<Tri>& out, QVector3D a, QVector3D b, QVector3D c) {
        // Degenerate (zero area) → skip.
        const QVector3D n = QVector3D::crossProduct(b - a, c - a);
        if (n.lengthSquared() < 1e-14f) {
            return;
        }
        Tri t;
        t.a = a;
        t.b = b;
        t.c = c;
        out.push_back(t);
    };

    QVector<Tri> out;
    out.reserve(tris.size() * 2);

    for (const Tri& src : tris) {
        const QVector3D v[3] = {snap(src.a), snap(src.b), snap(src.c)};
        const int s[3] = {sideOf(v[0]), sideOf(v[1]), sideOf(v[2])};

        // Treat "on plane" as keep-side for classification of fully-on tris.
        int nPos = 0, nNeg = 0;
        for (int i = 0; i < 3; ++i) {
            if (s[i] > 0) {
                ++nPos;
            } else if (s[i] < 0) {
                ++nNeg;
            }
        }

        if (nNeg == 0) {
            // Entirely on keep side or on the plane.
            emitTri(out, v[0], v[1], v[2]);
            continue;
        }
        if (nPos == 0) {
            // Entirely on discard side (or only on plane with no positive — drop).
            // Pure midplane faces (all x=0) are kept once as nNeg==0 above.
            continue;
        }

        // Mixed: collect polygon on the keep side (pos + on-plane + intersections).
        // Walk edges; Sutherland–Hodgman style for one plane.
        QVector3D poly[8];
        int nPoly = 0;
        for (int i = 0; i < 3; ++i) {
            const int j = (i + 1) % 3;
            const QVector3D& a = v[i];
            const QVector3D& b = v[j];
            const int sa = s[i];
            const int sb = s[j];

            const bool aKeep = sa >= 0; // on plane or positive
            const bool bKeep = sb >= 0;

            if (aKeep && bKeep) {
                poly[nPoly++] = b;
            } else if (aKeep && !bKeep) {
                poly[nPoly++] = intersectEdge(a, b);
            } else if (!aKeep && bKeep) {
                poly[nPoly++] = intersectEdge(a, b);
                poly[nPoly++] = b;
            }
        }

        // Fan triangulation of keep polygon (3 or 4 verts typically).
        if (nPoly < 3) {
            continue;
        }
        for (int i = 1; i + 1 < nPoly; ++i) {
            emitTri(out, poly[0], poly[i], poly[i + 1]);
        }
    }

    tris = std::move(out);
    recomputeNormals();
    GAZER_INFO << "Mesh clipped x>=0 tris" << tris.size();
}

void StlMesh::symmetrizeLeftFromRight()
{
    if (tris.isEmpty()) {
        return;
    }

    // Planar cut first so straddling faces become clean midplane edges.
    clipKeepPositiveX();

    auto mirrorX = [](QVector3D v) {
        v.setX(-v.x());
        return v;
    };

    QVector<Tri> out;
    out.reserve(tris.size() * 2);

    for (const Tri& t : tris) {
        const float cx = (t.a.x() + t.b.x() + t.c.x()) / 3.0f;

        // Keep every right-half / seam triangle.
        out.push_back(t);

        // Mirror only tris with volume on the right (not pure midplane).
        // Midplane faces stay once; their edges already seal the seam.
        if (cx > 1e-4f) {
            Tri left;
            left.a = mirrorX(t.a);
            left.b = mirrorX(t.c); // reverse winding
            left.c = mirrorX(t.b);
            out.push_back(left);
        }
    }

    tris = std::move(out);
    recomputeNormals();
    GAZER_INFO << "Mesh symmetrized L←R tris" << tris.size();
}

} // namespace gazer
