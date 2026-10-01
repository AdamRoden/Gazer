#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QJsonObject>
#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

namespace gazer {

class DialogueSocket;

/// GUI-thread ElevenLabs voice catalog. Live speech is `DialogueSocket`.
class ElevenClient final : public QObject {
    Q_OBJECT

public:
    static constexpr int kCacheTtlHours = 24;

    explicit ElevenClient(QObject* parent = nullptr);

    void validateApiKey(const QString& apiKey);
    void refreshCatalog(const QString& apiKey);
    /// Forward one utterance to `DialogueSocket`.
    void startSpeech(const QString& apiKey, const QString& voiceId, const QString& text);
    void abortSpeech();

    [[nodiscard]] static QString speechDir();
    [[nodiscard]] static QString voicesCachePath();
    static bool saveVoicesCache(const QByteArray& getBody);
    [[nodiscard]] static QJsonObject loadVoicesCache();

    /// Testable cache helpers. `getBody` is the raw GET /v1/voices JSON.
    static bool writeVoicesCacheAt(const QString& path, const QByteArray& getBody,
                                   const QDateTime& now);
    [[nodiscard]] static QJsonObject readVoicesCacheAt(const QString& path);
    [[nodiscard]] static bool voicesCacheFresh(const QJsonObject& cache,
                                               const QDateTime& now = QDateTime::currentDateTimeUtc());

signals:
    void keyValidated(bool ok, const QString& error);
    void catalogReady(bool ok, const QString& error);
    void speechChunk(const QByteArray& pcm);
    void speechStreamEnded();
    void speechFailed(int httpStatus, const QString& error, int retryAfterMs);

private:
    void startVoicesGet(const QString& apiKey, bool forValidate);
    void onValidateFinished();
    void onCatalogFinished();

    QNetworkAccessManager* m_nam = nullptr;
    QNetworkReply* m_validateReply = nullptr;
    QNetworkReply* m_catalogReply = nullptr;
    DialogueSocket* m_dialogue = nullptr;
};

} // namespace gazer
