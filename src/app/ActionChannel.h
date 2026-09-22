#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QVector>
#include <functional>

class QLocalServer;
class QLocalSocket;

namespace gazer {

/// Result of a framed write + ACK to the live host.
enum class PeerResult {
    Ok,
    ConnectFailed,
    Timeout,
    WriteFailed
};

/// Same-user named pipe for inbound page actions. Second `Gazer.exe --action` is a client.
/// Frames are uint32 LE length + UTF-8. The host ACKs `ok\n` before dispatch so a hung
/// GUI fails the ACK and a second instance can take over.
class ActionChannel final : public QObject {
    Q_OBJECT

public:
    explicit ActionChannel(QObject* parent = nullptr);

    using DispatchFn = std::function<void(const QString& payload)>;
    void setDispatch(DispatchFn fn);

    /// `Gazer` (`\\.\pipe\Gazer` on Windows). Override with `GAZER_ACTION_PIPE`.
    [[nodiscard]] static QString pipeName();

    /// Listen on `pipeName()`. False if another Gazer already owns the pipe.
    [[nodiscard]] bool listen();

    /// Write a framed payload and wait for ACK. Optional @p serverPid is the peer process.
    [[nodiscard]] static PeerResult sendToPeer(const QString& payload, QString* error = nullptr,
                                               quint32* serverPid = nullptr);

    static constexpr int kAckMs = 1000;

private:
    void onNewConnection();
    void onReadyRead(QLocalSocket* sock);
    void finishSocket(QLocalSocket* sock);
    void handlePayload(QLocalSocket* sock, const QString& payload);

    QLocalServer* m_server = nullptr;
    DispatchFn m_dispatch;
    QVector<QString> m_queued;
    QHash<QLocalSocket*, QByteArray> m_buffers;
};

} // namespace gazer
