#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <functional>

class QLocalServer;
class QLocalSocket;

namespace gazer {

/// Same-user named pipe for inbound page actions. Second `Gazer.exe --action` is a client.
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

    /// Write UTF-8 payload and disconnect. Empty payload is a no-op on the server.
    [[nodiscard]] static bool sendToPeer(const QString& payload, QString* error = nullptr);

private:
    void onNewConnection();
    void finishSocket(QLocalSocket* sock);

    QLocalServer* m_server = nullptr;
    DispatchFn m_dispatch;
    QVector<QString> m_queued;
};

} // namespace gazer
