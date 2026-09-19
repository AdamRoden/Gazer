#include "app/ActionChannel.h"

#include "utils/Log.h"

#include <QLocalServer>
#include <QLocalSocket>
#include <utility>

namespace gazer {

namespace {
constexpr int kConnectMs = 400;
constexpr int kWriteMs = 2000;
constexpr int kMaxPayload = 1024 * 1024;
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

bool ActionChannel::sendToPeer(const QString& payload, QString* error)
{
    QLocalSocket sock;
    sock.connectToServer(pipeName());
    if (!sock.waitForConnected(kConnectMs)) {
        if (error) {
            *error = sock.errorString().isEmpty() ? QStringLiteral("Gazer is not running")
                                                  : sock.errorString();
        }
        return false;
    }
    const QByteArray bytes = payload.toUtf8();
    if (bytes.size() > kMaxPayload) {
        if (error) {
            *error = QStringLiteral("Inbound action is too large");
        }
        return false;
    }
    if (sock.write(bytes) != bytes.size() || !sock.waitForBytesWritten(kWriteMs)) {
        if (error) {
            *error = QStringLiteral("Could not write inbound action");
        }
        return false;
    }
    sock.disconnectFromServer();
    if (sock.state() != QLocalSocket::UnconnectedState) {
        sock.waitForDisconnected(kWriteMs);
    }
    return true;
}

void ActionChannel::onNewConnection()
{
    while (m_server && m_server->hasPendingConnections()) {
        QLocalSocket* sock = m_server->nextPendingConnection();
        if (!sock) {
            continue;
        }
        connect(sock, &QLocalSocket::disconnected, this, [this, sock]() { finishSocket(sock); });
        connect(sock, &QLocalSocket::readyRead, this, [this, sock]() {
            if (sock->bytesAvailable() > kMaxPayload) {
                GAZER_WARN << "Inbound payload too large";
                sock->abort();
            }
        });
        if (sock->state() == QLocalSocket::UnconnectedState) {
            finishSocket(sock);
        }
    }
}

void ActionChannel::finishSocket(QLocalSocket* sock)
{
    if (!sock) {
        return;
    }
    sock->disconnect(this);
    const QByteArray data = sock->readAll();
    sock->deleteLater();
    if (data.isEmpty() || data.size() > kMaxPayload) {
        if (data.size() > kMaxPayload) {
            GAZER_WARN << "Inbound payload too large";
        }
        return;
    }
    const QString payload = QString::fromUtf8(data);
    if (m_dispatch) {
        m_dispatch(payload);
    } else {
        m_queued.push_back(payload);
    }
}

} // namespace gazer
