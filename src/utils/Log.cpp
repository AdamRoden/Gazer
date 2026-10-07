#include "utils/Log.h"

#include "utils/AppDirs.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>
#include <QUrl>

#include <cstdio>

namespace gazer {
namespace {

QMutex g_logMu;
QFile g_logFile;

constexpr int kKeptSessions = 5;

void capArchives(const QString& dir, const QString& stem, int keep)
{
    QDir d(dir);
    const QFileInfoList files = d.entryInfoList({QStringLiteral("%1-*.log").arg(stem)},
                                                QDir::Files, QDir::Time);
    for (int i = keep; i < files.size(); ++i) {
        QFile::remove(files.at(i).absoluteFilePath());
    }
}

void writeLine(const QString& line)
{
    fprintf(stderr, "%s", qPrintable(line));
    QMutexLocker lock(&g_logMu);
    if (!g_logFile.isOpen()) {
        return;
    }
    QTextStream(&g_logFile) << line;
    g_logFile.flush();
}

void gazerMessageHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg)
{
    const char* level = "INFO";
    switch (type) {
    case QtDebugMsg:
        level = "DEBUG";
        break;
    case QtInfoMsg:
        level = "INFO";
        break;
    case QtWarningMsg:
        level = "WARN";
        break;
    case QtCriticalMsg:
        level = "ERROR";
        break;
    case QtFatalMsg:
        level = "FATAL";
        break;
    }
    const QString line = QStringLiteral("%1 [%2] %3\n")
                             .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                                  QLatin1String(level), msg);
    writeLine(line);
    Q_UNUSED(ctx);
}

} // namespace

void installMessageHandler()
{
    qInstallMessageHandler(gazerMessageHandler);
    qputenv("QT_LOGGING_RULES", "gazer.*=true");
}

bool archiveSessionLog(const QString& livePath, const QString& destDir, int keep)
{
    const QFileInfo fi(livePath);
    if (!fi.exists() || fi.size() <= 0) {
        return true;
    }
    QDir().mkpath(destDir);
    const QString stem = fi.completeBaseName();
    const QString stamp =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    QString dest = QDir(destDir).filePath(QStringLiteral("%1-%2.log").arg(stem, stamp));
    for (int n = 1; QFileInfo::exists(dest) && n < 100; ++n) {
        dest = QDir(destDir).filePath(QStringLiteral("%1-%2-%3.log").arg(stem, stamp).arg(n));
    }
    if (!QFile::copy(livePath, dest)) {
        return false;
    }
    capArchives(destDir, stem, keep);
    return true;
}

bool openSessionLog(const QString& fileName)
{
    const QString path = QDir(AppDirs::logDir()).filePath(fileName);
    QMutexLocker lock(&g_logMu);
    if (g_logFile.isOpen()) {
        g_logFile.close();
    }
    const bool kept = archiveSessionLog(path, AppDirs::logDir(), kKeptSessions);
    g_logFile.setFileName(path);
    const auto mode = kept ? QIODevice::Truncate : QIODevice::Append;
    if (!g_logFile.open(QIODevice::WriteOnly | mode | QIODevice::Text)) {
        fprintf(stderr, "Could not open %s\n", qPrintable(path));
        return false;
    }
    const QString header =
        QStringLiteral("%1 [INFO] session pid %2 log %3\n")
            .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                 QString::number(QCoreApplication::applicationPid()), path);
    QTextStream(&g_logFile) << header;
    g_logFile.flush();
    return true;
}

QString sessionLogPath()
{
    QMutexLocker lock(&g_logMu);
    return g_logFile.fileName();
}

bool revealSessionLogs()
{
    return QDesktopServices::openUrl(QUrl::fromLocalFile(AppDirs::logDir()));
}

} // namespace gazer
