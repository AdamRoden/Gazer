#include "assist/HeadPoseMapper.h"

#include "input/InputService.h"
#include "input/MouseInjector.h"
#include "input/VirtualGamepad.h"

namespace gazer {

HeadPoseMapper::HeadPoseMapper(QObject* parent)
    : QObject(parent)
{
}

void HeadPoseMapper::setEnabled(bool on)
{
    if (m_enabled == on) {
        return;
    }
    m_enabled = on;
    if (!on) {
        m_gazeOffset = {};
        m_state = {};
        zeroJoystick();
    }
}

void HeadPoseMapper::setMaps(QVector<HeadPoseMap> maps)
{
    if (maps.size() > kMaxHeadPoseMaps) {
        maps.resize(kMaxHeadPoseMaps);
    }
    for (HeadPoseMap& m : maps) {
        clampHeadPoseMap(m);
    }
    m_maps = std::move(maps);
}

void HeadPoseMapper::setOrigin(const HeadPose& origin, bool set)
{
    m_origin = origin;
    m_originSet = set;
    m_state.commandArmed.clear();
}

void HeadPoseMapper::setPaused(bool paused)
{
    if (m_paused == paused) {
        return;
    }
    m_paused = paused;
    if (paused) {
        m_gazeOffset = {};
        m_state.mouseRemX = 0;
        m_state.mouseRemY = 0;
        m_state.scrollRemV = 0;
        m_state.scrollRemH = 0;
        zeroJoystick();
    }
}

void HeadPoseMapper::onTrackingLost()
{
    m_last.positionValid = false;
    m_last.rotationValid = false;
    m_gazeOffset = {};
    m_lastTs = -1;
    zeroJoystick();
}

void HeadPoseMapper::onPose(const HeadPose& pose)
{
    m_last.timestampMs = pose.timestampMs;
    if (pose.rotationValid) {
        m_last.yaw = pose.yaw;
        m_last.pitch = pose.pitch;
        m_last.roll = pose.roll;
        m_last.rotationValid = true;
    }
    if (pose.positionValid) {
        m_last.x = pose.x;
        m_last.y = pose.y;
        m_last.z = pose.z;
        m_last.positionValid = true;
    }
    qint64 dt = 16;
    if (m_lastTs >= 0 && pose.timestampMs > m_lastTs) {
        dt = pose.timestampMs - m_lastTs;
    }
    m_lastTs = pose.timestampMs;

    const HeadPoseOutputs out =
        evalHeadPoseMaps(m_maps, m_enabled, pose, m_origin, m_originSet, m_paused, dt, &m_state);
    apply(out);
}

HeadPose HeadPoseMapper::displayPose() const
{
    return m_originSet ? relativeHeadPose(m_last, m_origin) : m_last;
}

double HeadPoseMapper::axisRelative(HeadPoseAxis axis) const
{
    return headPoseAxisRelative(m_last, m_origin, m_originSet, axis);
}

void HeadPoseMapper::apply(const HeadPoseOutputs& out)
{
    m_gazeOffset = out.gazeOffset;

    if (out.mouseDx != 0 || out.mouseDy != 0) {
        QString err;
        (void)MouseInjector::moveBy(out.mouseDx, out.mouseDy, &err);
    }
    if (out.scrollVDelta != 0) {
        QString err;
        (void)MouseInjector::scrollDelta(out.scrollVDelta, &err);
    }
    if (out.scrollHDelta != 0) {
        QString err;
        (void)MouseInjector::scrollHorizontalDelta(out.scrollHDelta, &err);
    }

    auto setAxis = [&](bool drive, bool* was, const char* axis, double value) {
        if (!drive && !*was) {
            return;
        }
        if (!m_input) {
            *was = drive;
            return;
        }
        QString err;
        const double v = drive ? value : 0.0;
        if (!m_input->gamepad().setAxis(QLatin1String(axis), v, &err)) {
            if (!m_joyWarned && m_notify) {
                m_joyWarned = true;
                m_notify(err);
            }
        }
        *was = drive;
    };
    setAxis(out.driveJoyLX, &m_driveJoyLX, "lx", out.joyLX);
    setAxis(out.driveJoyLY, &m_driveJoyLY, "ly", out.joyLY);
    setAxis(out.driveJoyRX, &m_driveJoyRX, "rx", out.joyRX);
    setAxis(out.driveJoyRY, &m_driveJoyRY, "ry", out.joyRY);

    if (m_runCommand) {
        for (const QString& cmd : out.commands) {
            QString err;
            if (!m_runCommand(cmd, &err) && m_notify) {
                m_notify(err.isEmpty() ? QStringLiteral("Head pose command failed") : err);
            }
        }
    }
}

void HeadPoseMapper::zeroJoystick()
{
    if (!m_input) {
        m_driveJoyLX = m_driveJoyLY = m_driveJoyRX = m_driveJoyRY = false;
        return;
    }
    QString err;
    if (m_driveJoyLX) {
        (void)m_input->gamepad().setAxis(QStringLiteral("lx"), 0.0, &err);
    }
    if (m_driveJoyLY) {
        (void)m_input->gamepad().setAxis(QStringLiteral("ly"), 0.0, &err);
    }
    if (m_driveJoyRX) {
        (void)m_input->gamepad().setAxis(QStringLiteral("rx"), 0.0, &err);
    }
    if (m_driveJoyRY) {
        (void)m_input->gamepad().setAxis(QStringLiteral("ry"), 0.0, &err);
    }
    m_driveJoyLX = m_driveJoyLY = m_driveJoyRX = m_driveJoyRY = false;
}

} // namespace gazer
