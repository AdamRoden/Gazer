#pragma once

#include "core/GazePoint.h"

#include <QObject>
#include <QString>

namespace gazer {

/// Abstract eye-tracker backend.
///
/// Implementations:
///   - TrackerTobii  — Stream Engine (runtime DLL)
///   - TrackerMouse  — cursor fallback when Tobii is unavailable
class ITracker : public QObject {
    Q_OBJECT

public:
    explicit ITracker(QObject* parent = nullptr) : QObject(parent) {}
    ~ITracker() override = default;

    /// Begin streaming gaze samples. Returns false if hardware/SDK unavailable.
    [[nodiscard]] virtual bool start() = 0;
    virtual void stop() = 0;
    [[nodiscard]] virtual bool isRunning() const = 0;
    [[nodiscard]] virtual QString name() const = 0;

signals:
    void gazeUpdated(const gazer::GazePoint& point);
    void trackingLost();
    void trackingRestored();
};

} // namespace gazer
