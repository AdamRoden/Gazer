#pragma once

#include "core/ITracker.h"

#include <QTimer>

namespace gazer {

/// Fallback tracker: system mouse cursor as gaze (~60 Hz).
class TrackerMouse final : public ITracker {
    Q_OBJECT

public:
    explicit TrackerMouse(QObject* parent = nullptr);

    bool start() override;
    void stop() override;
    [[nodiscard]] bool isRunning() const override;
    [[nodiscard]] QString name() const override;

private:
    void onTick();

    QTimer m_timer;
    qint64 m_elapsedMs = 0;
    bool m_running = false;
};

} // namespace gazer
