#include "utils/WinProcess.h"

#include <QtTest>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

using namespace gazer;

class WinProcessTest final : public QObject {
    Q_OBJECT

private slots:
    void refuseSelfTerminate();
    void thisProcessIsNotGazerImage();
};

void WinProcessTest::refuseSelfTerminate()
{
#ifdef Q_OS_WIN
    QString err;
    QVERIFY(!WinProcess::terminate(GetCurrentProcessId(), &err));
    QVERIFY(!err.isEmpty());
    QVERIFY(!WinProcess::terminate(0, &err));
#else
    QSKIP("Windows-only");
#endif
}

void WinProcessTest::thisProcessIsNotGazerImage()
{
#ifdef Q_OS_WIN
    QVERIFY(!WinProcess::isGazerImage(GetCurrentProcessId()));
    QVERIFY(WinProcess::alive(GetCurrentProcessId()));
#else
    QSKIP("Windows-only");
#endif
}

QObject* createWinProcessTest()
{
    return new WinProcessTest;
}

#include "WinProcessTest.moc"
