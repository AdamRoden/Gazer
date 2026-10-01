#include "assist/DialogueSocket.h"

#include "assist/ElevenRequest.h"
#include "assist/WsFrame.h"
#include "utils/Log.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkProxy>
#include <QRandomGenerator>
#include <QSslError>
#include <QSslSocket>
#include <QTimer>
#include <QUrl>

namespace gazer {
namespace {

constexpr int kMaxUpgradeBytes = 16384;

QString upgradeError(int status)
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
    return QStringLiteral("ElevenLabs stream rejected");
}

int retryAfterMs(const QByteArray& head)
{
    const QByteArray lower = head.toLower();
    const QByteArray retry = QByteArrayLiteral("retry-after:");
    const int at = lower.indexOf(retry);
    if (at < 0) {
        return 1000;
    }
    bool ok = false;
    const int sec = lower.mid(at + retry.size()).trimmed().toInt(&ok);
    return ok && sec > 0 ? qMin(sec * 1000, 5000) : 1000;
}

} // namespace

DialogueUpgrade parseDialogueUpgrade(const QByteArray& rx)
{
    DialogueUpgrade out;
    const int sep = rx.indexOf("\r\n\r\n");
    if (sep < 0) {
        if (rx.size() > kMaxUpgradeBytes) {
            out.status = DialogueUpgrade::Status::Failed;
            out.error = QStringLiteral("ElevenLabs stream rejected");
        }
        return out;
    }
    const QByteArray head = rx.left(sep);
    const int lineEnd = head.indexOf("\r\n");
    const QByteArray statusLine = lineEnd < 0 ? head : head.left(lineEnd);
    const int sp1 = statusLine.indexOf(' ');
    const int sp2 = statusLine.indexOf(' ', sp1 + 1);
    int status = 0;
    if (sp1 > 0) {
        const int n = sp2 > sp1 ? sp2 - sp1 - 1 : statusLine.size() - sp1 - 1;
        status = statusLine.mid(sp1 + 1, n).toInt();
    }
    out.httpStatus = status;
    out.rest = rx.mid(sep + 4);
    if (status != 101) {
        out.status = DialogueUpgrade::Status::Failed;
        out.error = upgradeError(status);
        if (status == 429) {
            out.retryAfterMs = retryAfterMs(head);
        }
        return out;
    }
    out.status = DialogueUpgrade::Status::Ready;
    return out;
}

DialogueEvent parseDialogueEvent(const QByteArray& payload)
{
    DialogueEvent ev;
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        return ev;
    }
    const QJsonObject obj = doc.object();
    const QString errCode = obj.value(QStringLiteral("error")).toString();
    if (!errCode.isEmpty() && !obj.contains(QStringLiteral("audio"))) {
        const QString message = obj.value(QStringLiteral("message")).toString();
        ev.failed = true;
        ev.error = message.isEmpty() ? QStringLiteral("ElevenLabs stream failed") : message;
        const QString code = errCode.toLower();
        if (code.contains(QLatin1String("auth")) || code.contains(QLatin1String("api_key"))
            || code.contains(QLatin1String("unauthorized"))) {
            ev.httpStatus = 401;
            ev.error = QStringLiteral("Invalid API key");
        } else if (code.contains(QLatin1String("forbidden"))
                   || code.contains(QLatin1String("permission"))) {
            ev.httpStatus = 403;
            ev.error = QStringLiteral("ElevenLabs forbidden");
        } else if (code.contains(QLatin1String("rate")) || code.contains(QLatin1String("concurrent"))
                   || code.contains(QLatin1String("too_many"))
                   || code.contains(QLatin1String("busy"))) {
            ev.httpStatus = 429;
            ev.retryAfterMs = 1000;
            ev.error = QStringLiteral("ElevenLabs busy — try again");
        }
        return ev;
    }
    const QString audio = obj.value(QStringLiteral("audio")).toString();
    if (!audio.isEmpty()) {
        ev.pcm = QByteArray::fromBase64(audio.toLatin1());
    }
    ev.isFinal = obj.value(QStringLiteral("is_final")).toBool();
    return ev;
}

