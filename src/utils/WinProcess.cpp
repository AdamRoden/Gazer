#include "utils/WinProcess.h"

#include "utils/CrashDump.h"

#include <algorithm>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

namespace gazer {
namespace WinProcess {

#ifdef Q_OS_WIN
namespace {
QString baseNameFromPath(const wchar_t* path)
{
    const QString full = QString::fromWCharArray(path);
    const int slash =
        std::max(full.lastIndexOf(QLatin1Char('\\')), full.lastIndexOf(QLatin1Char('/')));
    return slash >= 0 ? full.mid(slash + 1) : full;
}
} // namespace
#endif

QString imageBase(quint32 pid)
{
#ifdef Q_OS_WIN
    if (pid == 0) {
        return {};
    }
    HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!proc) {
        return {};
    }
    wchar_t path[MAX_PATH]{};
    DWORD n = MAX_PATH;
    const BOOL ok = QueryFullProcessImageNameW(proc, 0, path, &n);
    CloseHandle(proc);
    return ok ? baseNameFromPath(path) : QString();
#else
    Q_UNUSED(pid);
    return {};
#endif
}

QString selfImageBase()
{
#ifdef Q_OS_WIN
    wchar_t path[MAX_PATH]{};
    const DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
    return n ? baseNameFromPath(path) : QString();
#else
    return {};
#endif
}

bool isGazerImage(quint32 pid)
{
    return imageBase(pid).compare(QLatin1String("Gazer.exe"), Qt::CaseInsensitive) == 0;
}

bool alive(quint32 pid)
{
#ifdef Q_OS_WIN
    if (pid == 0) {
        return false;
    }
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) {
        return false;
    }
    DWORD code = 0;
    const BOOL ok = GetExitCodeProcess(h, &code);
    CloseHandle(h);
    return ok && code == STILL_ACTIVE;
#else
    Q_UNUSED(pid);
    return false;
#endif
}

bool terminate(quint32 pid, QString* error)
{
#ifdef Q_OS_WIN
    if (pid == 0 || pid == GetCurrentProcessId()) {
        if (error) {
            *error = QStringLiteral("Refusing to terminate this process");
        }
        return false;
    }
    if (!isGazerImage(pid)) {
        if (error) {
            *error = QStringLiteral("Hung pid is not Gazer");
        }
        return false;
    }
    HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE
                                  | PROCESS_VM_READ,
                              FALSE, pid);
    if (!proc) {
        if (error) {
            *error = QStringLiteral("Could not open hung Gazer");
        }
        return false;
    }
    (void)CrashDump::writeDump(proc, pid);
    const BOOL ok = TerminateProcess(proc, 1);
    CloseHandle(proc);
    if (!ok) {
        if (error) {
            *error = QStringLiteral("Could not terminate hung Gazer");
        }
        return false;
    }
    return true;
#else
    Q_UNUSED(pid);
    if (error) {
        *error = QStringLiteral("Takeover is Windows-only");
    }
    return false;
#endif
}

} // namespace WinProcess
} // namespace gazer
