#include "app/ActionChannel.h"

#include "app/InboundActions.h"
#include "utils/Log.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <QtGlobal>
#include <cstring>
#include <utility>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

namespace gazer {

namespace {
constexpr int kConnectMs = 400;
constexpr int kWriteMs = 2000;
constexpr int kMaxPayload = 1024 * 1024;
constexpr char kAck[] = "ok\n";

QByteArray encodeFrame(const QString& payload)
{
    const QByteArray utf = payload.toUtf8();
    const auto n = quint32(utf.size());
    QByteArray out;
    out.resize(int(4 + n));
    out[0] = char(n & 0xff);
    out[1] = char((n >> 8) & 0xff);
    out[2] = char((n >> 16) & 0xff);
    out[3] = char((n >> 24) & 0xff);
    memcpy(out.data() + 4, utf.constData(), n);
    return out;
}

quint32 readU32Le(const QByteArray& b, int at)
{
    return quint32(uchar(b.at(at))) | (quint32(uchar(b.at(at + 1))) << 8)
           | (quint32(uchar(b.at(at + 2))) << 16) | (quint32(uchar(b.at(at + 3))) << 24);
}

#ifdef Q_OS_WIN
quint32 pipeServerPid(QLocalSocket& sock)
{
    const qintptr sd = sock.socketDescriptor();
    if (sd <= 0) {
        return 0;
    }
    ULONG pid = 0;
    if (!GetNamedPipeServerProcessId(HANDLE(sd), &pid)) {
        return 0;
    }
    return quint32(pid);
}
#endif
} // namespace

ActionChannel::ActionChannel(QObject* parent)
    : QObject(parent)
{
}

QString ActionChannel::pipeName()
{
    const QString env = qEnvironmentVariable(QByteArrayLiteral("GAZER_ACTION_PIPE")).trimmed();
    return env.isEmpty() ? QStringLiteral("Gazer") : env;
}

void ActionChannel::setDispatch(DispatchFn fn)
{
    m_dispatch = std::move(fn);
    if (!m_dispatch) {
        return;
    }
    const QVector<QString> queued = std::move(m_queued);
    m_queued.clear();
    for (const QString& payload : queued) {
        if (!payload.isEmpty()) {
            m_dispatch(payload);
        }
    }
}

bool ActionChannel::listen()
{
    if (m_server && m_server->isListening()) {
        return true;
    }
    const QString name = pipeName();
    QLocalSocket probe;
    probe.connectToServer(name);
    if (probe.waitForConnected(kConnectMs)) {
        probe.disconnectFromServer();
        return false;
    }
    QLocalServer::removeServer(name);
    if (!m_server) {
        m_server = new QLocalServer(this);
        m_server->setSocketOptions(QLocalServer::UserAccessOption);
        connect(m_server, &QLocalServer::newConnection, this, &ActionChannel::onNewConnection);
    }
    if (!m_server->listen(name)) {
        GAZER_WARN << "Inbound pipe listen failed:" << m_server->errorString();
        return false;
    }
    GAZER_INFO << "Inbound pipe" << name;
    return true;
}

PeerResult ActionChannel::sendToPeer(const QString& payload, QString* error, quint32* serverPid)
{
    QLocalSocket sock;
    sock.connectToServer(pipeName());
    if (!sock.waitForConnected(kConnectMs)) {
        if (error) {
            *error = sock.errorString().isEmpty() ? QStringLiteral("Gazer is not running")
                                                  : sock.errorString();
        }
        return PeerResult::ConnectFailed;
    }
#ifdef Q_OS_WIN
    if (serverPid) {
        *serverPid = pipeServerPid(sock);
    }
#else
    Q_UNUSED(serverPid);
#endif
    const QByteArray frame = encodeFrame(payload);
    if (int(payload.toUtf8().size()) > kMaxPayload) {
        if (error) {
            *error = QStringLiteral("Inbound action is too large");
        }
        return PeerResult::WriteFailed;
    }
    if (sock.write(frame) != frame.size() || !sock.waitForBytesWritten(kWriteMs)) {
        if (error) {
            *error = QStringLiteral("Could not write inbound action");
        }
        return PeerResult::WriteFailed;
    }
    if (!sock.waitForReadyRead(kAckMs)) {
        if (error) {
            *error = QStringLiteral("Gazer did not acknowledge (hung?)");
        }
        return PeerResult::Timeout;
    }
    const QByteArray ack = sock.readAll();
    sock.disconnectFromServer();
    if (!ack.contains(kAck)) {
        if (error) {
            *error = QStringLiteral("Gazer did not acknowledge (hung?)");
        }
        return PeerResult::Timeout;
    }
    return PeerResult::Ok;
}

void ActionChannel::onNewConnection()
{
    while (m_server && m_server->hasPendingConnections()) {
        QLocalSocket* sock = m_server->nextPendingConnection();
        if (!sock) {
            continue;
        }
        connect(sock, &QLocalSocket::disconnected, this, [this, sock]() { finishSocket(sock); });
        connect(sock, &QLocalSocket::readyRead, this, [this, sock]() { onReadyRead(sock); });
        if (sock->bytesAvailable() > 0) {
            onReadyRead(sock);
        }
        if (sock->state() == QLocalSocket::UnconnectedState) {
            finishSocket(sock);
        }
    }
}

void ActionChannel::onReadyRead(QLocalSocket* sock)
{
    if (!sock) {
        return;
    }
    QByteArray& buf = m_buffers[sock];
    buf += sock->readAll();
    if (buf.size() > kMaxPayload + 4) {
        GAZER_WARN << "Inbound payload too large";
        sock->abort();
        return;
    }
    while (buf.size() >= 4) {
        const quint32 n = readU32Le(buf, 0);
        if (n > quint32(kMaxPayload)) {
            GAZER_WARN << "Inbound payload too large";
            sock->abort();
            return;
        }
        if (quint32(buf.size()) < 4 + n) {
            break;
        }
        const QByteArray body = buf.mid(4, int(n));
        buf.remove(0, int(4 + n));
        handlePayload(sock, QString::fromUtf8(body));
    }
}

void ActionChannel::handlePayload(QLocalSocket* sock, const QString& payload)
{
    if (sock && sock->state() == QLocalSocket::ConnectedState) {
        sock->write(QByteArray(kAck));
        sock->flush();
    }
    if (isInboundPing(payload)) {
        return;
    }
    if (payload.isEmpty()) {
        return;
    }
    if (m_dispatch) {
        m_dispatch(payload);
    } else {
        m_queued.push_back(payload);
    }
}

void ActionChannel::finishSocket(QLocalSocket* sock)
{
    if (!sock) {
        return;
    }
    sock->disconnect(this);
    m_buffers.remove(sock);
    sock->deleteLater();
}

} // namespace gazer
