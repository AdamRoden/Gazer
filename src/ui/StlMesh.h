#pragma once

#include <QColor>
#include <QString>
#include <QVector3D>
#include <QVector>

namespace gazer {

/// Simple triangle mesh loaded from binary or ASCII STL.
struct StlMesh {
    struct Tri {
        QVector3D a, b, c;
        QVector3D normal; // unit geometric normal
    };

    QVector<Tri> tris;
    QVector3D boundsMin;
    QVector3D boundsMax;
    QVector3D center;
    float radius = 1.f; // max extent from center after normalize

    [[nodiscard]] bool isEmpty() const { return tris.isEmpty(); }

    /// Load binary or ASCII .stl. On success, mesh is centered at origin and
    /// scaled so max |component| ≈ 1 (radius ~1).
    [[nodiscard]] static bool loadFromFile(const QString& path, StlMesh& out, QString* error = nullptr);

    /// Apply a permanent orientation fix (e.g. Z-up → Y-up, face +Z).
    void bakeRotation(float yawDeg, float pitchDeg, float rollDeg);

    /// Rebuild left half (x &lt; 0) as a mirror of the right half so the face is symmetrical.
    void symmetrizeLeftFromRight();
};

} // namespace gazer
