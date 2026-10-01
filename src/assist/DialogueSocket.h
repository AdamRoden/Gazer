#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

class QSslSocket;
class QTimer;

namespace gazer {

/// Result of reading one HTTP upgrade off the socket buffer.
struct DialogueUpgrade {
    enum class Status { NeedMore, Ready, Failed };
    Status status = Status::NeedMore;
    int httpStatus = 0;
    int retryAfterMs = 0;
    QString error;
    QByteArray rest;
};

/// One text-to-dialogue JSON message. Non-JSON payloads leave every field unset.
struct DialogueEvent {
    QByteArray pcm;
    bool isFinal = false;
    bool failed = false;
    int httpStatus = 0;
    int retryAfterMs = 0;
    QString error;
};

[[nodiscard]] DialogueUpgrade parseDialogueUpgrade(const QByteArray& rx);
[[nodiscard]] DialogueEvent parseDialogueEvent(const QByteArray& payload);

/// v4 Turbo text-to-dialogue WebSocket. GUI thread only.
/// Emits PCM as the server sends it, then `speechStreamEnded`.
class DialogueSocket final : public QObject {
    Q_OBJECT

public:
    explicit DialogueSocket(QObject* parent = nullptr);
    ~DialogueSocket() override;

    void start(const QString& apiKey, const QString& voiceId, const QString& text);
    void abort();

signals:
    void speechChunk(const QByteArray& pcm);
    void speechStreamEnded();
    void speechFailed(int httpStatus, const QString& error, int retryAfterMs);

private:
    void tearDownSocket();
    void failStream(int httpStatus, const QString& error, int retryAfterMs);
    void onSocketEncrypted();
    void onSocketReadyRead();
    void onSocketError();
    void sendStreamText(const QByteArray& json);
    void finishStream();
    void armIdleTimer();
    void parseUpgrade();
    void parseFrames();

    QSslSocket* m_socket = nullptr;
    QTimer* m_idle = nullptr;
    QTimer* m_keepAlive = nullptr;
    QByteArray m_rx;
    QByteArray m_textAcc;
    QByteArray m_wsKey;
    QString m_streamText;
    QString m_streamVoice;
    bool m_upgraded = false;
    bool m_aborting = false;
    bool m_streamDone = false;
};

} // namespace gazer
