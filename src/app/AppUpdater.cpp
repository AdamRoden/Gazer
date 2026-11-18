#include "app/AppUpdater.h"

#include "app/UpdateRelease.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QUrl>

#if defined(Q_OS_WIN)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <softpub.h>
#include <wincrypt.h>
#include <wintrust.h>

#include "update/SignatureIntact.h"
#endif

namespace gazer {

namespace {

constexpr int kFirstCheckMs = 45000;
constexpr int kCheckIntervalMs = 4 * 60 * 60 * 1000;
constexpr int kDownloadTimeoutMs = 10 * 60 * 1000;

QString programDataRoot()
{
    QString root = qEnvironmentVariable("ProgramData");
    if (root.isEmpty()) {
        root = QStringLiteral("C:/ProgramData");
    }
    return root;
}

QString updateDirectory()
{
    const QString dir = QDir(programDataRoot()).filePath(QStringLiteral("Gazer/update"));
    QDir().mkpath(dir);
    return dir;
}

QString pendingMsiPath()
{
    return QDir(updateDirectory()).filePath(QStringLiteral("pending.msi"));
}

QString pendingPartPath()
{
    return QDir(updateDirectory()).filePath(QStringLiteral("pending.msi.part"));
}

QString pendingJsonPath()
{
    return QDir(updateDirectory()).filePath(QStringLiteral("pending.json"));
}

bool msiMagic(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray bytes = file.read(8);
    static const unsigned char kMagic[] = {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1};
    return bytes.size() == 8 && memcmp(bytes.constData(), kMagic, 8) == 0;
}

#if defined(Q_OS_WIN)
QString signerThumbprint(const QString& path)
{
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        return {};
    }
    const std::wstring wide = path.toStdWString();
    HCERTSTORE store = nullptr;
    HCRYPTMSG msg = nullptr;
    if (!CryptQueryObject(CERT_QUERY_OBJECT_FILE, wide.c_str(),
                          CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
                          CERT_QUERY_FORMAT_FLAG_BINARY, 0, nullptr, nullptr, nullptr, &store,
                          &msg, nullptr)) {
        return {};
    }
    QString thumb;
    DWORD bytes = 0;
    if (CryptMsgGetParam(msg, CMSG_SIGNER_CERT_INFO_PARAM, 0, nullptr, &bytes) && bytes > 0) {
        QByteArray buf(int(bytes), 0);
        if (CryptMsgGetParam(msg, CMSG_SIGNER_CERT_INFO_PARAM, 0, buf.data(), &bytes)) {
            auto* info = reinterpret_cast<CERT_INFO*>(buf.data());
            PCCERT_CONTEXT cert = CertFindCertificateInStore(
                store, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_SUBJECT_CERT, info,
                nullptr);
            if (cert) {
                BYTE hash[20];
                DWORD hashLen = sizeof(hash);
                if (CertGetCertificateContextProperty(cert, CERT_SHA1_HASH_PROP_ID, hash,
                                                      &hashLen)) {
                    thumb = QByteArray(reinterpret_cast<const char*>(hash), int(hashLen)).toHex();
                }
                CertFreeCertificateContext(cert);
            }
        }
    }
    if (store) {
        CertCloseStore(store, 0);
    }
    if (msg) {
        CryptMsgClose(msg);
    }
    return thumb;
}

bool signatureCoversFile(const QString& path)
{
    const std::wstring wide = path.toStdWString();
    WINTRUST_FILE_INFO file{};
    file.cbStruct = sizeof(file);
    file.pcwszFilePath = wide.c_str();
    GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    WINTRUST_DATA data{};
    data.cbStruct = sizeof(data);
    data.dwUIChoice = WTD_UI_NONE;
    data.fdwRevocationChecks = WTD_REVOKE_NONE;
    data.dwUnionChoice = WTD_CHOICE_FILE;
    data.dwStateAction = WTD_STATEACTION_VERIFY;
    data.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;
    data.pFile = &file;
    const LONG status = WinVerifyTrust(nullptr, &action, &data);
    data.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(nullptr, &action, &data);
    return signatureIntact(status);
}
#endif

QString installedSigner()
{
#if defined(Q_OS_WIN)
    const QString dir = QCoreApplication::applicationDirPath();
    const QString helper = signerThumbprint(QDir(dir).filePath(QStringLiteral("GazerUpdate.exe")));
    if (!helper.isEmpty()) {
        return helper;
    }
    return signerThumbprint(QDir(dir).filePath(QStringLiteral("Gazer.exe")));
#else
    return {};
#endif
}

} // namespace

