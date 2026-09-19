#include "app/ActionChannel.h"
#include "app/Application.h"
#include "app/InboundActions.h"
#include "ui/AppIcon.h"
#include "utils/Log.h"

#include <QApplication>
#include <QDateTime>
#include <QFile>
#include <QLoggingCategory>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTextStream>

#include <memory>

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
    qInstallMessageHandler(gazerMessageHandler);
    qputenv("QT_LOGGING_RULES", "gazer.*=true");

    // Overlay boards need an alpha buffer; software is the reliable path for
    // frameless translucent QQuickWindows on Windows.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    QQuickWindow::setDefaultAlphaBuffer(true);

    QApplication qapp(argc, argv);
    QApplication::setApplicationName(QStringLiteral("Gazer"));
    // Qt nests AppData as %AppData%\<org>\<app> when both are set. Leave org
    // empty so the on-disk tree is %AppData%\Gazer, not Gazer\Gazer.
    QApplication::setOrganizationName(QString());
    QApplication::setApplicationVersion(QStringLiteral("0.6.2"));
    // Tray owns lifetime; closing the preview must not quit.
    QApplication::setQuitOnLastWindowClosed(false);

    // Taskbar / Alt+Tab / window chrome for any QWidget that inherits the app icon.
    const QIcon appIcon = gazer::loadAppIcon();
    if (!appIcon.isNull()) {
        QApplication::setWindowIcon(appIcon);
    }

    const QStringList args = QCoreApplication::arguments();
    auto inbound = std::make_unique<gazer::ActionChannel>();
    if (!inbound->listen()) {
        QString err;
        if (!gazer::ActionChannel::sendToPeer(gazer::inboundForwardPayload(args), &err)) {
            fprintf(stderr, "%s\n", qPrintable(err));
            return 1;
        }
        return 0;
    }

    // File + stderr logging so expand/activate failures are visible without a debugger.
    // Open only as the live instance so a --action client does not truncate gazer.log.
    g_logFile.setFileName(QStringLiteral("gazer.log"));
    if (!g_logFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        fprintf(stderr, "Could not open gazer.log\n");
    }

    gazer::Application app;
    if (!app.initialize()) {
        GAZER_ERROR << "Failed to initialize Gazer";
        return 1;
    }
    app.takeInbound(std::move(inbound));

    const QString startup = gazer::inboundPayloadFromArgs(args);
    if (!startup.isEmpty()) {
        app.runInbound(startup);
    }

    return qapp.exec();
}
