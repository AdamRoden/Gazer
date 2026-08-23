#pragma once

#include "core/GazePoint.h"
#include "layout/InvalidGazeGrace.h"

#include <QObject>
#include <QString>
#include <QVector>

namespace gazer {

/// Hold gaze on one item; activate using a dwell-time sequence while gaze remains
/// (e.g. 800,600,400,200,100,50 ms). Steps advance until the last value, which
/// then repeats until leave. A brief miss / invalid sample does not rewind the
/// step index (blink / layout-refresh grace).
class DwellStateMachine final : public QObject {
    Q_OBJECT

public:
    explicit DwellStateMachine(QObject* parent = nullptr);

    void setEnabled(bool enabled);
    /// Single-step convenience (sequence of one).
    void setDwellMs(int ms);
    /// Progressive dwell steps in ms. 0 is legal (fire as soon as scan grace
    /// ends). Last step repeats while gaze holds; earlier steps do not wrap.
    void setDwellSequence(const QVector<int>& msSteps);
    void setInvalidGraceMs(int ms);
    /// Time on-target before dwell sequence / progress animation begins (ms).
    void setScanGraceMs(int ms);
    void reset();
    void leave();

    [[nodiscard]] bool isEnabled() const { return m_enabled; }
    [[nodiscard]] QString hoveredItemId() const { return m_currentId; }
    [[nodiscard]] double progress() const { return m_progress; }
    [[nodiscard]] int currentStepMs() const;
    [[nodiscard]] int scanGraceMs() const { return m_scanGraceMs; }
    [[nodiscard]] bool isScanGraceComplete() const
    {
        return m_scanGraceComplete && !m_currentId.isEmpty();
    }

    static constexpr int kDefaultInvalidGraceMs = 180;
    static constexpr int kDefaultScanGraceMs = 100;

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
    int m_scanGraceMs = kDefaultScanGraceMs;

    QString m_currentId;
    qint64 m_dwellStartMs = 0;
    double m_progress = 0.0;
    int m_stepIndex = 0;
    bool m_scanGraceComplete = false;
    InvalidGazeGrace m_invalidGrace;
};

} // namespace gazer
