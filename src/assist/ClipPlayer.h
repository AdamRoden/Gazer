#pragma once

#include <QObject>
#include <QString>

class QAudioOutput;
class QMediaPlayer;
class QTemporaryFile;

namespace gazer {

class ClipBoost;

/// GUI-thread MPEG/WAV playback. If the backend is missing, play() returns false
/// and the caller may SAPI-fall back. Do not construct off the GUI thread.
class ClipPlayer final : public QObject {
    Q_OBJECT

public:
    explicit ClipPlayer(QObject* parent = nullptr);
    ~ClipPlayer() override;

    [[nodiscard]] bool isAvailable() const;
    /// Start playing `path`. Returns false if the backend/file cannot start.
    /// playbackRate maps Eleven localSpeed; gain > 1 preprocesses PCM then plays.
    [[nodiscard]] bool play(const QString& path, double playbackRate = 1.0, double gain = 1.0);
    void stop();
    [[nodiscard]] bool isPlaying() const;

signals:
    void started();
    void stopped();
    void failed(const QString& error);

private:
    void onStateChanged();
    void onError();
    void onMediaStatus();
    void onBoostFinished(const QByteArray& wav);
    void onBoostFailed();
    void playUnboostedOrFail();
    void stopQuiet();
    [[nodiscard]] bool startFile(const QString& path, double playbackRate);

    QMediaPlayer* m_player = nullptr;
    QAudioOutput* m_audio = nullptr;
    ClipBoost* m_boost = nullptr;
    QTemporaryFile* m_gainFile = nullptr;
    QString m_sourcePath;
    double m_rate = 1.0;
    bool m_backendOk = false;
    bool m_playing = false;
    bool m_armed = false;
    bool m_boosting = false;
    bool m_suppressSignals = false;
};

} // namespace gazer
