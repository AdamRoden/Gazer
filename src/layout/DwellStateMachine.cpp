#include "layout/DwellStateMachine.h"

#include <QtGlobal>

namespace gazer {

DwellStateMachine::DwellStateMachine(QObject* parent)
    : QObject(parent)
{
}

void DwellStateMachine::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if (!m_enabled) {
        clearHover();
    }
}

void DwellStateMachine::setDwellMs(int ms)
{
    setDwellSequence({ms > 0 ? ms : 1});
}

void DwellStateMachine::setDwellSequence(const QVector<int>& msSteps)
{
    QVector<int> next;
    for (int ms : msSteps) {
        if (ms >= 0) {
            next.push_back(ms);
        }
    }
    if (next.isEmpty()) {
        next = {800};
    }
    if (next == m_sequence) {
        return;
    }
    m_sequence = std::move(next);
    if (m_stepIndex >= m_sequence.size()) {
        m_stepIndex = m_sequence.size() - 1;
    }
}

void DwellStateMachine::setInvalidGraceMs(int ms)
{
    m_invalidGrace.graceMs = qMax(0, ms);
}

void DwellStateMachine::setScanGraceMs(int ms)
{
    m_scanGraceMs = qMax(0, ms);
}

int DwellStateMachine::stepMsAt(int index) const
{
    if (m_sequence.isEmpty()) {
        return 800;
    }
    const int i = ((index % m_sequence.size()) + m_sequence.size()) % m_sequence.size();
    return m_sequence[i];
}

int DwellStateMachine::currentStepMs() const
{
    return stepMsAt(m_stepIndex);
}

void DwellStateMachine::reset()
{
    clearHover();
}

void DwellStateMachine::leave()
{
    clearHover();
}

void DwellStateMachine::clearHover()
{
    m_invalidGrace.reset();
    m_stepIndex = 0;
    m_scanGraceComplete = false;
    m_elapsedMs = 0;
    m_lastTs = 0;
    if (!m_currentId.isEmpty()) {
        m_currentId.clear();
        m_progress = 0.0;
        emit hoverChanged(QString());
        emit dwellProgress(QString(), 0.0);
    } else {
        m_progress = 0.0;
    }
}

void DwellStateMachine::afterActivation()
{
    m_elapsedMs = 0;
    if (m_rescanAfterStep && m_scanGraceMs > 0) {
        m_scanGraceComplete = false;
        m_progress = 1.0;
        emit dwellProgress(m_currentId, 1.0);
        return;
    }
    m_progress = 0.0;
    emit dwellProgress(m_currentId, 0.0);
}

void DwellStateMachine::onGazeSample(const gazer::GazePoint& point,
                                     const QString& itemIdUnderGaze)
{
    if (!m_enabled) {
        clearHover();
        return;
    }

    const qint64 now = point.timestampMs;

    // Empty hit (layout refresh, one-frame miss) uses the same grace as an
    // invalid sample so we do not rewind the sequence after each activation.
    if (!point.valid || itemIdUnderGaze.isEmpty()) {
        if (m_currentId.isEmpty()) {
            return;
        }
        if (m_invalidGrace.onInvalid(now) == InvalidGazeGrace::Result::Holding) {
            return;
        }
        clearHover();
        return;
    }

    if (m_invalidGrace.holding()) {
        m_lastTs = now;
    }
    m_invalidGrace.onValid();

    if (itemIdUnderGaze != m_currentId) {
        m_currentId = itemIdUnderGaze;
        m_elapsedMs = 0;
        m_lastTs = now;
        m_progress = 0.0;
        m_stepIndex = 0;
        m_scanGraceComplete = (m_scanGraceMs <= 0);
        emit hoverChanged(m_currentId);
        emit dwellProgress(m_currentId, 0.0);
        return;
    }

    const qint64 dt = now - m_lastTs;
    if (dt > 0) {
        m_elapsedMs += dt;
    }
    m_lastTs = now;

    // Scan grace: hold progress at 0 until the precursor time elapses, then
    // start the dwell sequence clock (and progress animation) from zero.
    if (!m_scanGraceComplete) {
        if (m_elapsedMs < m_scanGraceMs) {
            if (m_rescanAfterStep && m_progress >= 1.0) {
                return;
            }
            if (m_progress != 0.0) {
                m_progress = 0.0;
                emit dwellProgress(m_currentId, 0.0);
            }
            return;
        }
        m_scanGraceComplete = true;
        m_elapsedMs = 0;
        m_progress = 0.0;
        emit dwellProgress(m_currentId, 0.0);
        // Fall through. A 0-ms step fires on this sample (PageNotes leading 0).
    }

    while (true) {
        const int needMs = stepMsAt(m_stepIndex);
        if (needMs <= 0) {
            emit itemActivated(m_currentId);
            const bool more = m_stepIndex + 1 < m_sequence.size();
            if (more) {
                ++m_stepIndex;
            }
            afterActivation();
            if (more && m_scanGraceComplete) {
                continue;
            }
            break;
        }

        m_progress = qBound(0.0, static_cast<double>(m_elapsedMs) / static_cast<double>(needMs), 1.0);
        emit dwellProgress(m_currentId, m_progress);

        if (m_elapsedMs < needMs) {
            break;
        }
        emit itemActivated(m_currentId);
        if (m_stepIndex + 1 < m_sequence.size()) {
            ++m_stepIndex;
        }
        afterActivation();
        break;
    }
}

} // namespace gazer