AppUpdater::AppUpdater(QObject* parent)
    : QObject(parent)
{
}

QString AppUpdater::helperPath() const
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("GazerUpdate.exe"));
}

bool AppUpdater::acceptableMsi(const QString& path) const
{
    if (!msiMagic(path)) {
        return false;
    }
#if defined(Q_OS_WIN)
    const QString expected = installedSigner();
    if (expected.isEmpty()) {
        return true;
    }
    return signatureCoversFile(path)
           && signerThumbprint(path).compare(expected, Qt::CaseInsensitive) == 0;
#else
    return true;
#endif
}

bool AppUpdater::taskExists() const
{
    if (m_taskQueried) {
        return m_taskFound;
    }
    m_taskQueried = true;
#if defined(Q_OS_WIN)
    QProcess query;
    query.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
        args->flags |= CREATE_NO_WINDOW;
    });
    query.start(QStringLiteral("schtasks"),
                {QStringLiteral("/Query"), QStringLiteral("/TN"),
                 QStringLiteral("Gazer\\ApplyUpdate")});
    if (!query.waitForFinished(8000)) {
        query.kill();
        m_taskFound = false;
        return false;
    }
    m_taskFound = query.exitCode() == 0;
#else
    m_taskFound = false;
#endif
    return m_taskFound;
}

QString AppUpdater::statusLine() const
{
    switch (m_phase) {
    case Phase::Checking:
        return QStringLiteral("Checking for updates");
    case Phase::Available:
        return QStringLiteral("Update %1 is available").arg(m_version);
    case Phase::Downloading:
        return QStringLiteral("Downloading update %1%").arg(m_percent);
    case Phase::Ready:
        return QStringLiteral("Update %1 is ready to restart Gazer").arg(m_version);
    case Phase::Failed:
        return m_error.isEmpty() ? QStringLiteral("The update did not finish") : m_error;
    case Phase::Idle:
        break;
    }
    return QStringLiteral("Gazer %1").arg(QCoreApplication::applicationVersion());
}

QString AppUpdater::promptCaption() const
{
    return setupNote();
}

QString AppUpdater::setupNote() const
{
    if (!taskExists()) {
        return QStringLiteral("Silent updates start after this version is installed once.");
    }
    return QStringLiteral("Gazer checks for updates in the background.");
}

void AppUpdater::setPhase(Phase phase)
{
    m_phase = phase;
    emit changed();
}

void AppUpdater::maybePrompt(bool user)
{
    if (m_phase != Phase::Available && m_phase != Phase::Ready && m_phase != Phase::Failed) {
        return;
    }
    if (user) {
        emit promptRequested();
        return;
    }
    if (!m_snoozedVersion.isEmpty() && m_snoozedVersion == m_version) {
        return;
    }
    if ((m_phase == Phase::Available || m_phase == Phase::Ready) && taskExists()) {
        emit promptRequested();
    }
}

void AppUpdater::start()
{
    scanPending();
    QTimer::singleShot(kFirstCheckMs, this, [this]() { check(false); });
    m_interval.setInterval(kCheckIntervalMs);
    connect(&m_interval, &QTimer::timeout, this, [this]() { check(false); });
    m_interval.start();
}

void AppUpdater::checkNow()
{
    if (m_phase == Phase::Downloading) {
        return;
    }
    if (m_phase == Phase::Available || m_phase == Phase::Ready || m_phase == Phase::Failed) {
        emit promptRequested();
    }
    check(true);
}

