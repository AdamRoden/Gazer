#pragma once

#include "assist/PcmRate.h"

#include <QElapsedTimer>
#include <QObject>

class QAudioSink;
class QIODevice;
class QTimer;

namespace gazer {

/// Plays int16 mono PCM as it arrives. GUI thread only.
class PcmStreamPlayer final : public QObject {
    Q_OBJECT

public:
    explicit PcmStreamPlayer(QObject* parent = nullptr);
    ~PcmStreamPlayer() override;

    /// `speed` matches QMediaPlayer playback rate. `gain` is the 1–5× clip boost.
    [[nodiscard]] bool start(int sampleRate, double speed, double gain);
    void push(const QByteArray& pcm);
    /// No more audio. Emits `stopped` after the device drains.
    void finish();
    void stop();
    [[nodiscard]] bool isPlaying() const { return m_playing; }
    /// Samples the sink accepted. Survives `stop` so a failure slot can read it.
    [[nodiscard]] qint64 bytesAccepted() const { return m_bytesAccepted; }

signals:
    void started();
    void stopped();
    void failed(const QString& error);

private:
    void pump();
    void stopQuiet();
    void complete();
    void fail(const QString& error);

    QAudioSink* m_sink = nullptr;
    QIODevice* m_device = nullptr;
    QTimer* m_pump = nullptr;
    PcmRate::Resampler m_rate;
    QByteArray m_pending;
    QElapsedTimer m_finishClock;
    qint64 m_bytesAccepted = 0;
    int m_bytesPerSec = 1;
    int m_finishBudgetMs = 0;
    double m_speed = 1.0;
    double m_gain = 1.0;
    bool m_playing = false;
    bool m_ending = false;
    bool m_suppress = false;
};

} // namespace gazer
