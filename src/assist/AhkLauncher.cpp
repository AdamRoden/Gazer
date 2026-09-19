#include "assist/AhkLauncher.h"

#include "assist/ChildProcess.h"
#include "utils/Log.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSettings>
#include <QStandardPaths>
#include <QtGlobal>

namespace gazer {

namespace {

enum class AhkKind { V2_64, V2_32, Ux, V1, Other };

AhkKind classify(const QString& path)
{
    const QString n = QFileInfo(path).fileName().toLower();
    const QString p = QDir::fromNativeSeparators(path).toLower();
    const bool v1dir = p.contains(QLatin1String("/v1.1/")) || p.contains(QLatin1String("/v1/"));
    const bool v2dir = p.contains(QLatin1String("/v2/"));
    if (n == QLatin1String("autohotkeyu64.exe") || n == QLatin1String("autohotkeyu32.exe")
        || n == QLatin1String("autohotkeya64.exe") || n == QLatin1String("autohotkeya32.exe")) {
        return AhkKind::V1;
    }
    if (n == QLatin1String("autohotkey64.exe")) {
        return v1dir ? AhkKind::V1 : AhkKind::V2_64;
    }
    if (n == QLatin1String("autohotkey32.exe")) {
        return v1dir ? AhkKind::V1 : AhkKind::V2_32;
    }
    if (n == QLatin1String("autohotkey.exe")) {
        if (v1dir) {
            return AhkKind::V1;
        }
        if (v2dir) {
            return AhkKind::V2_64;
        }
        return AhkKind::Ux;
    }
    return AhkKind::Other;
}

int scoreKind(AhkKind k, bool wantV1)
{
    if (wantV1) {
        switch (k) {
        case AhkKind::V1:
            return 100;
        case AhkKind::Ux:
            return 80;
        case AhkKind::V2_64:
            return 20;
        case AhkKind::V2_32:
            return 10;
        case AhkKind::Other:
            break;
        }
        return 0;
    }
    switch (k) {
    case AhkKind::V2_64:
        return 100;
    case AhkKind::V2_32:
        return 90;
    case AhkKind::Ux:
        return 70;
    case AhkKind::V1:
        return 10;
    case AhkKind::Other:
        break;
    }
    return 0;
}

void appendIfFile(QStringList& out, const QString& path)
{
    const QString p = QDir::cleanPath(path);
    if (p.isEmpty() || out.contains(p)) {
        return;
    }
    out.push_back(p);
}

void addInstallTree(QStringList& out, const QString& dir)
{
    const QString d = QDir::cleanPath(dir);
    if (d.isEmpty()) {
        return;
    }
    appendIfFile(out, d + QStringLiteral("/v2/AutoHotkey64.exe"));
    appendIfFile(out, d + QStringLiteral("/v2/AutoHotkey32.exe"));
    appendIfFile(out, d + QStringLiteral("/AutoHotkey64.exe"));
    appendIfFile(out, d + QStringLiteral("/AutoHotkey.exe"));
    appendIfFile(out, d + QStringLiteral("/v1.1/AutoHotkeyU64.exe"));
    appendIfFile(out, d + QStringLiteral("/v1.1/AutoHotkey.exe"));
    appendIfFile(out, d + QStringLiteral("/AutoHotkeyU64.exe"));
}

void addRegistryInstallDir(QStringList& out, const QString& key)
{
    QSettings s(key, QSettings::NativeFormat);
    const QString dir = s.value(QStringLiteral("InstallDir")).toString().trimmed();
    if (!dir.isEmpty()) {
        addInstallTree(out, dir);
    }
    const QStringList groups = s.childGroups();
    for (const QString& g : groups) {
        s.beginGroup(g);
        const QString nested = s.value(QStringLiteral("InstallDir")).toString().trimmed();
        s.endGroup();
        if (!nested.isEmpty()) {
            addInstallTree(out, nested);
        }
    }
}

QString envDir(const QString& name)
{
    return QProcessEnvironment::systemEnvironment().value(name);
}

} // namespace

AhkLauncher::AhkLauncher(QObject* parent)
    : QObject(parent)
{
}

AhkLauncher::~AhkLauncher()
{
    ChildProcess::killChildren(this);
}

bool AhkLauncher::wantsV1(const QString& source)
{
    const QStringList lines = source.split(QLatin1Char('\n'));
    for (QString line : lines) {
        line = line.trimmed();
        if (!line.startsWith(QLatin1String("#Requires"), Qt::CaseInsensitive)) {
            continue;
        }
        const QString l = line.toLower();
        if (l.contains(QLatin1String("v1")) || l.contains(QLatin1String(" 1."))
            || l.contains(QLatin1String("autohotkey 1"))) {
            return true;
        }
        if (l.contains(QLatin1String("v2")) || l.contains(QLatin1String(" 2."))
            || l.contains(QLatin1String("autohotkey 2"))) {
            return false;
        }
    }
    return false;
}

QString AhkLauncher::findExecutableIn(const QStringList& candidates, const QString& source)
{
    const bool v1 = wantsV1(source);
    QString best;
    int bestScore = 0;
    for (const QString& raw : candidates) {
        const QString path = QDir::cleanPath(raw);
        if (path.isEmpty() || !QFileInfo::exists(path) || !QFileInfo(path).isFile()) {
            continue;
        }
        const int s = scoreKind(classify(path), v1);
        if (s > bestScore) {
            bestScore = s;
            best = path;
        }
    }
    return best;
}

QStringList AhkLauncher::systemCandidates()
{
    QStringList out;

    const QString forced = qEnvironmentVariable("GAZER_AHK").trimmed();
    if (!forced.isEmpty()) {
        appendIfFile(out, forced);
    }

#ifdef Q_OS_WIN
    addRegistryInstallDir(out, QStringLiteral("HKEY_CURRENT_USER\\SOFTWARE\\AutoHotkey"));
    addRegistryInstallDir(out, QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\AutoHotkey"));
    addRegistryInstallDir(out, QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\AutoHotkey"));
#endif

    addInstallTree(out, envDir(QStringLiteral("ProgramFiles")) + QStringLiteral("/AutoHotkey"));
    addInstallTree(out, envDir(QStringLiteral("ProgramFiles(x86)")) + QStringLiteral("/AutoHotkey"));
    addInstallTree(out, envDir(QStringLiteral("LOCALAPPDATA")) + QStringLiteral("/Programs/AutoHotkey"));

    for (const char* name : {"AutoHotkey64", "AutoHotkey32", "AutoHotkey"}) {
        const QString found = QStandardPaths::findExecutable(QString::fromLatin1(name));
        if (!found.isEmpty()) {
            appendIfFile(out, found);
        }
    }
    return out;
}

QString AhkLauncher::findExecutable(const QString& source, QString* error)
{
    if (m_overrideExe.has_value()) {
        const QString path = m_overrideExe->trimmed();
        if (path.isEmpty() || !QFileInfo::exists(path) || !QFileInfo(path).isFile()) {
            if (error) {
                *error = QStringLiteral("AutoHotkey is not installed");
            }
            return {};
        }
        return QDir::cleanPath(path);
    }

    const bool v1 = wantsV1(source);
    if (m_haveCache && m_cachedWantV1 == v1 && QFileInfo::exists(m_cachedExe)) {
        return m_cachedExe;
    }
    m_haveCache = false;
    m_cachedExe.clear();

    const QString env = qEnvironmentVariable("GAZER_AHK").trimmed();
    if (!env.isEmpty()) {
        if (QFileInfo::exists(env) && QFileInfo(env).isFile()) {
            m_cachedExe = QDir::cleanPath(env);
            m_cachedWantV1 = v1;
            m_haveCache = true;
            return m_cachedExe;
        }
        if (error) {
            *error = QStringLiteral("GAZER_AHK does not point to AutoHotkey");
        }
        return {};
    }

    const QString found = findExecutableIn(systemCandidates(), source);
    if (found.isEmpty()) {
        if (error) {
            *error = QStringLiteral("AutoHotkey is not installed");
        }
        return {};
    }
    m_cachedExe = found;
    m_cachedWantV1 = v1;
    m_haveCache = true;
    return found;
}

void AhkLauncher::setOverrideExecutable(const std::optional<QString>& path)
{
    m_overrideExe = path;
    m_haveCache = false;
    m_cachedExe.clear();
}

bool AhkLauncher::run(const QString& source, QString* error)
{
    if (source.trimmed().isEmpty()) {
        return true;
    }

    const QString exe = findExecutable(source, error);
    if (exe.isEmpty()) {
        return false;
    }

    const QString dirPath = QDir::temp().filePath(QStringLiteral("Gazer/ahk"));
    if (!QDir().mkpath(dirPath)) {
        if (error) {
            *error = QStringLiteral("Could not create AHK temp directory");
        }
        return false;
    }

    const qint64 pid = QCoreApplication::applicationPid();
    const QString scriptPath =
        QDir(dirPath).filePath(QStringLiteral("gazer-ahk-%1-%2.ahk").arg(pid).arg(++m_seq));

    QFile f(scriptPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = QStringLiteral("Could not write AHK temp script");
        }
        return false;
    }
    static const char kBom[] = "\xEF\xBB\xBF";
    f.write(kBom, 3);
    const QByteArray body = source.toUtf8();
    f.write(body);
    if (body.isEmpty() || body.back() != '\n') {
        f.write("\n", 1);
    }
    f.close();
    m_lastScriptPath = scriptPath;

    if (m_dryRun) {
        return true;
    }

    return ChildProcess::start(this, exe, {scriptPath}, dirPath, {}, QStringLiteral("AHK"),
                               [scriptPath](QProcess*) { QFile::remove(scriptPath); }, error)
           != nullptr;
}

} // namespace gazer