void AppUpdater::dismiss()
{
    if (!m_version.isEmpty()) {
        m_snoozedVersion = m_version;
    }
}

void AppUpdater::scanPending()
{
    const QString msi = pendingMsiPath();
    const QString jsonPath = pendingJsonPath();
    if (!QFileInfo::exists(msi)) {
        QFile::remove(jsonPath);
        return;
    }
    QFile jsonFile(jsonPath);
    if (!jsonFile.open(QIODevice::ReadOnly)) {
        QFile::remove(msi);
        return;
    }
    const QJsonObject object = QJsonDocument::fromJson(jsonFile.readAll()).object();
    const QString version = object.value(QStringLiteral("version")).toString();
    const VersionTriplet pending = parseVersionTriplet(version);
    const VersionTriplet current = parseVersionTriplet(QCoreApplication::applicationVersion());
    if (!pending.ok || !current.ok || compareVersions(pending, current) <= 0 || !acceptableMsi(msi)) {
        QFile::remove(msi);
        QFile::remove(jsonPath);
        return;
    }
    m_version = version;
    m_name = object.value(QStringLiteral("name")).toString();
    setPhase(Phase::Ready);
    maybePrompt(false);
}

void AppUpdater::check(bool user)
{
    if (m_phase == Phase::Downloading || m_reply) {
        return;
    }
    m_userCheck = user;
    if (m_phase != Phase::Available && m_phase != Phase::Ready) {
        setPhase(Phase::Checking);
    }
    const QUrl apiUrl(QStringLiteral(
        "https://api.github.com/repos/AdamRoden/Gazer/releases?per_page=20"));
    QNetworkRequest req(apiUrl);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Gazer"));
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_net.get(req);
    connect(m_reply, &QNetworkReply::finished, this, &AppUpdater::onCheckFinished);
}

void AppUpdater::onCheckFinished()
{
    QNetworkReply* reply = m_reply;
    m_reply = nullptr;
    if (!reply) {
        return;
    }
    const QByteArray body = reply->readAll();
    const QNetworkReply::NetworkError error = reply->error();
    const QString errorString = reply->errorString();
    reply->deleteLater();
    if (m_phase == Phase::Downloading) {
        return;
    }
    if (error != QNetworkReply::NoError) {
        if (m_phase == Phase::Available || m_phase == Phase::Ready) {
            if (m_userCheck) {
                emit statusMessage(QStringLiteral("Could not refresh the update check"));
                emit promptRequested();
            }
            emit changed();
            return;
        }
        m_error = errorString.isEmpty() ? QStringLiteral("Could not check for updates")
                                         : errorString;
        setPhase(Phase::Failed);
        if (m_userCheck) {
            emit promptRequested();
        }
        return;
    }
    const GazerMsiRelease release =
        newestGazerMsi(body, QCoreApplication::applicationVersion());
    if (!release.found || release.url.isEmpty()
        || !release.url.startsWith(QLatin1String("https://"))) {
        QFile::remove(pendingMsiPath());
        QFile::remove(pendingJsonPath());
        m_version.clear();
        m_url.clear();
        setPhase(Phase::Idle);
        if (m_userCheck) {
            emit statusMessage(QStringLiteral("Gazer %1 is up to date")
                                   .arg(QCoreApplication::applicationVersion()));
        }
        return;
    }
    m_version = release.version;
    m_name = release.name;
    m_url = release.url;
    const bool localReady = QFileInfo::exists(pendingMsiPath()) && acceptableMsi(pendingMsiPath());
    QFile jsonFile(pendingJsonPath());
    QString localVersion;
    if (localReady && jsonFile.open(QIODevice::ReadOnly)) {
        localVersion = QJsonDocument::fromJson(jsonFile.readAll())
                           .object()
                           .value(QStringLiteral("version"))
                           .toString();
    }
    if (localReady && localVersion == release.version) {
        setPhase(Phase::Ready);
    } else {
        setPhase(Phase::Available);
    }
    maybePrompt(m_userCheck);
}

