#pragma once

#include "core/HeadPose.h"

#include <QString>
#include <QVector>
#include <QtGlobal>

namespace gazer {

inline constexpr int kMaxHeadPoseMaps = 12;
inline constexpr int kMaxHeadPoseCurvePoints = 12;

enum class HeadPoseAxis { Yaw, Pitch, Roll, X, Y, Z };

enum class HeadPoseDest {
    MouseX,
    MouseY,
    ScrollV,
    ScrollH,
    GazeX,
    GazeY,
    JoyLX,
    JoyLY,
    JoyRX,
    JoyRY,
    Command
};

struct HeadPoseCurvePoint {
    double in = 0.0;
    double out = 0.0;

    [[nodiscard]] bool operator==(const HeadPoseCurvePoint& o) const
    {
        return qFuzzyCompare(1.0 + in, 1.0 + o.in) && qFuzzyCompare(1.0 + out, 1.0 + o.out);
    }
};

struct HeadPoseMap {
    QString id;
    bool enabled = true;
    HeadPoseAxis source = HeadPoseAxis::Yaw;
    HeadPoseDest dest = HeadPoseDest::MouseX;
    QVector<HeadPoseCurvePoint> points;
    QString command;
    double commandAt = 15.0;
    double hysteresis = 2.0;

    [[nodiscard]] bool operator==(const HeadPoseMap& o) const
    {
        return id == o.id && enabled == o.enabled && source == o.source && dest == o.dest
               && points == o.points && command == o.command
               && qFuzzyCompare(1.0 + commandAt, 1.0 + o.commandAt)
               && qFuzzyCompare(1.0 + hysteresis, 1.0 + o.hysteresis);
    }
};

[[nodiscard]] const char* headPoseAxisId(HeadPoseAxis axis);
[[nodiscard]] const char* headPoseAxisLabel(HeadPoseAxis axis);
[[nodiscard]] HeadPoseAxis headPoseAxisFromId(const QString& id, bool* ok = nullptr);

[[nodiscard]] const char* headPoseDestId(HeadPoseDest dest);
[[nodiscard]] const char* headPoseDestLabel(HeadPoseDest dest);
[[nodiscard]] HeadPoseDest headPoseDestFromId(const QString& id, bool* ok = nullptr);
[[nodiscard]] const char* headPoseDestJoyAxis(HeadPoseDest dest);

[[nodiscard]] HeadPoseMap defaultHeadPoseMap();
void clampHeadPoseMap(HeadPoseMap& m);
[[nodiscard]] QString headPoseMapDestSummary(const HeadPoseMap& m);

[[nodiscard]] double headPoseAxisValue(const HeadPose& pose, HeadPoseAxis axis);
[[nodiscard]] bool headPoseAxisValid(const HeadPose& pose, HeadPoseAxis axis);
/// Snapshot every axis (yaw/pitch/roll/x/y/z) as the Recenter origin.
void captureHeadPoseOrigin(HeadPose& origin, const HeadPose& pose);
[[nodiscard]] HeadPose relativeHeadPose(const HeadPose& pose, const HeadPose& origin);
[[nodiscard]] double headPoseAxisRelative(const HeadPose& pose, const HeadPose& origin,
                                          bool originSet, HeadPoseAxis axis);

} // namespace gazer
