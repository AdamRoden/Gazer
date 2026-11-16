#pragma once

#include <QFile>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QTimer>

class QNetworkReply;

namespace gazer {

/// Checks GitHub for a newer Gazer MSI, downloads it beside ProgramData, and
/// asks GazerUpdate.exe to install it after this process quits.
class AppUpdater final : public QObject {
    Q_OBJECT

public:
    enum class Phase { Idle, Checking, Available, Downloading, Ready, Failed };

    explicit AppUpdater(QObject* parent = nullptr);

    /// First check about 45 seconds after this call, then about every 4 hours.
    void start();
    void checkNow();
    void beginDownload();
    /// Starts the helper and emits restarting() after it is running.
    [[nodiscard]] bool beginRestart();
    void dismiss();

    [[nodiscard]] Phase phase() const { return m_phase; }
    [[nodiscard]] QString statusLine() const;
    [[nodiscard]] QString promptCaption() const;
    [[nodiscard]] QString setupNote() const;
    [[nodiscard]] QString blockReason() const { return m_blockReason; }
    [[nodiscard]] bool taskExists() const;

signals:
    void promptRequested();
    void changed();
    void restarting();
    void statusMessage(const QString& message);

private:
    void check(bool user);
    void scanPending();
    void setPhase(Phase phase);
    void maybePrompt(bool user);
    void onCheckFinished();
    void onDownloadFinished();
    [[nodiscard]] bool acceptableMsi(const QString& path) const;
    [[nodiscard]] QString helperPath() const;

    Phase m_phase = Phase::Idle;
    QString m_version;
    QString m_name;
    QString m_url;
    QString m_error;
    QString m_blockReason;
    QString m_snoozedVersion;
    int m_percent = 0;
    int m_reportedPercent = -1;
    bool m_userCheck = false;
    bool m_restarting = false;
    mutable bool m_taskQueried = false;
    mutable bool m_taskFound = false;

    QNetworkAccessManager m_net;
    QTimer m_interval;
    QFile m_part;
    QNetworkReply* m_reply = nullptr;
};

} // namespace gazer
