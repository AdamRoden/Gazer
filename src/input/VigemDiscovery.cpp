#include "input/VigemDiscovery.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace gazer {

namespace {

void appendUnique(QStringList& out, const QString& path)
{
    const QString p = QDir::cleanPath(path);
    if (p.isEmpty() || out.contains(p, Qt::CaseInsensitive)) {
        return;
    }
    out.push_back(p);
}

void appendClientDll(QStringList& out, const QString& dir)
{
    if (dir.trimmed().isEmpty()) {
        return;
    }
    appendUnique(out, QDir(dir).filePath(QStringLiteral("ViGEmClient.dll")));
}

QString envDir(const char* name)
{
    return qEnvironmentVariable(name).trimmed();
}

void addNefariusClient(QStringList& out, const QString& programFiles)
{
    if (programFiles.isEmpty()) {
        return;
    }
    appendClientDll(out, QDir(programFiles).filePath(
                             QStringLiteral("Nefarius Software Solutions/ViGEm Client")));
}

} // namespace

QStringList VigemDiscovery::clientDllCandidates()
{
    QStringList out;
    const QString envDll = qEnvironmentVariable("GAZER_VIGEM_DLL").trimmed();
    if (!envDll.isEmpty()) {
        appendUnique(out, envDll);
    }
    if (!QCoreApplication::applicationDirPath().isEmpty()) {
        appendClientDll(out, QCoreApplication::applicationDirPath());
    }
    appendClientDll(out, QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    addNefariusClient(out, envDir("ProgramFiles"));
    addNefariusClient(out, envDir("ProgramW6432"));
    addNefariusClient(out, envDir("ProgramFiles(x86)"));
    return out;
}

QString VigemDiscovery::findClientDll()
{
    for (const QString& path : clientDllCandidates()) {
        if (QFileInfo::exists(path) && QFileInfo(path).isFile()) {
            return path;
        }
    }
    return {};
}

bool VigemDiscovery::busLooksInstalled()
{
    QSettings s(QStringLiteral("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\ViGEmBus"),
                QSettings::NativeFormat);
    return !s.value(QStringLiteral("ImagePath")).toString().isEmpty();
}

QString VigemDiscovery::describeInstall()
{
    const bool bus = busLooksInstalled();
    const QString dll = findClientDll();
    if (bus && !dll.isEmpty()) {
        return QStringLiteral("Ready — virtual Xbox pad available");
    }
    if (bus) {
        return QStringLiteral("Driver installed. ViGEmClient.dll is not next to Gazer.exe yet.");
    }
    if (!dll.isEmpty()) {
        return QStringLiteral("Client found. Install the ViGEmBus driver.");
    }
    return QStringLiteral("Not installed. Dwell Install to download the official driver setup.");
}

} // namespace gazer
