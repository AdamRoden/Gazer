#include "app/ComposeUiInternal.h"
#include "layout/PageTypes.h"
#include "ui/ScrollBar.h"

#include <QColor>
#include <QRectF>
#include <QtTest>

using namespace gazer;

class ScrollBarTest final : public QObject {
    Q_OBJECT

private slots:
    void parseAndFormatSpec();
    void maxOffsetAndT();
    void offsetAtYEnds();
    void offsetAtYMonotonic();
    void overlayIsPassiveTrack();
};

void ScrollBarTest::parseAndFormatSpec()
{
    QCOMPARE(ScrollBar::formatSpec(3, 10, 50), QStringLiteral("3,10,50"));
    const ScrollBar::Spec s = ScrollBar::parseSpec(QStringLiteral("3,10,50"));
    QCOMPARE(s.offset, 3);
    QCOMPARE(s.visible, 10);
    QCOMPARE(s.total, 50);
    const ScrollBar::Spec empty = ScrollBar::parseSpec({});
    QCOMPARE(empty.total, 0);
    QCOMPARE(empty.maxOffset(), 0);
}

void ScrollBarTest::maxOffsetAndT()
{
    ScrollBar::Spec s;
    s.offset = 0;
    s.visible = 10;
    s.total = 10;
    QCOMPARE(s.maxOffset(), 0);
    QCOMPARE(s.t(), 0.0);
    s.total = 50;
    s.offset = 20;
    QCOMPARE(s.maxOffset(), 40);
    QCOMPARE(s.t(), 0.5);
}

void ScrollBarTest::offsetAtYEnds()
{
    const QRectF cell(0, 0, 40, 400);
    ScrollBar::Spec s;
    s.visible = 10;
    s.total = 50;
    QCOMPARE(ScrollBar::offsetAtY(cell, cell.top(), s), 0);
    QCOMPARE(ScrollBar::offsetAtY(cell, cell.bottom(), s), s.maxOffset());
}

void ScrollBarTest::offsetAtYMonotonic()
{
    const QRectF cell(100, 50, 48, 500);
    ScrollBar::Spec s;
    s.visible = 10;
    s.total = 80;
    int prev = -1;
    for (int i = 0; i <= 10; ++i) {
        const double y = cell.top() + cell.height() * (i / 10.0);
        const int off = ScrollBar::offsetAtY(cell, y, s);
        QVERIFY(off >= prev);
        prev = off;
    }
    QCOMPARE(prev, s.maxOffset());
}

void ScrollBarTest::overlayIsPassiveTrack()
{
    const PageGrid g =
        compose_detail::overlayScrollbar(4, 10, 40, QColor(40, 40, 44));
    QCOMPARE(g.cells.size(), 1);
    QCOMPARE(g.subGrids.size(), 0);
    const PageCell& track = g.cells[0];
    QCOMPARE(track.id, QStringLiteral("sb_track"));
    QCOMPARE(track.role, QStringLiteral("scrollbar"));
    QCOMPARE(track.caption, QStringLiteral("4,10,40"));
    QVERIFY(!track.isInteractive());
    QVERIFY(track.actions.isEmpty());
}

QObject* createScrollBarTest()
{
    return new ScrollBarTest;
}

#include "ScrollBarTest.moc"
