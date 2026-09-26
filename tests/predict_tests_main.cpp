#include <QCoreApplication>
#include <QTest>
#include <memory>

QObject* createPredictTest();

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    const std::unique_ptr<QObject> test(createPredictTest());
    return QTest::qExec(test.get(), argc, argv);
}
