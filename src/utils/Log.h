#pragma once

#include <QLoggingCategory>
#include <QString>

/// Gazer logging category. Defined once in main.cpp via Q_LOGGING_CATEGORY.
Q_DECLARE_LOGGING_CATEGORY(lcGazer)

// Stream-style logging (qCInfo and friends are statement macros — use only like this):
//   GAZER_INFO << "started with" << name;
#define GAZER_INFO  qCInfo(lcGazer)
#define GAZER_WARN  qCWarning(lcGazer)
#define GAZER_ERROR qCCritical(lcGazer)
#define GAZER_DEBUG qCDebug(lcGazer)

namespace gazer {

/// stderr plus the session file. Call once, before any GAZER_* line.
void installMessageHandler();

/// Copy a non-empty @p livePath into @p destDir as `<stem>-yyyyMMdd-HHmmss-zzz.log`
/// and keep the newest @p keep archives. Returns false when that copy fails;
/// the caller should append instead of truncating.
bool archiveSessionLog(const QString& livePath, const QString& destDir, int keep = 5);

/// Archive and open `%LOCALAPPDATA%\Gazer\logs\<fileName>` for this process.
/// @p fileName is a bare name (`gazer.log`, `guard.log`), not a path.
bool openSessionLog(const QString& fileName);

[[nodiscard]] QString sessionLogPath();

/// Open the log directory in Explorer.
[[nodiscard]] bool revealSessionLogs();

} // namespace gazer
