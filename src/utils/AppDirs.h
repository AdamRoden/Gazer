#pragma once

#include <QString>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

namespace gazer {
namespace AppDirs {

/// `%LOCALAPPDATA%\Gazer` (dumps, rotated logs, guard log).
[[nodiscard]] QString localRoot();
[[nodiscard]] QString crashDir();
[[nodiscard]] QString logDir();

#ifdef Q_OS_WIN
/// For the crash filter (no Qt). Writes the crash directory, creating it.
bool ensureCrashDirWide(wchar_t* out, size_t cap);
#endif

} // namespace AppDirs
} // namespace gazer
