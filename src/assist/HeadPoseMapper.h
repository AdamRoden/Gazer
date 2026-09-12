#pragma once

#include "core/HeadPose.h"
#include "mapping/HeadPoseCurve.h"
#include "mapping/HeadPoseTypes.h"

#include <QObject>
#include <QPointF>
#include <QString>
#include <QVector>
#include <functional>

namespace gazer {

class InputService;

/// Analog head-pose maps: tracker stream → mouse/scroll/gaze offset/joystick/commands.
class HeadPoseMapper final : public QObject {
    Q_OBJECT

public:
    using NotifyFn = std::function<void(const QString&)>;
    using RunCommandFn = std::function<bool(const QString&, QString*)>;

    explicit HeadPoseMapper(QObject* parent = nullptr);

    void setInput(InputService* input) { m_input = input; }
    void setRunCommand(RunCommandFn fn) { m_runCommand = std::move(fn); }
    void setNotifyFn(NotifyFn fn) { m_notify = std::move(fn); }

    void setEnabled(bool on);
    [[nodiscard]] bool isEnabled() const { return m_enabled; }
    void setMaps(QVector<HeadPoseMap> maps);
    void setOrigin(const HeadPose& origin, bool set);
    [[nodiscard]] bool originSet() const { return m_originSet; }
    void setPaused(bool paused);
    [[nodiscard]] bool isPaused() const { return m_paused; }

    void onPose(const HeadPose& pose);
    void onTrackingLost();

    [[nodiscard]] HeadPose lastPose() const { return m_last; }
    [[nodiscard]] HeadPose displayPose() const;
    [[nodiscard]] double axisRelative(HeadPoseAxis axis) const;
    [[nodiscard]] QPointF gazeOffset() const { return m_gazeOffset; }

signals:
    void outputsApplied();

private:
    void apply(const HeadPoseOutputs& out);
    void zeroJoystick();

    InputService* m_input = nullptr;
    RunCommandFn m_runCommand;
    NotifyFn m_notify;
    bool m_enabled = false;
    bool m_paused = false;
    bool m_originSet = false;
    bool m_joyWarned = false;
    HeadPose m_origin;
    HeadPose m_last;
    QVector<HeadPoseMap> m_maps;
    HeadPoseEvalState m_state;
    QPointF m_gazeOffset;
    qint64 m_lastTs = -1;
    bool m_driveJoyLX = false;
    bool m_driveJoyLY = false;
    bool m_driveJoyRX = false;
    bool m_driveJoyRY = false;
};

} // namespace gazer
