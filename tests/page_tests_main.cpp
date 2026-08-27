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
QObject* createSettingsLayoutTest();
QObject* createComboMouseTest();
QObject* createAhkLauncherTest();
QObject* createKeyStateManagerTest();

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
    std::unique_ptr<QObject> settings(createSettingsLayoutTest());
    status |= QTest::qExec(settings.get(), rest);
    std::unique_ptr<QObject> combo(createComboMouseTest());
    status |= QTest::qExec(combo.get(), rest);
    std::unique_ptr<QObject> ahk(createAhkLauncherTest());
    status |= QTest::qExec(ahk.get(), rest);
    std::unique_ptr<QObject> keys(createKeyStateManagerTest());
    status |= QTest::qExec(keys.get(), rest);
    return status;
}
