#pragma once

#include <QFile>
#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QUrl;

namespace gazer {

/// Downloads the official Nefarius ViGEmBus setup and launches it (UAC via runas).
class VigemInstaller final : public QObject {
    Q_OBJECT

public:
    explicit VigemInstaller(QObject* parent = nullptr);
    ~VigemInstaller() override;

    [[nodiscard]] bool isBusy() const { return m_busy; }
    [[nodiscard]] QString statusLine() const { return m_status; }

    void start();
    /// After a successful download, dwell again to ShellExecute runas (UAC).
    void launchPendingSetup();
    [[nodiscard]] bool hasPendingSetup() const;

signals:
    void statusChanged();
    void finished(bool ok, const QString& message);

private:
    void setStatus(const QString& line);
    void fail(const QString& message);
    void abortReply();
    void fetchLatestRelease();
    void downloadSetup(const QUrl& url);
    void launchSetup(const QString& path);
    void onReleaseFinished();
    void onDownloadFinished();

    QNetworkAccessManager* m_nam = nullptr;
    QNetworkReply* m_reply = nullptr;
    QFile m_file;
    bool m_busy = false;
    QString m_status;
    QString m_setupPath;
};

} // namespace gazer
