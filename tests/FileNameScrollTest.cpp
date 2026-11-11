#include "ui/PhraseLayout.h"

#include <QtTest>

using namespace gazer;

class FileNameScrollTest final : public QObject {
    Q_OBJECT

private slots:
    void shortLineStaysPut();
    void middleOfLongLinePinsCaret();
    void endOfLongLineClamps();
    void caretIndexFollowsAdvances();
};

void FileNameScrollTest::shortLineStaysPut()
{
    QVERIFY(qFuzzyIsNull(pinnedFileScroll(40.0, 100.0, 200.0)));
}

void FileNameScrollTest::middleOfLongLinePinsCaret()
{
    const double scroll = pinnedFileScroll(200.0, 400.0, 200.0);
    QCOMPARE(scroll, 100.0);
    QCOMPARE(200.0 - scroll, 100.0);
}

void FileNameScrollTest::endOfLongLineClamps()
{
    const double scroll = pinnedFileScroll(400.0, 400.0, 200.0);
    QCOMPARE(scroll, 200.0);
    QCOMPARE(400.0 - scroll, 200.0);
}

void FileNameScrollTest::caretIndexFollowsAdvances()
{
    const double prefix[] = {0.0, 10.0, 20.0, 30.0};
    QCOMPARE(caretIndexAtAdvances(prefix, 4, 0.0), 0);
    QCOMPARE(caretIndexAtAdvances(prefix, 4, 4.0), 0);
    QCOMPARE(caretIndexAtAdvances(prefix, 4, 5.0), 1);
    QCOMPARE(caretIndexAtAdvances(prefix, 4, 100.0), 3);
    QCOMPARE(caretIndexAtAdvances(nullptr, 4, 10.0), 0);
}

QObject* createFileNameScrollTest()
{
    return new FileNameScrollTest;
}

#include "FileNameScrollTest.moc"
