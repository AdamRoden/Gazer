#include "mapping/HeadPoseCurve.h"

#include <QtMath>
#include <algorithm>
#include <cmath>

namespace gazer {

namespace {

constexpr double kWheel = 120.0;
constexpr double kMaxMouseStep = 80.0;
constexpr double kMaxScrollDelta = 360.0;

const char* axisIds[] = {"yaw", "pitch", "roll", "x", "y", "z"};
const char* axisLabels[] = {"Yaw", "Pitch", "Roll", "X", "Y", "Z"};
const char* destIds[] = {"mouseX", "mouseY", "scrollV", "scrollH", "gazeX", "gazeY",
                         "joyLX",  "joyLY",  "joyRX",   "joyRY",   "command"};
const char* destLabels[] = {"MouseMove Horizontal",
                            "MouseMove Vertical",
                            "Scroll Vertical",
                            "Scroll Horizontal",
                            "GazeOffset Horizontal",
                            "GazeOffset Vertical",
                            "Left stick X",
                            "Left stick Y",
                            "Right stick X",
                            "Right stick Y",
                            "Command"};

int axisIndex(HeadPoseAxis a)
{
    return qBound(0, int(a), 5);
}

int destIndex(HeadPoseDest d)
{
    return qBound(0, int(d), 10);
}

} // namespace

const char* headPoseAxisId(HeadPoseAxis axis)
{
    return axisIds[axisIndex(axis)];
}

const char* headPoseAxisLabel(HeadPoseAxis axis)
{
    return axisLabels[axisIndex(axis)];
}

HeadPoseAxis headPoseAxisFromId(const QString& id, bool* ok)
{
    const QString s = id.trimmed().toLower();
    for (int i = 0; i < 6; ++i) {
        if (s == QLatin1String(axisIds[i])) {
            if (ok) {
                *ok = true;
            }
            return HeadPoseAxis(i);
        }
    }
    if (ok) {
        *ok = false;
    }
    return HeadPoseAxis::Yaw;
}

const char* headPoseDestId(HeadPoseDest dest)
{
    return destIds[destIndex(dest)];
}

const char* headPoseDestLabel(HeadPoseDest dest)
{
    return destLabels[destIndex(dest)];
}

HeadPoseDest headPoseDestFromId(const QString& id, bool* ok)
{
    const QString s = id.trimmed();
    for (int i = 0; i < 11; ++i) {
        if (s.compare(QLatin1String(destIds[i]), Qt::CaseInsensitive) == 0) {
            if (ok) {
                *ok = true;
            }
            return HeadPoseDest(i);
        }
    }
    if (ok) {
        *ok = false;
    }
    return HeadPoseDest::MouseX;
}

const char* headPoseDestJoyAxis(HeadPoseDest dest)
{
    switch (dest) {
    case HeadPoseDest::JoyLX:
        return "lx";
    case HeadPoseDest::JoyLY:
        return "ly";
    case HeadPoseDest::JoyRX:
        return "rx";
    case HeadPoseDest::JoyRY:
        return "ry";
    default:
        return "";
    }
}

HeadPoseMap defaultHeadPoseMap()
{
    HeadPoseMap m;
    m.enabled = true;
    m.source = HeadPoseAxis::Yaw;
    m.dest = HeadPoseDest::MouseX;
    m.points = {{-25.0, -600.0}, {0.0, 0.0}, {25.0, 600.0}};
    m.commandAt = 15.0;
    m.hysteresis = 2.0;
    return m;
}

void clampHeadPoseMap(HeadPoseMap& m)
{
    m.id = m.id.trimmed();
    m.command = m.command.trimmed();
    m.hysteresis = qBound(0.1, m.hysteresis, 45.0);
    m.commandAt = qBound(-180.0, m.commandAt, 180.0);
    if (qAbs(m.commandAt) < 0.5) {
        m.commandAt = 15.0;
    }
    QVector<HeadPoseCurvePoint> pts;
    pts.reserve(m.points.size());
    for (HeadPoseCurvePoint p : m.points) {
        p.in = qBound(-1000.0, p.in, 1000.0);
        p.out = qBound(-10000.0, p.out, 10000.0);
        pts.push_back(p);
    }
    std::sort(pts.begin(), pts.end(),
              [](const HeadPoseCurvePoint& a, const HeadPoseCurvePoint& b) { return a.in < b.in; });
    QVector<HeadPoseCurvePoint> uniq;
    for (const HeadPoseCurvePoint& p : pts) {
        if (!uniq.isEmpty() && qAbs(uniq.last().in - p.in) < 1e-6) {
            uniq.last() = p;
            continue;
        }
        uniq.push_back(p);
        if (uniq.size() >= kMaxHeadPoseCurvePoints) {
            break;
        }
    }
    if (uniq.size() < 2) {
        uniq = defaultHeadPoseMap().points;
    }
    m.points = std::move(uniq);
}

QString headPoseMapDestSummary(const HeadPoseMap& m)
{
    if (m.dest == HeadPoseDest::Command) {
        const QString cmd = m.command.isEmpty() ? QStringLiteral("(none)") : m.command;
        const QChar unit = (m.source == HeadPoseAxis::X || m.source == HeadPoseAxis::Y
                            || m.source == HeadPoseAxis::Z)
                               ? QLatin1Char('c')
                               : QChar(0x00B0);
        QString unitStr = (unit == QLatin1Char('c')) ? QStringLiteral(" cm") : QString(unit);
        return QStringLiteral("%1 at %2%3").arg(cmd).arg(m.commandAt, 0, 'f', 0).arg(unitStr);
    }
    return QString::fromLatin1(headPoseDestLabel(m.dest));
}

double headPoseAxisValue(const HeadPose& pose, HeadPoseAxis axis)
{
    switch (axis) {
    case HeadPoseAxis::Yaw:
        return pose.yaw;
    case HeadPoseAxis::Pitch:
        return pose.pitch;
    case HeadPoseAxis::Roll:
        return pose.roll;
    case HeadPoseAxis::X:
        return pose.x;
    case HeadPoseAxis::Y:
        return pose.y;
    case HeadPoseAxis::Z:
        return pose.z;
    }
    return 0.0;
}

bool headPoseAxisValid(const HeadPose& pose, HeadPoseAxis axis)
{
    switch (axis) {
    case HeadPoseAxis::Yaw:
    case HeadPoseAxis::Pitch:
    case HeadPoseAxis::Roll:
        return pose.rotationValid;
    case HeadPoseAxis::X:
    case HeadPoseAxis::Y:
    case HeadPoseAxis::Z:
        return pose.positionValid;
    }
    return false;
}

void captureHeadPoseOrigin(HeadPose& origin, const HeadPose& pose)
{
    origin.yaw = pose.yaw;
    origin.pitch = pose.pitch;
    origin.roll = pose.roll;
    origin.x = pose.x;
    origin.y = pose.y;
    origin.z = pose.z;
    origin.timestampMs = pose.timestampMs;
    origin.rotationValid = true;
    origin.positionValid = true;
}

HeadPose relativeHeadPose(const HeadPose& pose, const HeadPose& origin)
{
    HeadPose out = pose;
    out.yaw = pose.yaw - origin.yaw;
    out.pitch = pose.pitch - origin.pitch;
    out.roll = pose.roll - origin.roll;
    out.x = pose.x - origin.x;
    out.y = pose.y - origin.y;
    out.z = pose.z - origin.z;
    return out;
}

double headPoseAxisRelative(const HeadPose& pose, const HeadPose& origin, bool originSet,
                            HeadPoseAxis axis)
{
    if (!headPoseAxisValid(pose, axis)) {
        return 0.0;
    }
    double raw = headPoseAxisValue(pose, axis);
    if (originSet) {
        raw -= headPoseAxisValue(origin, axis);
    }
    return raw;
}

double evalHeadPoseCurve(const QVector<HeadPoseCurvePoint>& points, double in)
{
    if (points.isEmpty()) {
        return 0.0;
    }
    if (points.size() == 1 || in <= points.first().in) {
        return points.first().out;
    }
    if (in >= points.last().in) {
        return points.last().out;
    }
    for (int i = 1; i < points.size(); ++i) {
        const HeadPoseCurvePoint& a = points[i - 1];
        const HeadPoseCurvePoint& b = points[i];
        if (in <= b.in) {
            const double span = b.in - a.in;
            if (span < 1e-12) {
                return b.out;
            }
            const double t = (in - a.in) / span;
            return a.out + t * (b.out - a.out);
        }
    }
    return points.last().out;
}

HeadPoseOutputs evalHeadPoseMaps(const QVector<HeadPoseMap>& maps, bool enabled, const HeadPose& pose,
                                 const HeadPose& origin, bool originSet, bool paused, qint64 dtMs,
                                 HeadPoseEvalState* state)
{
    HeadPoseOutputs out;
    const double dt = qBound(0.0, dtMs / 1000.0, 0.25);
    if (!enabled) {
        if (state) {
            state->mouseRemX = 0;
            state->mouseRemY = 0;
            state->scrollRemV = 0;
            state->scrollRemH = 0;
        }
        return out;
    }

    double mouseVx = 0.0;
    double mouseVy = 0.0;
    double scrollV = 0.0;
    double scrollH = 0.0;

    for (const HeadPoseMap& m : maps) {
        if (!m.enabled || m.id.isEmpty()) {
            continue;
        }
        const double raw = headPoseAxisRelative(pose, origin, originSet, m.source);
        const double mapped = evalHeadPoseCurve(m.points, raw);
        if (state) {
            state->lastSource.insert(m.id, raw);
        }

        if (m.dest == HeadPoseDest::Command) {
            if (!state) {
                continue;
            }
            const double at = m.commandAt;
            const double hyst = m.hysteresis;
            const bool beyond = at >= 0.0 ? (raw >= at) : (raw <= at);
            const bool release = at >= 0.0 ? (raw < at - hyst) : (raw > at + hyst);
            bool armed = state->commandArmed.value(m.id, false);
            if (release) {
                armed = false;
            }
            if (beyond && !armed && !paused && !m.command.isEmpty()) {
                out.commands.push_back(m.command);
                armed = true;
            }
            state->commandArmed.insert(m.id, armed);
            continue;
        }

        if (paused) {
            continue;
        }

        switch (m.dest) {
        case HeadPoseDest::MouseX:
            mouseVx += mapped;
            break;
        case HeadPoseDest::MouseY:
            mouseVy += mapped;
            break;
        case HeadPoseDest::ScrollV:
            scrollV += mapped;
            break;
        case HeadPoseDest::ScrollH:
            scrollH += mapped;
            break;
        case HeadPoseDest::GazeX:
            out.gazeOffset.rx() += mapped;
            break;
        case HeadPoseDest::GazeY:
            out.gazeOffset.ry() += mapped;
            break;
        case HeadPoseDest::JoyLX:
            out.joyLX = qBound(-1.0, mapped, 1.0);
            out.driveJoyLX = true;
            break;
        case HeadPoseDest::JoyLY:
            out.joyLY = qBound(-1.0, mapped, 1.0);
            out.driveJoyLY = true;
            break;
        case HeadPoseDest::JoyRX:
            out.joyRX = qBound(-1.0, mapped, 1.0);
            out.driveJoyRX = true;
            break;
        case HeadPoseDest::JoyRY:
            out.joyRY = qBound(-1.0, mapped, 1.0);
            out.driveJoyRY = true;
            break;
        case HeadPoseDest::Command:
            break;
        }
    }

    if (paused) {
        if (state) {
            state->mouseRemX = 0;
            state->mouseRemY = 0;
            state->scrollRemV = 0;
            state->scrollRemH = 0;
        }
        out.gazeOffset = {};
        return out;
    }

    auto integrate = [&](double vel, double* rem, double maxStep) {
        const double add = vel * dt;
        double acc = (rem ? *rem : 0.0) + add;
        acc = qBound(-maxStep, acc, maxStep);
        const int step = int(acc >= 0.0 ? std::floor(acc) : std::ceil(acc));
        if (rem) {
            *rem = acc - step;
        }
        return step;
    };

    out.mouseDx = integrate(mouseVx, state ? &state->mouseRemX : nullptr, kMaxMouseStep);
    out.mouseDy = integrate(mouseVy, state ? &state->mouseRemY : nullptr, kMaxMouseStep);
    const int sv = integrate(scrollV * kWheel, state ? &state->scrollRemV : nullptr, kMaxScrollDelta);
    const int sh = integrate(scrollH * kWheel, state ? &state->scrollRemH : nullptr, kMaxScrollDelta);
    out.scrollVDelta = sv;
    out.scrollHDelta = sh;
    return out;
}

} // namespace gazer
