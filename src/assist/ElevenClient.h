#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;
class QNetworkReply;

namespace gazer {

class DialogueSocket;

/// GUI-thread ElevenLabs voice catalog and speech transport.
/// One untagged identity phrase uses `DialogueSocket`. Several breaths, or a
/// speed or pitch that is not 1, use `ElevenHttpStream`.
class ElevenClient final : public QObject {
    Q_OBJECT

public:
    static constexpr int kCacheTtlHours = 24;

    explicit ElevenClient(QObject* parent = nullptr);

    void validateApiKey(const QString& apiKey);
    void refreshCatalog(const QString& apiKey);
    /// One text-to-dialogue WebSocket utterance.
    void startSpeech(const QString& apiKey, const QString& voiceId, const QString& text);
    /// One HTTP PCM stream per breath, prefetching the next.
    void startPhrases(const QString& apiKey, const QString& voiceId, const QStringList& phrases);
    void abortSpeech();

    [[nodiscard]] static QString speechDir();
    [[nodiscard]] static QString voicesCachePath();
    static bool saveVoicesCache(const QByteArray& getBody);
    [[nodiscard]] static QJsonObject loadVoicesCache();

    /// Testable cache helpers. `getBody` is JSON with a `voices` array.
    /// Written caches are marked `catalog` `v2`.
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
    void beginVoicesWalk(const QString& apiKey, bool forValidate);
    void fetchVoicesPage();
    void onVoicesPageFinished();
    void finishVoicesPass();
    void failVoicesWalk(const QString& error);

    QNetworkAccessManager* m_nam = nullptr;
    QNetworkReply* m_pageReply = nullptr;
    DialogueSocket* m_dialogue = nullptr;
    class ElevenHttpStream* m_http = nullptr;
    int m_walkGen = 0;
    bool m_walkValidate = false;
    bool m_walkDefaults = true;
    int m_walkPages = 0;
    QString m_walkKey;
    QString m_walkToken;
    QString m_walkPrevToken;
    QJsonArray m_walkVoices;
    QStringList m_walkIds;
};

} // namespace gazer
