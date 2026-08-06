#pragma once

#include "core/GazePoint.h"

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QVector>

namespace gazer {

/// Hold gaze on one item; activate using a dwell-time sequence while gaze remains
/// (e.g. 600,300,100,600 ms). Steps advance until the last value, which then
/// repeats forever until leave. Replaces legacy single ms + repeatMs.
class DwellStateMachine final : public QObject {
    Q_OBJECT

public:
    explicit DwellStateMachine(QObject* parent = nullptr);

    void setEnabled(bool enabled);
    /// Single-step convenience (sequence of one).
    void setDwellMs(int ms);
    /// Progressive / repeating dwell steps in ms (cycled until leave).
    void setDwellSequence(const QVector<int>& msSteps);
    void setInvalidGraceMs(int ms);
    void reset();
    void leave();

    [[nodiscard]] bool isEnabled() const { return m_enabled; }
    [[nodiscard]] QString hoveredItemId() const { return m_currentId; }
    [[nodiscard]] double progress() const { return m_progress; }
    [[nodiscard]] int currentStepMs() const;

public slots:
    void onGazeSample(const gazer::GazePoint& point, const QString& itemIdUnderGaze);

signals:
    void hoverChanged(const QString& itemId);
    void dwellProgress(const QString& itemId, double progress);
    void itemActivated(const QString& itemId);

private:
    void clearHover();
    [[nodiscard]] int stepMsAt(int index) const;

    bool m_enabled = true;
    QVector<int> m_sequence = {800};
    int m_invalidGraceMs = 180;

    QString m_currentId;
    qint64 m_dwellStartMs = 0;
    double m_progress = 0.0;
    int m_stepIndex = 0;

    bool m_inInvalidGrace = false;
    QElapsedTimer m_invalidGraceClock;
};

} // namespace gazer
