#include "app/Application.h"
#include "ui/AppIcon.h"
#include "utils/Log.h"

#include <QApplication>
#include <QDateTime>
#include <QFile>
#include <QLoggingCategory>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTextStream>

// Define the logging category declared in Log.h (for GAZER_* macros).
Q_LOGGING_CATEGORY(lcGazer, "gazer")

namespace {
QFile g_logFile;

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
    const QString line =
        QStringLiteral("%1 [%2] %3\n")
            .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                 QLatin1String(level), msg);
    fprintf(stderr, "%s", qPrintable(line));
    if (g_logFile.isOpen()) {
        QTextStream(&g_logFile) << line;
        g_logFile.flush();
    }
    Q_UNUSED(ctx);
}
} // namespace

int main(int argc, char* argv[])
{
    // File + stderr logging so expand/activate failures are visible without a debugger.
    g_logFile.setFileName(QStringLiteral("gazer.log"));
    g_logFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
    qInstallMessageHandler(gazerMessageHandler);
    qputenv("QT_LOGGING_RULES", "gazer.*=true");

    // Overlay boards need an alpha buffer; software is the reliable path for
    // frameless translucent QQuickWindows on Windows.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    QQuickWindow::setDefaultAlphaBuffer(true);

    QApplication qapp(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Gazer"));
    QApplication::setOrganizationName(QStringLiteral("Gazer"));
    QApplication::setApplicationVersion(QStringLiteral("0.5.0"));
    // Tray owns lifetime; closing the preview must not quit.
    QApplication::setQuitOnLastWindowClosed(false);

    // Taskbar / Alt+Tab / window chrome for any QWidget that inherits the app icon.
    const QIcon appIcon = gazer::loadAppIcon();
    if (!appIcon.isNull()) {
        QApplication::setWindowIcon(appIcon);
    }

    gazer::Application app;
    if (!app.initialize()) {
        GAZER_ERROR << "Failed to initialize Gazer";
        return 1;
    }

    return qapp.exec();
}
