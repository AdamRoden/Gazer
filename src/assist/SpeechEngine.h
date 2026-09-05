#pragma once

#include "assist/ElevenRequest.h"
#include "assist/TtsService.h"

#include <QObject>
#include <QString>

namespace gazer {

class AppSettings;
class ClipPlayer;
class ElevenClient;
class SpeechSecrets;

enum class SpeakKind { Canned, Composed };

/// Resolves canned vs composer speech. Canned is always SAPI. Composed uses
/// ElevenLabs when model+key+voiceId are set, then SAPI-falls back.
class SpeechEngine final : public QObject {
    Q_OBJECT

public:
    enum class Backend { Sapi, Eleven };
    struct Status {
        bool busy = false;
        bool speaking = false;
        QString lastError;
    };
    struct LastClip {
        QString path;
        QString phrase;
    };

    SpeechEngine(TtsService& tts, AppSettings& settings, SpeechSecrets& secrets,
                 ElevenClient& eleven, ClipPlayer& clips, QObject* parent = nullptr);
    ~SpeechEngine() override;

    void speak(const QString& phrase, SpeakKind kind, bool recordHistory = true);
    void stop();
    void previewCurrent();
    void previewVoice(const QString& voiceId);
    [[nodiscard]] bool playFile(const QString& path);

    [[nodiscard]] Status status() const { return m_status; }
    [[nodiscard]] Backend lastAttemptedBackend() const { return m_lastBackend; }
    [[nodiscard]] LastClip lastClip() const { return m_lastClip; }
    [[nodiscard]] bool elevenLatched() const { return m_elevenLatched; }
    /// Point `lastClip` at the history/clips copy so tmp MPEG can be deleted.
    void keepGeneratedClip(const QString& stablePath);

signals:
    void statusChanged();
    void finished();
    void failed(const QString& error);
    void notify(const QString& message);
    void historyReady(const QString& phrase, const QString& backend, const QString& modelId,
                      const QString& voiceId, const QString& mpegPath);

private:
    void cancelInFlight();
    void setIdle();
    void speakSapi(const QString& spoken);
    void startEleven(const QString& phrase, const QString& voiceId);
    void fallbackSapi(const QString& spoken);
    bool writeMpegTemp(const QByteArray& mpeg, QString* pathOut);
    void playMpeg(const QString& path, double localSpeed, const QString& spokenFallback);
    void latchIfNeeded();
    void emitHistoryIfNeeded(const QString& backend, const QString& modelId,
                             const QString& voiceId);
    void removeIfTemp(const QString& path);
    void discardTmpIfUnretained();
    void setLastClip(const QString& path, const QString& phrase);
    void maybeResetElevenLatch();
    [[nodiscard]] QString elevenConfigFingerprint() const;
    [[nodiscard]] QString tmpMpegPath() const;

    void onTtsStarted();
    void onTtsFinished();
    void onTtsFailed(const QString& error);
    void onSpeechReady(const QByteArray& mpeg);
    void onSpeechFailed(int httpStatus, const QString& error, int retryAfterMs);
    void onClipStarted();
    void onClipStopped();
    void onClipFailed(const QString& error);

    TtsService& m_tts;
    AppSettings& m_settings;
    SpeechSecrets& m_secrets;
    ElevenClient& m_eleven;
    ClipPlayer& m_clips;

    Status m_status;
    Backend m_lastBackend = Backend::Sapi;
    SpeakKind m_kind = SpeakKind::Canned;
    quint64 m_generation = 0;
    quint64 m_sapiGen = 0;
    quint64 m_elevenGen = 0;
    quint64 m_clipGen = 0;
    bool m_retried429 = false;
    bool m_elevenLatched = false;
    int m_elevenFails = 0;
    QString m_elevenConfigFp;
    QString m_pendingSpoken;
    QString m_pendingVoiceId;
    ElevenRequest::Prepared m_pendingPrep;
    LastClip m_lastClip;
    QString m_tmpMpeg;
    bool m_recordHistory = false;
    QString m_historyPhrase;
};

} // namespace gazer
