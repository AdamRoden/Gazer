#include "utils/AppDirs.h"

#include <QDir>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#  include <shlobj.h>
#  include <stdio.h>
#endif

namespace gazer {
namespace AppDirs {

QString localRoot()
{
    const QString base =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QString dir = QDir(base).filePath(QStringLiteral("Gazer"));
    QDir().mkpath(dir);
    return dir;
}

QString crashDir()
{
    const QString dir = QDir(localRoot()).filePath(QStringLiteral("crashes"));
    QDir().mkpath(dir);
    return dir;
}

QString logDir()
{
    const QString dir = QDir(localRoot()).filePath(QStringLiteral("logs"));
    QDir().mkpath(dir);
    return dir;
}

#ifdef Q_OS_WIN
bool ensureCrashDirWide(wchar_t* out, size_t cap)
{
    if (!out || cap < 32) {
        return false;
    }
    wchar_t root[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, root))) {
        return false;
    }
    if (_snwprintf(out, cap, L"%s\\Gazer\\crashes", root) < 0) {
        return false;
    }
    const int shErr = SHCreateDirectoryExW(nullptr, out, nullptr);
    return shErr == ERROR_SUCCESS || shErr == ERROR_ALREADY_EXISTS || shErr == ERROR_FILE_EXISTS;
}
#endif

} // namespace AppDirs
} // namespace gazer
