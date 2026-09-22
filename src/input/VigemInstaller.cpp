#include "input/VigemInstaller.h"

#include "input/VigemDiscovery.h"
#include "input/VigemRelease.h"
#include "utils/Log.h"

#include <QDir>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#ifdef Q_OS_WIN
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN
#    endif
#    include <windows.h>
#    include <shellapi.h>
#endif

namespace gazer {

VigemInstaller::VigemInstaller(QObject* parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

VigemInstaller::~VigemInstaller()
{
    abortReply();
}

void VigemInstaller::setStatus(const QString& line)
{
    m_status = line;
    emit statusChanged();
}

void VigemInstaller::fail(const QString& message)
{
    if (m_file.isOpen()) {
        m_file.close();
    }
    if (!m_setupPath.isEmpty()) {
        QFile::remove(m_setupPath);
        m_setupPath.clear();
    }
    m_busy = false;
    m_status.clear();
    emit statusChanged();
    emit finished(false, message);
}

void VigemInstaller::abortReply()
{
    if (!m_reply) {
        return;
    }
    QNetworkReply* prev = m_reply;
    m_reply = nullptr;
    prev->disconnect(this);
    prev->abort();
    prev->deleteLater();
}

void VigemInstaller::start()
{
    if (m_busy) {
        return;
    }
    if (VigemDiscovery::busLooksInstalled()) {
        emit finished(true, QStringLiteral("ViGEmBus is already installed. Recheck if the status is stale."));
        return;
    }
    if (hasPendingSetup()) {
        launchPendingSetup();
        return;
    }
    m_busy = true;
    setStatus(QStringLiteral("Looking up latest ViGEmBus setup…"));
    fetchLatestRelease();
}

void VigemInstaller::fetchLatestRelease()
{
    abortReply();
    const QUrl apiUrl(QString::fromUtf8(kViGEmBusLatestApi));
    QNetworkRequest req(apiUrl);
    req.setRawHeader("User-Agent", "Gazer");
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_nam->get(req);
    QTimer::singleShot(20000, m_reply, [reply = m_reply]() {
        if (reply) {
            reply->abort();
        }
    });
    connect(m_reply, &QNetworkReply::finished, this, &VigemInstaller::onReleaseFinished);
}

void VigemInstaller::onReleaseFinished()
{
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }
    if (m_reply == reply) {
        m_reply = nullptr;
    }
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        fail(QStringLiteral("Could not look up ViGEmBus setup: %1").arg(reply->errorString()));
        return;
    }
    QString parseErr;
    const QString picked = vigemBusSetupUrlFromReleaseJson(reply->readAll(), &parseErr);
    if (picked.isEmpty()) {
        fail(parseErr.isEmpty() ? QStringLiteral("ViGEmBus release has no setup file") : parseErr);
        return;
    }
    setStatus(QStringLiteral("Downloading ViGEmBus setup…"));
    downloadSetup(QUrl(picked));
}

void VigemInstaller::downloadSetup(const QUrl& url)
{
    abortReply();
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir().mkpath(dir);
    m_setupPath = QDir(dir).filePath(QStringLiteral("Gazer-ViGEmBus-setup.exe"));
    m_file.setFileName(m_setupPath);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        fail(QStringLiteral("Cannot write ViGEm setup to %1").arg(m_setupPath));
        return;
    }
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "Gazer");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_nam->get(req);
    QTimer::singleShot(120000, m_reply, [reply = m_reply]() {
        if (reply) {
            reply->abort();
        }
    });
    connect(m_reply, &QNetworkReply::readyRead, this, [this]() {
        if (m_reply && m_file.isOpen()) {
            m_file.write(m_reply->readAll());
        }
    });
    connect(m_reply, &QNetworkReply::finished, this, &VigemInstaller::onDownloadFinished);
}

void VigemInstaller::onDownloadFinished()
{
    auto* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }
    if (m_reply == reply) {
        m_reply = nullptr;
    }
    reply->deleteLater();
    if (m_file.isOpen()) {
        if (reply->error() == QNetworkReply::NoError) {
            m_file.write(reply->readAll());
        }
        m_file.close();
    }
    if (reply->error() != QNetworkReply::NoError) {
        fail(QStringLiteral("ViGEm download failed: %1").arg(reply->errorString()));
        return;
    }
    const QFileInfo fi(m_setupPath);
    if (!fi.exists() || fi.size() < 1024) {
        fail(QStringLiteral("ViGEm download was empty"));
        return;
    }
    const QString name = QFileInfo(reply->url().path()).fileName();
    if (name.endsWith(QLatin1String(".msi"), Qt::CaseInsensitive)) {
        const QString msi = QDir(fi.absolutePath()).filePath(name);
        if (QFile::rename(m_setupPath, msi)) {
            m_setupPath = msi;
        }
    }
    m_busy = false;
    setStatus(QStringLiteral("Downloaded. A helper must dwell Launch setup (UAC)."));
    emit finished(true, QStringLiteral(
                            "ViGEmBus setup downloaded. A helper is needed for the UAC prompt."));
}

bool VigemInstaller::hasPendingSetup() const
{
    return !m_setupPath.isEmpty() && QFileInfo::exists(m_setupPath);
}

void VigemInstaller::launchPendingSetup()
{
    if (!hasPendingSetup()) {
        fail(QStringLiteral("Download the ViGEmBus setup first."));
        return;
    }
    if (m_busy) {
        return;
    }
    launchSetup(m_setupPath);
}

void VigemInstaller::launchSetup(const QString& path)
{
#ifdef Q_OS_WIN
    const QString native = QDir::toNativeSeparators(path);
    QString verb = QStringLiteral("runas");
    QString file = native;
    QString params;
    if (native.endsWith(QLatin1String(".msi"), Qt::CaseInsensitive)) {
        file = QStringLiteral("msiexec");
        params = QStringLiteral("/i \"%1\"").arg(native);
    }
    const INT_PTR rc = reinterpret_cast<INT_PTR>(
        ShellExecuteW(nullptr, reinterpret_cast<LPCWSTR>(verb.utf16()),
                      reinterpret_cast<LPCWSTR>(file.utf16()),
                      params.isEmpty() ? nullptr : reinterpret_cast<LPCWSTR>(params.utf16()),
                      nullptr, SW_SHOWNORMAL));
    m_busy = false;
    m_status.clear();
    emit statusChanged();
    if (rc <= 32) {
        emit finished(false, QStringLiteral("Could not start ViGEm setup (code %1)").arg(int(rc)));
        return;
    }
    emit finished(true, QStringLiteral("ViGEmBus setup started. Finish the installer, then Recheck."));
#else
    Q_UNUSED(path);
    fail(QStringLiteral("ViGEm is only available on Windows"));
#endif
}

} // namespace gazer
