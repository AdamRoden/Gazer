#include "assist/ElevenHttp.h"

#include "assist/ElevenRequest.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

namespace gazer {
namespace {

constexpr int kStallMs = 20000;

int retryAfterMs(const QNetworkReply* reply)
{
    if (!reply) {
        return 1000;
    }
    bool ok = false;
    const int sec = QString::fromLatin1(reply->rawHeader("Retry-After")).trimmed().toInt(&ok);
    return ok && sec > 0 ? qMin(sec * 1000, 5000) : 1000;
}

bool rejectedType(const QByteArray& contentType)
{
    const QByteArray ct = contentType.toLower();
    return ct.contains("json") || ct.contains("mpeg") || ct.contains("mp3");
}

QString httpError(int status)
{
    if (status == 401 || status == 403) {
        return QStringLiteral("Invalid API key");
    }
    if (status == 429) {
        return QStringLiteral("ElevenLabs busy — try again");
    }
    if (status > 0) {
        return QStringLiteral("ElevenLabs error %1").arg(status);
    }
    return QStringLiteral("ElevenLabs stream failed");
}

} // namespace

class ElevenHttpStream::ReplySlot : public QObject {
public:
    ReplySlot(QNetworkReply* reply, int index, QObject* parent)
        : QObject(parent)
        , reply(reply)
        , index(index)
    {
    }

    QNetworkReply* reply = nullptr;
    QByteArray held;
    bool done = false;
    bool bad = false;
    int index = -1;
};

ElevenHttpStream::ElevenHttpStream(QNetworkAccessManager* nam, QObject* parent)
    : QObject(parent)
    , m_nam(nam)
    , m_stall(new QTimer(this))
{
    m_stall->setSingleShot(true);
    m_stall->setInterval(kStallMs);
    connect(m_stall, &QTimer::timeout, this, [this]() {
        fail(0, QStringLiteral("ElevenLabs timed out"), 0);
    });
}

void ElevenHttpStream::start(const QString& apiKey, const QString& voiceId,
                             const QStringList& phrases)
{
    abort();
    m_settled = false;
    m_heard = false;
    m_key = apiKey;
    m_voiceId = voiceId.trimmed();
    m_phrases = phrases;
    m_openAt = 0;
    if (!m_nam || m_key.trimmed().isEmpty() || m_voiceId.isEmpty() || m_phrases.isEmpty()) {
        fail(0, QStringLiteral("Empty audio"), 0);
        return;
    }
    m_live = openPhrase(m_openAt++);
    if (!m_live) {
        fail(0, QStringLiteral("ElevenLabs stream failed"), 0);
        return;
    }
    maybePrefetch();
    armStall();
}

void ElevenHttpStream::abort()
{
    m_settled = true;
    if (m_stall) {
        m_stall->stop();
    }
    drop(m_live);
    drop(m_ahead);
    m_phrases.clear();
    m_openAt = 0;
}

void ElevenHttpStream::armStall()
{
    if (!m_settled && m_stall) {
        m_stall->start();
    }
}

void ElevenHttpStream::drop(ReplySlot*& slot)
{
    if (!slot) {
        return;
    }
    if (slot->reply) {
        slot->reply->disconnect(this);
        slot->reply->abort();
        slot->reply->deleteLater();
        slot->reply = nullptr;
    }
    slot->deleteLater();
    slot = nullptr;
}

ElevenHttpStream::ReplySlot* ElevenHttpStream::openPhrase(int index)
{
    if (!m_nam || index < 0 || index >= m_phrases.size()) {
        return nullptr;
    }
    const QString encoded = QString::fromUtf8(QUrl::toPercentEncoding(m_voiceId.toUtf8()));
    QUrl url(QStringLiteral("https://api.elevenlabs.io/v1/text-to-speech/%1/stream").arg(encoded));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("output_format"), QStringLiteral("pcm_24000"));
    url.setQuery(query);

    QJsonObject body;
    body.insert(QStringLiteral("text"), m_phrases.at(index));
    body.insert(QStringLiteral("model_id"), ElevenRequest::kModelTurbo.toString());
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Accept", "application/octet-stream");
    req.setRawHeader("xi-api-key", m_key.toUtf8());

