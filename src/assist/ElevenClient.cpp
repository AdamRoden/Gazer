#include "assist/ElevenClient.h"

#include "assist/ElevenRequest.h"
#include "utils/AtomicFile.h"
#include "utils/Log.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

namespace gazer {

ElevenClient::ElevenClient(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
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

void ElevenClient::startVoicesGet(const QString& apiKey, bool forValidate)
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
    QNetworkReply*& slot = forValidate ? m_validateReply : m_catalogReply;
    if (slot) {
        QNetworkReply* prev = slot;
        slot = nullptr;
        prev->disconnect(this);
        prev->abort();
        prev->deleteLater();
    }
    QNetworkRequest req(QUrl(ElevenRequest::kVoicesUrl.toString()));
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("xi-api-key", key.toUtf8());
    slot = m_nam->get(req);
    QTimer::singleShot(ElevenRequest::kVoicesTimeoutMs, slot, [reply = slot]() {
        if (reply) {
            reply->abort();
        }
    });
    connect(slot, &QNetworkReply::finished, this,
            forValidate ? &ElevenClient::onValidateFinished : &ElevenClient::onCatalogFinished);
}

void ElevenClient::validateApiKey(const QString& apiKey)
{
    startVoicesGet(apiKey, true);
}

void ElevenClient::refreshCatalog(const QString& apiKey)
{
    startVoicesGet(apiKey, false);
}

void ElevenClient::onValidateFinished()
{
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }
    if (m_validateReply == reply) {
        m_validateReply = nullptr;
    }
    reply->deleteLater();
    if (m_validateReply) {
        return;
    }
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    if (reply->error() == QNetworkReply::NoError && status >= 200 && status < 300) {
        if (!saveVoicesCache(body)) {
            GAZER_DEBUG << "ElevenLabs voices cache not written";
        }
        GAZER_INFO << "ElevenLabs key ok" << status;
        emit keyValidated(true, {});
        return;
    }
    QString err = QStringLiteral("ElevenLabs key check failed");
    if (status == 401 || status == 403) {
        err = QStringLiteral("Invalid API key");
    } else if (reply->error() == QNetworkReply::OperationCanceledError) {
        err = QStringLiteral("Key check timed out");
    } else if (status > 0) {
        err = QStringLiteral("ElevenLabs error %1").arg(status);
    }
    GAZER_WARN << err << "status" << status;
    emit keyValidated(false, err);
}

void ElevenClient::onCatalogFinished()
{
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }
    if (m_catalogReply == reply) {
        m_catalogReply = nullptr;
    }
    reply->deleteLater();
    if (m_catalogReply) {
        return;
    }
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    if (reply->error() == QNetworkReply::NoError && status >= 200 && status < 300) {
        if (!saveVoicesCache(body)) {
            GAZER_DEBUG << "ElevenLabs voices cache not written";
        }
        emit catalogReady(true, {});
        return;
    }
    QString err = QStringLiteral("Voice list failed");
    if (reply->error() == QNetworkReply::OperationCanceledError) {
        err = QStringLiteral("Voice list timed out");
    } else if (status > 0) {
        err = QStringLiteral("ElevenLabs error %1").arg(status);
    }
    GAZER_WARN << err << "status" << status;
    emit catalogReady(false, err);
}

void ElevenClient::fetchSpeech(const QString& apiKey, const QString& voiceId, const QJsonObject& body)
{
    const QString key = apiKey.trimmed();
    const QString id = voiceId.trimmed();
    if (key.isEmpty() || id.isEmpty()) {
        emit speechFailed(0, QStringLiteral("Missing API key or voice"), 0);
        return;
    }
    abortSpeech();
    QNetworkRequest req(QUrl(ElevenRequest::speakUrl(id)));
    req.setRawHeader("Accept", "audio/mpeg");
    req.setRawHeader("Content-Type", "application/json");
    req.setRawHeader("xi-api-key", key.toUtf8());
    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
    m_speakReply = m_nam->post(req, payload);
    QTimer::singleShot(ElevenRequest::kSpeakTimeoutMs, m_speakReply, [reply = m_speakReply]() {
        if (reply) {
            reply->abort();
        }
    });
    connect(m_speakReply, &QNetworkReply::finished, this, &ElevenClient::onSpeakFinished);
}

void ElevenClient::abortSpeech()
{
    if (!m_speakReply) {
        return;
    }
    QNetworkReply* reply = m_speakReply;
    m_speakReply = nullptr;
    reply->disconnect(this);
    reply->abort();
    reply->deleteLater();
}

void ElevenClient::onSpeakFinished()
{
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }
    if (m_speakReply == reply) {
        m_speakReply = nullptr;
    }
    reply->deleteLater();
    if (m_speakReply) {
        return;
    }
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() == QNetworkReply::NoError && status >= 200 && status < 300) {
        const QByteArray mpeg = reply->readAll();
        GAZER_INFO << "ElevenLabs speech ok" << status << "bytes" << mpeg.size();
        emit speechReady(mpeg);
        return;
    }
    int retryAfterMs = 0;
    if (status == 429) {
        bool ok = false;
        const int sec = QString::fromLatin1(reply->rawHeader("Retry-After").trimmed()).toInt(&ok);
        retryAfterMs = ok && sec > 0 ? qMin(sec * 1000, 5000) : 1000;
    }
    QString err = QStringLiteral("ElevenLabs request failed");
    if (status == 401 || status == 403) {
        err = QStringLiteral("Invalid API key");
    } else if (status == 429) {
        err = QStringLiteral("ElevenLabs busy — try again");
    } else if (reply->error() == QNetworkReply::OperationCanceledError) {
        err = QStringLiteral("ElevenLabs timed out");
    } else if (status > 0) {
        err = QStringLiteral("ElevenLabs error %1").arg(status);
    }
    GAZER_WARN << err << "status" << status;
    emit speechFailed(status, err, retryAfterMs);
}

} // namespace gazer
