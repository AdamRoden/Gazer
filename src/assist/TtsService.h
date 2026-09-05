#pragma once

#include <QObject>
#include <QString>
#include <QVector>

class QTimer;

namespace gazer {

/// Text-to-speech. Windows: SAPI ISpVoice (async). Other platforms: log-only stub.
class TtsService final : public QObject {
    Q_OBJECT

public:
    struct VoiceInfo {
        QString token;
        QString name;
        QString gender;
        QString language;
    };

    explicit TtsService(QObject* parent = nullptr);
    ~TtsService() override;

    [[nodiscard]] bool isAvailable() const { return m_available; }
    [[nodiscard]] bool isSpeaking() const { return m_speaking; }
    [[nodiscard]] bool speak(const QString& text, QString* error = nullptr);
    /// Map 0.5–2.0 onto SAPI SetRate (−10…10). No-op off Windows.
    void setSpeed(double speed);
    void stop();
    [[nodiscard]] QVector<VoiceInfo> listVoices() const;
    bool setVoiceToken(const QString& token, QString* error = nullptr);
    [[nodiscard]] QString voiceToken() const { return m_voiceToken; }

signals:
    void started(const QString& text);
    void finished();
    void failed(const QString& error);

private slots:
    void onSapiNotify();
    void pollSapiStatus();

private:
    void attachNotify();
    void emitFinishedIfSpeaking();
    void drainEndEvents();

    bool m_available = false;
    bool m_speaking = false;
    QString m_voiceToken;
    bool m_seenSpeaking = false;
    int m_pollTicks = 0;
    void* m_voice = nullptr;      // ISpVoice* on Windows
    void* m_notifySink = nullptr; // ISpNotifySink* on Windows
    unsigned long m_stream = 0;
    QTimer* m_poll = nullptr;
};

} // namespace gazer
