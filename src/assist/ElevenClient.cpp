#include "assist/ElevenClient.h"

#include "assist/DialogueSocket.h"
#include "assist/ElevenHttp.h"
#include "assist/ElevenRequest.h"
#include "utils/AtomicFile.h"
#include "utils/Log.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

namespace gazer {

ElevenClient::ElevenClient(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_dialogue(new DialogueSocket(this))
    , m_http(new ElevenHttpStream(m_nam, this))
{
    connect(m_dialogue, &DialogueSocket::speechChunk, this, &ElevenClient::speechChunk);
    connect(m_dialogue, &DialogueSocket::speechStreamEnded, this, &ElevenClient::speechStreamEnded);
    connect(m_dialogue, &DialogueSocket::speechFailed, this, &ElevenClient::speechFailed);
    connect(m_http, &ElevenHttpStream::speechChunk, this, &ElevenClient::speechChunk);
    connect(m_http, &ElevenHttpStream::speechStreamEnded, this, &ElevenClient::speechStreamEnded);
    connect(m_http, &ElevenHttpStream::speechFailed, this, &ElevenClient::speechFailed);
}

QString ElevenClient::speechDir()
{
    const QString dir =
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
            .filePath(QStringLiteral("speech"));
    QDir().mkpath(dir);
    return dir;
}

QString ElevenClient::voicesCachePath()
{
    return QDir(speechDir()).filePath(QStringLiteral("voices-cache.json"));
}

bool ElevenClient::writeVoicesCacheAt(const QString& path, const QByteArray& getBody,
                                      const QDateTime& now)
{
    if (path.isEmpty() || getBody.isEmpty()) {
        return false;
    }
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(getBody, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }
    const QJsonObject body = doc.object();
    if (!body.contains(QStringLiteral("voices")) || !body.value(QStringLiteral("voices")).isArray()) {
        return false;
    }
    QJsonObject root;
    root.insert(QStringLiteral("fetchedAt"), now.toUTC().toString(Qt::ISODate));
    root.insert(QStringLiteral("catalog"), QStringLiteral("v2"));
    root.insert(QStringLiteral("voices"), body.value(QStringLiteral("voices")));
    const QString parent = QFileInfo(path).absolutePath();
    if (!parent.isEmpty()) {
        QDir().mkpath(parent);
    }
    return writeFileAtomically(path, QJsonDocument(root).toJson(QJsonDocument::Compact));
}

QJsonObject ElevenClient::readVoicesCacheAt(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        return {};
    }
    return doc.object();
}

bool ElevenClient::voicesCacheFresh(const QJsonObject& cache, const QDateTime& now)
{
    if (cache.isEmpty() || !cache.contains(QStringLiteral("voices"))) {
        return false;
    }
    if (cache.value(QStringLiteral("catalog")).toString() != QLatin1String("v2")) {
        return false;
    }
    const QDateTime at =
        QDateTime::fromString(cache.value(QStringLiteral("fetchedAt")).toString(), Qt::ISODate);
    if (!at.isValid()) {
        return false;
    }
    return at.toUTC().msecsTo(now.toUTC()) < qint64(kCacheTtlHours) * 3600 * 1000
           && at.toUTC().msecsTo(now.toUTC()) >= 0;
}

bool ElevenClient::saveVoicesCache(const QByteArray& getBody)
{
    return writeVoicesCacheAt(voicesCachePath(), getBody, QDateTime::currentDateTimeUtc());
}

QJsonObject ElevenClient::loadVoicesCache()
{
    return readVoicesCacheAt(voicesCachePath());
}

void ElevenClient::failVoicesWalk(const QString& error)
{
    const bool validate = m_walkValidate;
    ++m_walkGen;
    m_walkKey.clear();
    if (validate) {
        emit keyValidated(false, error);
    } else {
        emit catalogReady(false, error);
    }
}

void ElevenClient::beginVoicesWalk(const QString& apiKey, bool forValidate)
{
    const QString key = apiKey.trimmed();
    if (key.isEmpty()) {
        if (forValidate) {
            emit keyValidated(false, QStringLiteral("Empty key"));
        } else {
            emit catalogReady(false, QStringLiteral("Empty key"));
        }
        return;
    }
    ++m_walkGen;
    if (m_pageReply) {
        QNetworkReply* prev = m_pageReply;
        m_pageReply = nullptr;
        prev->disconnect(this);
        prev->abort();
        prev->deleteLater();
    }
    m_walkValidate = forValidate;
    m_walkDefaults = true;
    m_walkPages = 0;
    m_walkKey = key;
    m_walkToken.clear();
    m_walkPrevToken.clear();
    m_walkVoices = {};
    m_walkIds.clear();
    fetchVoicesPage();
}

void ElevenClient::fetchVoicesPage()
{
    QUrl url(ElevenRequest::kVoicesUrl.toString());
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("page_size"), QStringLiteral("100"));
    query.addQueryItem(QStringLiteral("include_total_count"), QStringLiteral("false"));
    if (m_walkDefaults) {
        query.addQueryItem(QStringLiteral("voice_type"), QStringLiteral("default"));
    }
    if (!m_walkToken.isEmpty()) {
        query.addQueryItem(QStringLiteral("next_page_token"), m_walkToken);
    }
    url.setQuery(query);
    QNetworkRequest req(url);
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("xi-api-key", m_walkKey.toUtf8());
    m_pageReply = m_nam->get(req);
    m_pageReply->setProperty("walkGen", m_walkGen);
    QTimer::singleShot(ElevenRequest::kVoicesTimeoutMs, m_pageReply, [reply = m_pageReply]() {
        if (reply) {
            reply->abort();
        }
    });
    connect(m_pageReply, &QNetworkReply::finished, this, &ElevenClient::onVoicesPageFinished);
}

