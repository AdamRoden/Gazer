#include "utils/Log.h"

#include <QCoreApplication>
#include <QStringList>
#include <QTest>
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
QObject* createInboundActionsTest();
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
    std::unique_ptr<QObject> pages(createPageLoaderTest());
    status |= QTest::qExec(pages.get(), argc, argv);
    std::unique_ptr<QObject> pageActions(createPageLoaderActionTest());
    status |= QTest::qExec(pageActions.get(), rest);
    std::unique_ptr<QObject> dims(createPageDimTest());
    status |= QTest::qExec(dims.get(), rest);
    std::unique_ptr<QObject> nav(createPageNavTest());
    status |= QTest::qExec(nav.get(), rest);
    std::unique_ptr<QObject> hits(createPageHitTest());
    status |= QTest::qExec(hits.get(), rest);
    std::unique_ptr<QObject> hitLive(createPageHitLiveTest());
    status |= QTest::qExec(hitLive.get(), rest);
    std::unique_ptr<QObject> compose(createPageComposeTest());
    status |= QTest::qExec(compose.get(), rest);
    std::unique_ptr<QObject> settings(createSettingsLayoutTest());
    status |= QTest::qExec(settings.get(), rest);
    std::unique_ptr<QObject> appSettings(createAppSettingsTest());
    status |= QTest::qExec(appSettings.get(), rest);
    std::unique_ptr<QObject> materialPal(createMaterialPaletteTest());
    status |= QTest::qExec(materialPal.get(), rest);
    std::unique_ptr<QObject> mouseDwell(createMouseDwellMoveTest());
    status |= QTest::qExec(mouseDwell.get(), rest);
    std::unique_ptr<QObject> progress(createProgressPaintTest());
    status |= QTest::qExec(progress.get(), rest);
    std::unique_ptr<QObject> scroll(createScrollBarTest());
    status |= QTest::qExec(scroll.get(), rest);
    std::unique_ptr<QObject> combo(createComboMouseTest());
    status |= QTest::qExec(combo.get(), rest);
    std::unique_ptr<QObject> lts(createLookToScrollTest());
    status |= QTest::qExec(lts.get(), rest);
    std::unique_ptr<QObject> ahk(createAhkLauncherTest());
    status |= QTest::qExec(ahk.get(), rest);
    std::unique_ptr<QObject> inbound(createInboundActionsTest());
    status |= QTest::qExec(inbound.get(), rest);
    std::unique_ptr<QObject> keys(createKeyStateManagerTest());
    status |= QTest::qExec(keys.get(), rest);
    std::unique_ptr<QObject> colorHex(createColorPickerHexTest());
    status |= QTest::qExec(colorHex.get(), rest);
    std::unique_ptr<QObject> headPose(createHeadPoseMapperTest());
    status |= QTest::qExec(headPose.get(), rest);
    return status;
}
