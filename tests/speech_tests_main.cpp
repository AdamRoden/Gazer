#include "utils/Log.h"

#include <QCoreApplication>
#include <QStringList>
#include <QTest>
#include <memory>

Q_LOGGING_CATEGORY(lcGazer, "gazer")

QObject* createElevenRequestTest();
QObject* createComposeBufferTest();
QObject* createVoiceCatalogTest();
QObject* createSoundboardStoreTest();
QObject* createSpeechHistoryTest();

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
    std::unique_ptr<QObject> req(createElevenRequestTest());
    status |= QTest::qExec(req.get(), argc, argv);
    std::unique_ptr<QObject> buf(createComposeBufferTest());
    status |= QTest::qExec(buf.get(), rest);
    std::unique_ptr<QObject> voices(createVoiceCatalogTest());
    status |= QTest::qExec(voices.get(), rest);
    std::unique_ptr<QObject> board(createSoundboardStoreTest());
    status |= QTest::qExec(board.get(), rest);
    std::unique_ptr<QObject> hist(createSpeechHistoryTest());
    status |= QTest::qExec(hist.get(), rest);
    return status;
}