void ElevenClient::validateApiKey(const QString& apiKey)
{
    beginVoicesWalk(apiKey, true);
}

void ElevenClient::refreshCatalog(const QString& apiKey)
{
    beginVoicesWalk(apiKey, false);
}

void ElevenClient::onVoicesPageFinished()
{
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }
    const int gen = reply->property("walkGen").toInt();
    if (m_pageReply == reply) {
        m_pageReply = nullptr;
    }
    reply->deleteLater();
    if (gen != m_walkGen) {
        return;
    }
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
        QString err = m_walkValidate ? QStringLiteral("ElevenLabs key check failed")
                                     : QStringLiteral("Voice list failed");
        if (status == 401 || status == 403) {
            err = QStringLiteral("Invalid API key");
        } else if (reply->error() == QNetworkReply::OperationCanceledError) {
            err = m_walkValidate ? QStringLiteral("Key check timed out")
                                 : QStringLiteral("Voice list timed out");
        } else if (status > 0) {
            err = QStringLiteral("ElevenLabs error %1").arg(status);
        }
        GAZER_WARN << err << "status" << status;
        failVoicesWalk(err);
        return;
    }
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &pe);
    const QJsonArray page =
        pe.error == QJsonParseError::NoError && doc.isObject()
            ? doc.object().value(QStringLiteral("voices")).toArray()
            : QJsonArray();
    for (const QJsonValue& item : page) {
        const QJsonObject voice = item.toObject();
        const QString id = voice.value(QStringLiteral("voice_id")).toString().trimmed();
        if (id.isEmpty() || m_walkIds.contains(id)) {
            continue;
        }
        m_walkIds.push_back(id);
        m_walkVoices.append(voice);
    }
    const QString next = doc.isObject()
                             ? doc.object().value(QStringLiteral("next_page_token")).toString()
                             : QString();
    ++m_walkPages;
    const bool repeated = !next.isEmpty() && (next == m_walkToken || next == m_walkPrevToken);
    if (!next.isEmpty() && !repeated && m_walkPages < 20) {
        m_walkPrevToken = m_walkToken;
        m_walkToken = next;
        fetchVoicesPage();
        return;
    }
    finishVoicesPass();
}

void ElevenClient::finishVoicesPass()
{
    if (m_walkDefaults) {
        m_walkDefaults = false;
        m_walkPages = 0;
        m_walkToken.clear();
        m_walkPrevToken.clear();
        fetchVoicesPage();
        return;
    }
    QJsonObject wrapped;
    wrapped.insert(QStringLiteral("voices"), m_walkVoices);
    const QByteArray payload = QJsonDocument(wrapped).toJson(QJsonDocument::Compact);
    if (!saveVoicesCache(payload)) {
        GAZER_DEBUG << "ElevenLabs voices cache not written";
    }
    const bool validate = m_walkValidate;
    m_walkKey.clear();
    if (validate) {
        GAZER_INFO << "ElevenLabs key ok";
        emit keyValidated(true, {});
    } else {
        emit catalogReady(true, {});
    }
}

void ElevenClient::startSpeech(const QString& apiKey, const QString& voiceId, const QString& text)
{
    m_http->abort();
    m_dialogue->start(apiKey, voiceId, text);
}

void ElevenClient::startPhrases(const QString& apiKey, const QString& voiceId,
                                const QStringList& phrases)
{
    m_dialogue->abort();
    m_http->start(apiKey, voiceId, phrases);
}

void ElevenClient::abortSpeech()
{
    m_dialogue->abort();
    m_http->abort();
}

} // namespace gazer