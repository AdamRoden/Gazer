#pragma once

#include "mapping/HeadPoseTypes.h"

#include <QHash>
#include <QPointF>
#include <QString>
#include <QVector>

namespace gazer {

[[nodiscard]] double evalHeadPoseCurve(const QVector<HeadPoseCurvePoint>& points, double in);

struct HeadPoseEvalState {
    QHash<QString, double> lastSource;
    QHash<QString, bool> commandArmed;
    double mouseRemX = 0.0;
    double mouseRemY = 0.0;
    double scrollRemV = 0.0;
    double scrollRemH = 0.0;
};

struct HeadPoseOutputs {
    int mouseDx = 0;
    int mouseDy = 0;
    int scrollVDelta = 0; // raw wheel units (120 = one notch)
    int scrollHDelta = 0;
    QPointF gazeOffset;
    double joyLX = 0.0;
    double joyLY = 0.0;
    double joyRX = 0.0;
    double joyRY = 0.0;
    bool driveJoyLX = false;
    bool driveJoyLY = false;
    bool driveJoyRX = false;
    bool driveJoyRY = false;
    QStringList commands;
};

/// Pure analog eval. @p paused zeros outputs, still updates command arming, and
/// clears mouse/scroll remainders so unpause does not dump a step. @p dtMs used
/// for mouse/scroll integration.
[[nodiscard]] HeadPoseOutputs evalHeadPoseMaps(const QVector<HeadPoseMap>& maps, bool enabled,
                                               const HeadPose& pose, const HeadPose& origin,
                                               bool originSet, bool paused, qint64 dtMs,
                                               HeadPoseEvalState* state);

} // namespace gazer
