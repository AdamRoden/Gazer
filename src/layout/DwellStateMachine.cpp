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
        if (ms > 0) {
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
    if (!m_currentId.isEmpty()) {
        m_currentId.clear();
        m_progress = 0.0;
        emit hoverChanged(QString());
        emit dwellProgress(QString(), 0.0);
    } else {
        m_progress = 0.0;
    }
}

void DwellStateMachine::onGazeSample(const gazer::GazePoint& point,
                                     const QString& itemIdUnderGaze)
{
    if (!m_enabled) {
        clearHover();
        return;
    }

    // Empty hit (layout refresh, one-frame miss) uses the same grace as an
    // invalid sample so we do not rewind the sequence after each activation.
    if (!point.valid || itemIdUnderGaze.isEmpty()) {
        if (m_currentId.isEmpty()) {
            return;
        }
        if (m_invalidGrace.onInvalid() == InvalidGazeGrace::Result::Holding) {
            return;
        }
        clearHover();
        return;
    }

    m_invalidGrace.onValid();

    if (itemIdUnderGaze != m_currentId) {
        m_currentId = itemIdUnderGaze;
        m_dwellStartMs = point.timestampMs;
        m_progress = 0.0;
        m_stepIndex = 0;
        m_scanGraceComplete = (m_scanGraceMs <= 0);
        emit hoverChanged(m_currentId);
        emit dwellProgress(m_currentId, 0.0);
        return;
    }

    qint64 elapsed = point.timestampMs - m_dwellStartMs;

    // Scan grace: hold progress at 0 until the precursor time elapses, then
    // start the dwell sequence clock (and progress animation) from zero.
    if (!m_scanGraceComplete) {
        if (elapsed < m_scanGraceMs) {
            if (m_progress != 0.0) {
                m_progress = 0.0;
                emit dwellProgress(m_currentId, 0.0);
            }
            return;
        }
        m_scanGraceComplete = true;
        m_dwellStartMs = point.timestampMs;
        elapsed = 0;
        m_progress = 0.0;
        // Fall through with elapsed == 0 so the first post-grace sample does not
        // immediately activate (needMs is always > 0).
        emit dwellProgress(m_currentId, 0.0);
    }

    const int needMs = stepMsAt(m_stepIndex);
    m_progress = qBound(0.0, static_cast<double>(elapsed) / static_cast<double>(needMs), 1.0);
    emit dwellProgress(m_currentId, m_progress);

    if (elapsed >= needMs) {
        // Fire this step, then advance toward the last step. The final value
        // is held forever (no wrap back to the first).
        if (m_stepIndex + 1 < m_sequence.size()) {
            ++m_stepIndex;
        }
        m_dwellStartMs = point.timestampMs;
        m_progress = 0.0;
        emit dwellProgress(m_currentId, 0.0);
        emit itemActivated(m_currentId);
    }
}

} // namespace gazer
