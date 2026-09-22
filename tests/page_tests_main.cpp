#include "utils/Log.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStringList>
#include <QTest>
#include <QTextStream>
#include <memory>

Q_LOGGING_CATEGORY(lcGazer, "gazer")

QObject* createPageLoaderTest();
QObject* createPageLoaderActionTest();
QObject* createPageDimTest();
QObject* createPageNavTest();
QObject* createPageHitTest();
QObject* createPageHitLiveTest();
QObject* createPageComposeTest();
QObject* createSettingsLayoutTest();
QObject* createAppSettingsTest();
QObject* createMaterialPaletteTest();
QObject* createMouseDwellMoveTest();
QObject* createProgressPaintTest();
QObject* createScrollBarTest();
QObject* createComboMouseTest();
QObject* createLookToScrollTest();
QObject* createAhkLauncherTest();
QObject* createSidecarHostTest();
QObject* createVirtualGamepadTest();
QObject* createInboundActionsTest();
QObject* createActionChannelTest();
QObject* createHeartbeatTest();
QObject* createInjectGateTest();
QObject* createWinProcessTest();
QObject* createActionLoopServiceTest();
QObject* createKeyStateManagerTest();
QObject* createColorPickerHexTest();
QObject* createHeadPoseMapperTest();

namespace {

QStringList argsWithoutDashO(int argc, char** argv)
{
    QStringList args;
    for (int i = 0; i < argc; ++i) {
        const QString a = QString::fromLocal8Bit(argv[i]);
        if (a == QLatin1String("-o")) {
            ++i;
            continue;
        }
        if (a.startsWith(QLatin1String("-o"))) {
            continue;
        }
        args.push_back(a);
    }
    return args;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    int status = 0;
    const QStringList rest = argsWithoutDashO(argc, argv);
    QFile suiteLog(QDir::temp().filePath(QStringLiteral("GazerPageTests-suites.txt")));
    (void)suiteLog.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
    auto runSuite = [&](const char* name, QObject* obj, bool first) {
        const int s = first ? QTest::qExec(obj, argc, argv) : QTest::qExec(obj, rest);
        QTextStream(&suiteLog) << name << ' ' << s << '\n';
        suiteLog.flush();
        status |= s;
    };
    std::unique_ptr<QObject> pages(createPageLoaderTest());
    runSuite("PageLoaderTest", pages.get(), true);
    std::unique_ptr<QObject> pageActions(createPageLoaderActionTest());
    runSuite("PageLoaderActionTest", pageActions.get(), false);
    std::unique_ptr<QObject> dims(createPageDimTest());
    runSuite("PageDimTest", dims.get(), false);
    std::unique_ptr<QObject> nav(createPageNavTest());
    runSuite("PageNavTest", nav.get(), false);
    std::unique_ptr<QObject> hits(createPageHitTest());
    runSuite("PageHitTest", hits.get(), false);
    std::unique_ptr<QObject> hitLive(createPageHitLiveTest());
    runSuite("PageHitLiveTest", hitLive.get(), false);
    std::unique_ptr<QObject> compose(createPageComposeTest());
    runSuite("PageComposeTest", compose.get(), false);
    std::unique_ptr<QObject> settings(createSettingsLayoutTest());
    runSuite("SettingsLayoutTest", settings.get(), false);
    std::unique_ptr<QObject> appSettings(createAppSettingsTest());
    runSuite("AppSettingsTest", appSettings.get(), false);
    std::unique_ptr<QObject> materialPal(createMaterialPaletteTest());
    runSuite("MaterialPaletteTest", materialPal.get(), false);
    std::unique_ptr<QObject> mouseDwell(createMouseDwellMoveTest());
    runSuite("MouseDwellMoveTest", mouseDwell.get(), false);
    std::unique_ptr<QObject> progress(createProgressPaintTest());
    runSuite("ProgressPaintTest", progress.get(), false);
    std::unique_ptr<QObject> scroll(createScrollBarTest());
    runSuite("ScrollBarTest", scroll.get(), false);
    std::unique_ptr<QObject> combo(createComboMouseTest());
    runSuite("ComboMouseTest", combo.get(), false);
    std::unique_ptr<QObject> lts(createLookToScrollTest());
    runSuite("LookToScrollTest", lts.get(), false);
    std::unique_ptr<QObject> ahk(createAhkLauncherTest());
    runSuite("AhkLauncherTest", ahk.get(), false);
    std::unique_ptr<QObject> sidecar(createSidecarHostTest());
    runSuite("SidecarHostTest", sidecar.get(), false);
    std::unique_ptr<QObject> gamepad(createVirtualGamepadTest());
    runSuite("VirtualGamepadTest", gamepad.get(), false);
    std::unique_ptr<QObject> inbound(createInboundActionsTest());
    runSuite("InboundActionsTest", inbound.get(), false);
    std::unique_ptr<QObject> channel(createActionChannelTest());
    runSuite("ActionChannelTest", channel.get(), false);
    std::unique_ptr<QObject> heartbeat(createHeartbeatTest());
    runSuite("HeartbeatTest", heartbeat.get(), false);
    std::unique_ptr<QObject> injectGate(createInjectGateTest());
    runSuite("InjectGateTest", injectGate.get(), false);
    std::unique_ptr<QObject> winProc(createWinProcessTest());
    runSuite("WinProcessTest", winProc.get(), false);
    std::unique_ptr<QObject> loops(createActionLoopServiceTest());
    runSuite("ActionLoopServiceTest", loops.get(), false);
    std::unique_ptr<QObject> keys(createKeyStateManagerTest());
    runSuite("KeyStateManagerTest", keys.get(), false);
    std::unique_ptr<QObject> colorHex(createColorPickerHexTest());
    runSuite("ColorPickerHexTest", colorHex.get(), false);
    std::unique_ptr<QObject> headPose(createHeadPoseMapperTest());
    runSuite("HeadPoseMapperTest", headPose.get(), false);
    return status;
}
