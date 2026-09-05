#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

class QAudioDecoder;

namespace gazer {

/// Decode a clip, apply gain > 1, emit 16-bit PCM WAV. GUI thread.
class ClipBoost final : public QObject {
    Q_OBJECT

public:
    explicit ClipBoost(QObject* parent = nullptr);

    [[nodiscard]] bool isSupported() const;
    void start(const QString& path, double gain);
    void cancel();

signals:
    void finished(const QByteArray& wav);
    void failed();

private:
    void onBuffer();
    void onFinished();
    void onError();
    void fail();

    QAudioDecoder* m_decoder = nullptr;
    QByteArray m_pcm;
    int m_sampleRate = 0;
    int m_channels = 0;
    double m_gain = 1.0;
    quint64 m_gen = 0;
    bool m_running = false;
};

} // namespace gazer