    QNetworkReply* reply = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    auto* slot = new ReplySlot(reply, index, this);
    connect(reply, &QNetworkReply::readyRead, this, [this, slot]() { onReady(slot); });
    connect(reply, &QNetworkReply::finished, this, [this, slot]() { onDone(slot); });
    return slot;
}

void ElevenHttpStream::maybePrefetch()
{
    if (m_settled || m_ahead || m_openAt >= m_phrases.size()) {
        return;
    }
    m_ahead = openPhrase(m_openAt++);
}

void ElevenHttpStream::onReady(ReplySlot* slot)
{
    if (m_settled || !slot || !slot->reply || slot->bad) {
        return;
    }
    const int status = slot->reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status >= 400 || rejectedType(slot->reply->rawHeader("Content-Type"))) {
        slot->bad = true;
        return;
    }
    const QByteArray bytes = slot->reply->readAll();
    if (bytes.isEmpty()) {
        return;
    }
    armStall();
    m_heard = true;
    if (slot == m_live) {
        emit speechChunk(bytes);
    } else {
        slot->held.append(bytes);
    }
}

void ElevenHttpStream::onDone(ReplySlot* slot)
{
    if (m_settled || !slot || !slot->reply) {
        return;
    }
    onReady(slot);
    QNetworkReply* reply = slot->reply;
    slot->reply = nullptr;
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError net = reply->error();
    const bool bad = slot->bad || status >= 400
                     || (net != QNetworkReply::NoError
                         && net != QNetworkReply::OperationCanceledError);
    int retry = status == 429 ? retryAfterMs(reply) : 0;
    QString message = httpError(status);
    if (bad && rejectedType(reply->rawHeader("Content-Type"))) {
        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        const QString detail = obj.value(QStringLiteral("detail")).toString();
        if (!detail.isEmpty() && status != 401 && status != 403 && status != 429) {
            message = detail;
        }
    }
    reply->deleteLater();

    const bool live = slot == m_live;
    const bool ahead = slot == m_ahead;
    if (!live && !ahead) {
        slot->deleteLater();
        return;
    }
    if (bad) {
        if (live) {
            m_live = nullptr;
        }
        if (ahead) {
            m_ahead = nullptr;
        }
        slot->deleteLater();
        fail(status, message, retry);
        return;
    }
    if (net == QNetworkReply::OperationCanceledError) {
        if (live) {
            m_live = nullptr;
        }
        if (ahead) {
            m_ahead = nullptr;
        }
        slot->deleteLater();
        return;
    }
    slot->done = true;
    if (ahead && !live) {
        return;
    }
    finishLive();
}

void ElevenHttpStream::finishLive()
{
    if (m_live) {
        m_live->deleteLater();
        m_live = nullptr;
    }
    if (m_ahead) {
        if (!m_ahead->held.isEmpty()) {
            m_heard = true;
            emit speechChunk(m_ahead->held);
            m_ahead->held.clear();
        }
        if (m_ahead->done) {
            m_ahead->deleteLater();
            m_ahead = nullptr;
            openLiveOrSucceed();
            return;
        }
        m_live = m_ahead;
        m_ahead = nullptr;
        maybePrefetch();
        armStall();
        return;
    }
    openLiveOrSucceed();
}

void ElevenHttpStream::openLiveOrSucceed()
{
    if (m_settled) {
        return;
    }
    if (m_openAt >= m_phrases.size()) {
        succeed();
        return;
    }
    m_live = openPhrase(m_openAt++);
    if (!m_live) {
        fail(0, QStringLiteral("ElevenLabs stream failed"), 0);
        return;
    }
    maybePrefetch();
    armStall();
}

void ElevenHttpStream::fail(int httpStatus, const QString& error, int retryAfterMs)
{
    if (m_settled) {
        return;
    }
    m_settled = true;
    if (m_stall) {
        m_stall->stop();
    }
    drop(m_live);
    drop(m_ahead);
    emit speechFailed(httpStatus, error, retryAfterMs);
}

void ElevenHttpStream::succeed()
{
    if (m_settled) {
        return;
    }
    if (!m_heard) {
        fail(0, QStringLiteral("Empty audio"), 0);
        return;
    }
    m_settled = true;
    if (m_stall) {
        m_stall->stop();
    }
    emit speechStreamEnded();
}

} // namespace gazer
