#pragma once

#include "core/ITracker.h"

namespace gazer {

/// Test seam. Emits scripted gaze and head samples and the same loss signals
/// the Tobii worker posts. No hardware and no Stream Engine DLL.
class FakeTracker final : public ITracker {
    Q_OBJECT

public:
    explicit FakeTracker(QObject* parent = nullptr);

    bool start() override;
    void stop() override;
    [[nodiscard]] bool isRunning() const override;
    [[nodiscard]] QString name() const override;

    void emitGaze(const GazePoint& point);
    void emitHead(const HeadPose& pose);
    void emitGazeStale();
    void emitHeadStale();
    void emitDisconnected();
    void emitReconnected();

signals:
    void gazeStreamStale();
    void headStreamStale();
    void deviceDisconnected();
    void deviceReconnected();

private:
    bool m_running = false;
};

} // namespace gazer
