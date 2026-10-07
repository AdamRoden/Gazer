#pragma once

#include <QString>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

namespace gazer {
namespace CrashDump {

void installHandlers();
void registerRestart(const QString& extraArgs);
void unregisterRestart();

#ifdef Q_OS_WIN
/// Write a minidump for @p process (current or a hung peer). No Qt calls;
/// the caller caps the crash directory from a normal thread.
bool writeDump(HANDLE process, DWORD pid, EXCEPTION_POINTERS* exception = nullptr);
#endif

void capCrashDir(int keep = 10);

} // namespace CrashDump
} // namespace gazer
