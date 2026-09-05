#pragma once

#include <QObject>
#include <QString>

class QAudioOutput;
class QMediaPlayer;

namespace gazer {

/// GUI-thread MPEG/WAV playback. If the backend is missing, play() returns false
/// and the caller may SAPI-fall back. Do not construct off the GUI thread.
class ClipPlayer final : public QObject {
    Q_OBJECT

public:
    explicit ClipPlayer(QObject* parent = nullptr);
    ~ClipPlayer() override;

    [[nodiscard]] bool isAvailable() const;
    /// Start playing `path`. Returns false if the backend/file cannot start.
    /// playbackRate maps Eleven localSpeed; pitch is not applied in v1.
    [[nodiscard]] bool play(const QString& path, double playbackRate = 1.0);
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

    QMediaPlayer* m_player = nullptr;
    QAudioOutput* m_audio = nullptr;
    bool m_backendOk = false;
    bool m_playing = false;
    bool m_armed = false;
    bool m_suppressSignals = false;
};

} // namespace gazer
