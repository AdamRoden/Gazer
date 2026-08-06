#pragma once

#include <QLoggingCategory>

/// Gazer logging category. Defined once in main.cpp via Q_LOGGING_CATEGORY.
Q_DECLARE_LOGGING_CATEGORY(lcGazer)

// Stream-style logging (qCInfo and friends are statement macros — use only like this):
//   GAZER_INFO << "started with" << name;
#define GAZER_INFO  qCInfo(lcGazer)
#define GAZER_WARN  qCWarning(lcGazer)
#define GAZER_ERROR qCCritical(lcGazer)
#define GAZER_DEBUG qCDebug(lcGazer)