void AppUpdater::beginDownload()
{
    if (m_phase == Phase::Downloading || m_url.isEmpty() || m_reply) {
        return;
    }
    const QUrl url(m_url);
    if (url.scheme() != QLatin1String("https")) {
        m_error = QStringLiteral("The update link is not secure.");
        setPhase(Phase::Failed);
        emit promptRequested();
        return;
    }
    QDir().mkpath(updateDirectory());
    QFile::remove(pendingPartPath());
    m_part.setFileName(pendingPartPath());
    if (!m_part.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_error = QStringLiteral("Could not save the update.");
        setPhase(Phase::Failed);
        emit promptRequested();
        return;
    }
    m_percent = 0;
    m_reportedPercent = 0;
    m_snoozedVersion.clear();
    setPhase(Phase::Downloading);
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Gazer"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_net.get(req);
    connect(m_reply, &QNetworkReply::readyRead, this, [this]() {
        if (m_reply && m_part.isOpen()) {
            m_part.write(m_reply->readAll());
        }
    });
    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        const int pct = total > 0 ? int((received * 100) / total) : 0;
        m_percent = qBound(0, pct, 100);
        if (m_percent == 100 || m_percent / 5 != m_reportedPercent / 5) {
            m_reportedPercent = m_percent;
            emit changed();
        }
    });
    QNetworkReply* reply = m_reply;
    QTimer::singleShot(kDownloadTimeoutMs, reply, [reply]() {
        if (reply->isRunning()) {
            reply->abort();
        }
    });
    connect(m_reply, &QNetworkReply::finished, this, &AppUpdater::onDownloadFinished);
}

void AppUpdater::onDownloadFinished()
{
    QNetworkReply* reply = m_reply;
    m_reply = nullptr;
    if (reply && m_part.isOpen()) {
        m_part.write(reply->readAll());
    }
    m_part.close();
    const QString part = m_part.fileName();
    const bool ok = reply && reply->error() == QNetworkReply::NoError;
    const QUrl finalUrl = reply ? reply->url() : QUrl();
    if (reply) {
        reply->deleteLater();
    }
    if (!ok || finalUrl.scheme() != QLatin1String("https") || !acceptableMsi(part)) {
        QFile::remove(part);
        m_error = QStringLiteral("The update download could not be verified.");
        setPhase(Phase::Failed);
        emit promptRequested();
        return;
    }
    const QString dest = pendingMsiPath();
    QFile::remove(dest);
    if (!QFile::rename(part, dest)) {
        QFile::remove(part);
        m_error = QStringLiteral("Could not store the update.");
        setPhase(Phase::Failed);
        emit promptRequested();
        return;
    }
    QJsonObject meta;
    meta.insert(QStringLiteral("version"), m_version);
    meta.insert(QStringLiteral("name"), m_name);
    QFile jsonFile(pendingJsonPath());
    if (jsonFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        jsonFile.write(QJsonDocument(meta).toJson(QJsonDocument::Compact));
    }
    setPhase(Phase::Ready);
    emit promptRequested();
}

bool AppUpdater::beginRestart()
{
    if (m_restarting) {
        return false;
    }
    if (m_phase != Phase::Ready) {
        m_blockReason = QStringLiteral("The update is not ready to restart Gazer.");
        return false;
    }
    const QString helper = helperPath();
    if (!taskExists() || !QFileInfo::exists(helper)) {
        m_blockReason = QStringLiteral(
            "Restart Gazer needs the update task from an installed copy of Gazer.");
        emit changed();
        return false;
    }
    const QString gazer =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("Gazer.exe"));
    qint64 pid = 0;
    const bool started = QProcess::startDetached(helper, {QStringLiteral("--finish"), gazer},
                                                 QString(), &pid);
    if (!started || pid == 0) {
        m_blockReason = QStringLiteral("Could not start the update helper.");
        emit changed();
        return false;
    }
    m_restarting = true;
    emit restarting();
    return true;
}

} // namespace gazer
