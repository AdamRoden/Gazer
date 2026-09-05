#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QJsonObject>
#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

namespace gazer {

/// GUI-thread ElevenLabs HTTP. Validate + TTS POST. Catalog cache on disk.
class ElevenClient final : public QObject {
    Q_OBJECT

public:
    static constexpr int kCacheTtlHours = 24;

    explicit ElevenClient(QObject* parent = nullptr);

    void validateApiKey(const QString& apiKey);
    void refreshCatalog(const QString& apiKey);
    void fetchSpeech(const QString& apiKey, const QString& voiceId, const QJsonObject& body);
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
    void speechReady(const QByteArray& mpeg);
    void speechFailed(int httpStatus, const QString& error, int retryAfterMs);

private:
    void startVoicesGet(const QString& apiKey, bool forValidate);
    void onValidateFinished();
    void onCatalogFinished();
    void onSpeakFinished();

    QNetworkAccessManager* m_nam = nullptr;
    QNetworkReply* m_validateReply = nullptr;
    QNetworkReply* m_catalogReply = nullptr;
    QNetworkReply* m_speakReply = nullptr;
};

} // namespace gazer