DialogueSocket::DialogueSocket(QObject* parent)
    : QObject(parent)
    , m_idle(new QTimer(this))
    , m_keepAlive(new QTimer(this))
{
    m_idle->setSingleShot(true);
    connect(m_idle, &QTimer::timeout, this, [this]() {
        if (!m_streamDone && !m_aborting) {
            failStream(0, QStringLiteral("ElevenLabs timed out"), 0);
        }
    });
    m_keepAlive->setInterval(10000);
    connect(m_keepAlive, &QTimer::timeout, this, [this]() {
        if (m_upgraded && !m_streamDone && !m_aborting) {
            sendStreamText(ElevenRequest::keepAliveMessage());
        }
    });
}

DialogueSocket::~DialogueSocket()
{
    abort();
}

void DialogueSocket::start(const QString& apiKey, const QString& voiceId, const QString& text)
{
    const QString key = apiKey.trimmed();
    const QString id = voiceId.trimmed();
    if (key.isEmpty() || id.isEmpty() || text.isEmpty()) {
        emit speechFailed(0, QStringLiteral("Missing API key or voice"), 0);
        return;
    }
    if (key.contains(QLatin1Char('\r')) || key.contains(QLatin1Char('\n'))) {
        emit speechFailed(401, QStringLiteral("Invalid API key"), 0);
        return;
    }
    abort();
    m_aborting = false;
    m_streamDone = false;
    m_upgraded = false;
    m_rx.clear();
    m_textAcc.clear();
    m_streamText = text;
    m_streamVoice = id;

    m_socket = new QSslSocket(this);
    const QNetworkProxyQuery proxyQuery(QUrl(QStringLiteral("https://api.elevenlabs.io")));
    const QList<QNetworkProxy> proxies = QNetworkProxyFactory::systemProxyForQuery(proxyQuery);
    if (!proxies.isEmpty() && proxies.first().type() != QNetworkProxy::NoProxy
        && proxies.first().type() != QNetworkProxy::DefaultProxy) {
        m_socket->setProxy(proxies.first());
    } else {
        m_socket->setProxy(QNetworkProxy::NoProxy);
    }
    connect(m_socket, &QSslSocket::encrypted, this, &DialogueSocket::onSocketEncrypted);
    connect(m_socket, &QSslSocket::readyRead, this, &DialogueSocket::onSocketReadyRead);
    connect(m_socket, &QAbstractSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        onSocketError();
    });
    connect(m_socket, &QSslSocket::sslErrors, this, [this](const QList<QSslError>&) {
        if (!m_aborting && !m_streamDone) {
            failStream(0, QStringLiteral("ElevenLabs TLS failed"), 0);
        }
    });

    QByteArray raw(16, '\0');
    for (int i = 0; i < raw.size(); ++i) {
        raw[i] = char(QRandomGenerator::global()->generate() & 0xFF);
    }
    m_wsKey = raw.toBase64();
    armIdleTimer();
    GAZER_INFO << "ElevenLabs stream" << ElevenRequest::kModelTurbo << "voice" << id;
    m_socket->setProperty("xiKey", key);
    m_socket->connectToHostEncrypted(QStringLiteral("api.elevenlabs.io"), 443);
}

void DialogueSocket::abort()
{
    m_aborting = true;
    tearDownSocket();
    m_aborting = false;
    m_streamDone = true;
}

void DialogueSocket::tearDownSocket()
{
    m_idle->stop();
    m_keepAlive->stop();
    m_upgraded = false;
    m_textAcc.clear();
    m_rx.clear();
    m_wsKey.clear();
    m_streamText.clear();
    m_streamVoice.clear();
    if (!m_socket) {
        return;
    }
    QSslSocket* socket = m_socket;
    m_socket = nullptr;
    socket->disconnect(this);
    socket->abort();
    socket->deleteLater();
}

void DialogueSocket::failStream(int httpStatus, const QString& error, int retryAfterMs)
{
    if (m_aborting || m_streamDone) {
        return;
    }
    m_streamDone = true;
    tearDownSocket();
    GAZER_WARN << error << "status" << httpStatus;
    emit speechFailed(httpStatus, error, retryAfterMs);
}

void DialogueSocket::armIdleTimer()
{
    m_idle->start(ElevenRequest::kSpeakTimeoutMs);
}

void DialogueSocket::sendStreamText(const QByteArray& json)
{
    if (!m_socket || !m_upgraded) {
        return;
    }
    const quint32 mask = QRandomGenerator::global()->generate();
    m_socket->write(WsFrame::clientFrame(WsFrame::kOpText, json, mask));
}

