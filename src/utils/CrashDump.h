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
/// Write a minidump for @p process (current or a hung peer). Caps files in the crash dir.
bool writeDump(HANDLE process, DWORD pid);
#endif

void capCrashDir(int keep = 10);
void rotateLiveLog(const QString& livePath, int keep = 5);

} // namespace CrashDump
} // namespace gazer
