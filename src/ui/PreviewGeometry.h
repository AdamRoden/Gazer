#pragma once

#include <QMatrix4x4>
#include <QVector>
#include <QVector3D>
#include <cmath>

namespace gazer {
namespace PreviewGeom {

constexpr double kPi = 3.14159265358979323846;
constexpr int kHudH = 48;
constexpr int kVertStrideFloats = 9;
constexpr int kVertStrideBytes = kVertStrideFloats * int(sizeof(float));
constexpr float kEyeYNudge = 0.0f;
constexpr float kEyeZRecess = 0.28f;
constexpr float kEyeRadiusScale = 1.0f;
constexpr float kEyeRadiusMin = 0.15f;
constexpr float kEyeRadiusMax = 0.3f;
/// Absolute tracker Z is cm from the camera; this rest distance places the mesh at the origin.
constexpr double kHeadRestZCm = 55.0;
constexpr double kHeadPosZScale = 0.025;

/// Mesh Z. Origin-relative poses are already 0 at Recenter, so skip the rest subtract.
inline double headMeshTz(double zCm, bool originRelative)
{
    return (originRelative ? zCm : (zCm - kHeadRestZCm)) * kHeadPosZScale;
}

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

Vec3 applyLivePose(const Vec3& v, double yawDeg, double pitchDeg, double rollDeg, double tx,
                   double ty, double tz);
QMatrix4x4 livePoseMatrix(double yawDeg, double pitchDeg, double rollDeg, double tx, double ty,
                          double tz);
void emitTri(QVector<float>& out, const Vec3& a, const Vec3& b, const Vec3& c, float r, float g,
             float bch);
void appendShadow(QVector<float>& out);
void appendEyes(QVector<float>& out, const QVector3D& eyeLeft, const QVector3D& eyeRight,
                float eyeRadius, double lookX, double lookY, double eyeOpen, double yaw,
                double pitch, double roll);

extern const char* kVertCompat;
extern const char* kFragCompat;
extern const char* kVertCore;
extern const char* kFragCore;

} // namespace PreviewGeom
} // namespace gazer
