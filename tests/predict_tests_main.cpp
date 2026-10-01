#include <QCoreApplication>
#include <QStringList>
#include <QTest>
#include <memory>

QObject* createPredictTest();
QObject* createCharPriorTest();

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
    const std::unique_ptr<QObject> predict(createPredictTest());
    int status = QTest::qExec(predict.get(), argc, argv);
    const std::unique_ptr<QObject> prior(createCharPriorTest());
    status |= QTest::qExec(prior.get(), argsWithoutDashO(argc, argv));
    return status;
}
