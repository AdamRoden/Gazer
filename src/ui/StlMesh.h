#pragma once

#include <QString>
#include <QVector3D>
#include <QVector>

namespace gazer {

/// Triangle mesh used by the head preview.
/// Named historically for STL; the loader accepts Wavefront OBJ only.
struct StlMesh {
    struct Tri {
        QVector3D a, b, c;
        QVector3D normal; // unit geometric normal (recomputed on load / orient)
    };

    QVector<Tri> tris;
    QVector3D boundsMin;
    QVector3D boundsMax;
    QVector3D center; // zero after normalize
    float radius = 1.f;

    [[nodiscard]] bool isEmpty() const { return tris.isEmpty(); }

    /// Load Wavefront .obj (tris/quads/n-gons). On success: centered at origin,
    /// uniformly scaled so the longest AABB edge is length 2.
    [[nodiscard]] static bool loadFromFile(const QString& path, StlMesh& out,
                                           QString* error = nullptr);

    /// Rebuild per-triangle unit normals from vertex winding.
    void recomputeNormals();

    /// Clip to the half-space x ≥ 0 with a planar cut at x = 0. Triangles that
    /// straddle the plane are split; new vertices lie on the plane so the seam
    /// has closed edges (no open gaps).
    void clipKeepPositiveX();

    /// Force bilateral symmetry about the YZ plane (X = 0): planar-cut to x ≥ 0
    /// (creating midplane edges), then mirror that half to rebuild x < 0.
    void symmetrizeLeftFromRight();
};

} // namespace gazer
