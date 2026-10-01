#include "utils/Log.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStringList>
#include <QTest>
#include <QTextStream>
#include <memory>

Q_LOGGING_CATEGORY(lcGazer, "gazer")

QObject* createElevenRequestTest();
QObject* createComposeBufferTest();
QObject* createVoiceCatalogTest();
QObject* createSoundboardStoreTest();
QObject* createSpeechHistoryTest();
QObject* createAudioGainTest();
QObject* createSpeechStreamTest();

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
    QFile suiteLog(QDir::temp().filePath(QStringLiteral("GazerSpeechTests-suites.txt")));
    (void)suiteLog.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
    auto runSuite = [&](const char* name, QObject* obj, bool first) {
        const int s = first ? QTest::qExec(obj, argc, argv) : QTest::qExec(obj, rest);
        QTextStream(&suiteLog) << name << ' ' << s << '\n';
        suiteLog.flush();
        status |= s;
    };
    std::unique_ptr<QObject> req(createElevenRequestTest());
    runSuite("ElevenRequestTest", req.get(), true);
    std::unique_ptr<QObject> buf(createComposeBufferTest());
    runSuite("ComposeBufferTest", buf.get(), false);
    std::unique_ptr<QObject> voices(createVoiceCatalogTest());
    runSuite("VoiceCatalogTest", voices.get(), false);
    std::unique_ptr<QObject> board(createSoundboardStoreTest());
    runSuite("SoundboardStoreTest", board.get(), false);
    std::unique_ptr<QObject> hist(createSpeechHistoryTest());
    runSuite("SpeechHistoryTest", hist.get(), false);
    std::unique_ptr<QObject> gain(createAudioGainTest());
    runSuite("AudioGainTest", gain.get(), false);
    std::unique_ptr<QObject> stream(createSpeechStreamTest());
    runSuite("SpeechStreamTest", stream.get(), false);
    return status;
}