void DialogueSocket::onSocketEncrypted()
{
    if (!m_socket || m_aborting || m_streamDone) {
        return;
    }
    const QString key = m_socket->property("xiKey").toString();
    m_socket->setProperty("xiKey", {});
    const QUrl url(ElevenRequest::dialogueStreamUrl());
    const QString path = url.path() + QLatin1Char('?') + url.query();
    QByteArray req;
    req += "GET " + path.toUtf8() + " HTTP/1.1\r\n";
    req += "Host: api.elevenlabs.io\r\n";
    req += "Upgrade: websocket\r\n";
    req += "Connection: Upgrade\r\n";
    req += "Sec-WebSocket-Key: " + m_wsKey + "\r\n";
    req += "Sec-WebSocket-Version: 13\r\n";
    req += "xi-api-key: " + key.toUtf8() + "\r\n";
    req += "\r\n";
    m_socket->write(req);
}

void DialogueSocket::onSocketReadyRead()
{
    if (!m_socket || m_aborting || m_streamDone) {
        return;
    }
    m_rx += m_socket->readAll();
    if (!m_upgraded) {
        parseUpgrade();
    }
    if (m_upgraded && !m_streamDone) {
        parseFrames();
    }
}

void DialogueSocket::onSocketError()
{
    if (m_aborting || m_streamDone) {
        return;
    }
    failStream(0, QStringLiteral("ElevenLabs connection failed"), 0);
}

void DialogueSocket::parseUpgrade()
{
    const DialogueUpgrade up = parseDialogueUpgrade(m_rx);
    if (up.status == DialogueUpgrade::Status::NeedMore) {
        return;
    }
    if (up.status == DialogueUpgrade::Status::Failed) {
        failStream(up.httpStatus, up.error, up.retryAfterMs);
        return;
    }
    m_rx = up.rest;
    m_upgraded = true;
    armIdleTimer();
    m_keepAlive->start();
    sendStreamText(ElevenRequest::registerVoicesMessage(m_streamVoice));
    sendStreamText(ElevenRequest::speakInputsMessage(m_streamText, m_streamVoice));
    // close_socket flushes short phrases that never reach the server's word buffer.
    sendStreamText(ElevenRequest::closeSocketMessage());
}

void DialogueSocket::parseFrames()
{
    while (!m_streamDone && !m_rx.isEmpty()) {
        const WsFrame::Incoming frame = WsFrame::serverFrame(m_rx);
        if (frame.status == WsFrame::Incoming::Status::NeedMore) {
            return;
        }
        if (frame.status == WsFrame::Incoming::Status::Error || frame.consumed <= 0) {
            failStream(0, QStringLiteral("ElevenLabs stream failed"), 0);
            return;
        }
        m_rx.remove(0, frame.consumed);
        if (frame.opcode == WsFrame::kOpPing) {
            if (m_socket) {
                const quint32 mask = QRandomGenerator::global()->generate();
                m_socket->write(WsFrame::clientFrame(WsFrame::kOpPong, frame.payload, mask));
            }
            continue;
        }
        if (frame.opcode == WsFrame::kOpClose) {
            if (!m_streamDone) {
                finishStream();
            }
            return;
        }
        if (frame.opcode == WsFrame::kOpPong) {
            continue;
        }
        if (frame.opcode == WsFrame::kOpText || frame.opcode == WsFrame::kOpBinary
            || frame.opcode == WsFrame::kOpCont) {
            if (frame.opcode != WsFrame::kOpCont) {
                m_textAcc.clear();
            }
            m_textAcc += frame.payload;
            if (!frame.fin) {
                continue;
            }
            const QByteArray payload = m_textAcc;
            m_textAcc.clear();
            const DialogueEvent ev = parseDialogueEvent(payload);
            if (ev.failed) {
                failStream(ev.httpStatus, ev.error, ev.retryAfterMs);
                return;
            }
            if (!ev.pcm.isEmpty()) {
                emit speechChunk(ev.pcm);
            }
            if (ev.isFinal) {
                finishStream();
            }
        }
    }
}

void DialogueSocket::finishStream()
{
    if (m_streamDone) {
        return;
    }
    m_streamDone = true;
    tearDownSocket();
    emit speechStreamEnded();
}

} // namespace gazer
