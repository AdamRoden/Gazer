#include "ui/PreviewGeometry.h"

#include <QtMath>
#include <algorithm>
#include <cmath>

namespace gazer {
namespace PreviewGeom {

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

/// Iris disc radius as a fraction of eyeball radius (plane∩sphere = that circle).
constexpr double kIrisRadiusOfEye = 0.3;
/// Pupil radius as a fraction of iris radius.
constexpr double kPupilOfIris = 0.35;
constexpr double kLidClosed = 0.16;
constexpr double kGazeGain = 0.5;

// Compatibility (GL 2.1 / ES 2.0) — no #version; Qt injects one when needed.
const char* kVertCompat = R"(
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

const char* kFragCompat = R"(
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

const char* kVertCore = R"(
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

const char* kFragCore = R"(
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


} // namespace PreviewGeom
} // namespace gazer
