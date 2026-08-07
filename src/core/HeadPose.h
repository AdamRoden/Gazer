#pragma once

#include <QtGlobal>

namespace gazer {

/// Head pose sample (OpenTrack-friendly units after conversion).
struct HeadPose {
    double yaw = 0.0;   // deg
    double pitch = 0.0; // deg
    double roll = 0.0;  // deg
    double x = 0.0;     // cm
    double y = 0.0;     // cm
    double z = 0.0;     // cm
    qint64 timestampMs = 0;
    bool positionValid = false;
    bool rotationValid = false;

    [[nodiscard]] bool valid() const { return positionValid || rotationValid; }
};

} // namespace gazer
