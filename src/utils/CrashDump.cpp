#include "utils/CrashDump.h"

#include "utils/AppDirs.h"
#include "utils/Log.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <atomic>
#include <csignal>
#include <exception>

#ifdef Q_OS_WIN
#  include <stdio.h>
#  include <string>
#endif

namespace gazer {
namespace CrashDump {

namespace {

std::atomic<bool> g_dumping{false};

void capDir(const QString& dir, const QStringList& filters, int keep)
{
    QDir d(dir);
    QFileInfoList files = d.entryInfoList(filters, QDir::Files, QDir::Time);
    for (int i = keep; i < files.size(); ++i) {
        QFile::remove(files.at(i).absoluteFilePath());
    }
}

#ifdef Q_OS_WIN
using MiniDumpWriteDumpFn = BOOL(WINAPI*)(HANDLE, DWORD, HANDLE, DWORD, void*, void*, void*);

MiniDumpWriteDumpFn loadDumper()
{
    static MiniDumpWriteDumpFn fn = nullptr;
    static bool tried = false;
    if (!tried) {
        tried = true;
        HMODULE lib = LoadLibraryW(L"dbghelp.dll");
        if (lib) {
            fn = reinterpret_cast<MiniDumpWriteDumpFn>(
                GetProcAddress(lib, "MiniDumpWriteDump"));
        }
    }
    return fn;
}

LONG WINAPI sehFilter(EXCEPTION_POINTERS*)
{
    if (g_dumping.exchange(true)) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    writeDump(GetCurrentProcess(), GetCurrentProcessId());
    g_dumping.store(false);
    return EXCEPTION_CONTINUE_SEARCH;
}

void terminateDump()
{
    if (g_dumping.exchange(true)) {
        return;
    }
    writeDump(GetCurrentProcess(), GetCurrentProcessId());
    g_dumping.store(false);
}

void onAbort(int)
{
    terminateDump();
    std::_Exit(3);
}
#endif

} // namespace

void capCrashDir(int keep)
{
    capDir(AppDirs::crashDir(), {QStringLiteral("*.dmp")}, keep);
}

void rotateLiveLog(const QString& livePath, int keep)
{
    QFileInfo fi(livePath);
    if (!fi.exists() || fi.size() <= 0) {
        return;
    }
    const QString destDir = AppDirs::logDir();
    const QString stamp =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString dest = QDir(destDir).filePath(QStringLiteral("gazer-%1.log").arg(stamp));
    QFile::copy(livePath, dest);
    capDir(destDir, {QStringLiteral("gazer-*.log")}, keep);
}

#ifdef Q_OS_WIN
bool writeDump(HANDLE process, DWORD pid)
{
    auto dump = loadDumper();
    if (!dump || !process) {
        return false;
    }
    wchar_t dir[MAX_PATH]{};
    if (!AppDirs::ensureCrashDirWide(dir, MAX_PATH)) {
        return false;
    }
    wchar_t path[MAX_PATH]{};
    SYSTEMTIME st{};
    GetLocalTime(&st);
    if (_snwprintf(path, MAX_PATH, L"%s\\gazer-%u-%04u%02u%02u-%02u%02u%02u.dmp", dir,
                   unsigned(pid), st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute,
                   st.wSecond)
        < 0) {
        return false;
    }
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    constexpr DWORD kType = 0x0000 | 0x0040; // MiniDumpNormal | WithIndirectlyReferencedMemory
    const BOOL ok = dump(process, pid, file, kType, nullptr, nullptr, nullptr);
    CloseHandle(file);
    return ok == TRUE;
}
#endif

void installHandlers()
{
#ifdef Q_OS_WIN
    SetUnhandledExceptionFilter(sehFilter);
    std::set_terminate(terminateDump);
    std::signal(SIGABRT, onAbort);
#endif
}

void registerRestart(const QString& extraArgs)
{
#ifdef Q_OS_WIN
    const QString cmd = extraArgs.trimmed();
    std::wstring w = cmd.toStdWString();
    const HRESULT hr = RegisterApplicationRestart(w.empty() ? nullptr : w.c_str(), 0);
    if (FAILED(hr)) {
        GAZER_WARN << "RegisterApplicationRestart failed" << quint32(hr);
    }
#else
    Q_UNUSED(extraArgs);
#endif
}

void unregisterRestart()
{
#ifdef Q_OS_WIN
    UnregisterApplicationRestart();
#endif
}

} // namespace CrashDump
} // namespace gazer
