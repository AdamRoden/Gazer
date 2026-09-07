#include "layout/PageDim.h"

#include <QtTest>

using namespace gazer;

class PageDimTest final : public QObject {
    Q_OBJECT

private slots:
    void dimPixelsVsProportion();
    void dimFraction();
    void dimHeightRelative();
    void dimScreenExpression();
    void dimClampExpression();
    void expressionDoesNotInventMonitor();
    void placeRectBottom();
    void placeRectHeightSquareAtPoint();
};

void PageDimTest::dimPixelsVsProportion()
{
    QCOMPARE(PageDimParse::parse(QStringLiteral("150")).unit, PageDim::Unit::Pixels);
    QCOMPARE(PageDimParse::parse(QStringLiteral("150")).value, 150.0);
    QCOMPARE(PageDimParse::parse(QStringLiteral("0.5")).unit, PageDim::Unit::Proportion);
    QCOMPARE(PageDimParse::parse(QStringLiteral("0.5")).value, 0.5);
    QCOMPARE(PageDimParse::parse(QStringLiteral("1.0")).resolve(1920), 1920.0);
    QCOMPARE(PageDimParse::parse(QStringLiteral("1920")).resolve(100), 1920.0);
    QCOMPARE(PageDimParse::parse(QStringLiteral("2")).unit, PageDim::Unit::Pixels);
    QCOMPARE(PageDimParse::parse(QStringLiteral("2.0")).unit, PageDim::Unit::Proportion);
}

void PageDimTest::dimFraction()
{
    const PageDim d = PageDimParse::parse(QStringLiteral("-1/2"));
    QCOMPARE(d.unit, PageDim::Unit::Proportion);
    QCOMPARE(d.value, -0.5);
    const PageDimPair p = PageDimParse::parsePair(QStringLiteral("1/2,1/4"));
    QCOMPARE(p.x.resolve(200), 100.0);
    QCOMPARE(p.y.resolve(200), 50.0);
}

void PageDimTest::dimHeightRelative()
{
    const PageDim h = PageDimParse::parse(QStringLiteral("0.25h"));
    QCOMPARE(h.unit, PageDim::Unit::HeightProportion);
    QCOMPARE(h.value, 0.25);
    QCOMPARE(h.resolve(1920, 1080), 270.0);
    const PageDim frac = PageDimParse::parse(QStringLiteral("1/4h"));
    QCOMPARE(frac.unit, PageDim::Unit::HeightProportion);
    QCOMPARE(frac.resolve(800, 400), 100.0);
    QString err;
    QVERIFY(PageDimParse::parse(QStringLiteral("150h"), &err).unit == PageDim::Unit::Unset);
    QVERIFY(!err.isEmpty());
}

void PageDimTest::dimScreenExpression()
{
    const PageDim w = PageDimParse::parse(QStringLiteral("A_ScreenWidth"));
    QCOMPARE(w.unit, PageDim::Unit::Expression);
    QCOMPARE(w.resolve(0, 0, 2560, 1080), 2560.0);
    const PageDim h = PageDimParse::parse(QStringLiteral("A_ScreenHeight"));
    QCOMPARE(h.resolve(0, 0, 2560, 1080), 1080.0);
    const PageDim wide = PageDimParse::parse(QStringLiteral("A_ScreenHeight/9*16"));
    QCOMPARE(wide.unit, PageDim::Unit::Expression);
    QCOMPARE(wide.resolve(0, 0, 2560, 1080), 1920.0);
    const PageDimPair p = PageDimParse::parsePair(QStringLiteral("A_ScreenHeight/9*16, A_ScreenHeight"));
    QCOMPARE(p.x.resolve(2560, 1080, 2560, 1080), 1920.0);
    QCOMPARE(p.y.resolve(1080, 1080, 2560, 1080), 1080.0);
    QCOMPARE(PageDimParse::token(wide), QStringLiteral("A_ScreenHeight/9*16"));

    const QRectF ultra(0, 0, 2560, 1080);
    const QRectF r = PageDimParse::placeRect(ultra, PageAnchor::Center, {}, p, QSizeF(2560, 1080));
    QCOMPARE(r.width(), 1920.0);
    QCOMPARE(r.height(), 1080.0);
    QCOMPARE(r.left(), 320.0);

    QString err;
    QVERIFY(PageDimParse::parse(QStringLiteral("A_Nope"), &err).unit == PageDim::Unit::Unset);
    QVERIFY(err.contains(QStringLiteral("Unknown identifier")));
    QVERIFY(PageDimParse::parse(QStringLiteral("1/2")).unit == PageDim::Unit::Proportion);
}

void PageDimTest::dimClampExpression()
{
    const PageDim d = PageDimParse::parse(QStringLiteral("clamp(1.8*A_ScreenHeight, 1080, A_ScreenWidth)"));
    QCOMPARE(d.unit, PageDim::Unit::Expression);
    QCOMPARE(d.resolve(0, 0, 3440, 1440), 2592.0);
    QCOMPARE(d.resolve(0, 0, 1920, 1080), 1920.0);
    QCOMPARE(d.resolve(0, 0, 800, 600), 800.0);
    QCOMPARE(d.resolve(0, 0, 1280, 800), 1280.0);
    QCOMPARE(d.resolve(0, 0, 1920, 500), 1080.0);

    const PageDimPair p = PageDimParse::parsePair(
        QStringLiteral("clamp(1.8*A_ScreenHeight, 1080, A_ScreenWidth), A_ScreenHeight"));
    QCOMPARE(p.x.unit, PageDim::Unit::Expression);
    QCOMPARE(p.y.unit, PageDim::Unit::Expression);
    QCOMPARE(p.x.resolve(3440, 1440, 3440, 1440), 2592.0);
    QCOMPARE(p.y.resolve(1440, 1440, 3440, 1440), 1440.0);
    QCOMPARE(PageDimParse::token(p.x),
             QStringLiteral("clamp(1.8*A_ScreenHeight, 1080, A_ScreenWidth)"));

    const QRectF ultra(0, 0, 3440, 1440);
    const QRectF r = PageDimParse::placeRect(ultra, PageAnchor::Top, {}, p, QSizeF(3440, 1440));
    QCOMPARE(r.width(), 2592.0);
    QCOMPARE(r.height(), 1440.0);

    QString err;
    QVERIFY(PageDimParse::parse(QStringLiteral("clamp(1, 2)"), &err).unit == PageDim::Unit::Unset);
    QVERIFY(err.contains(QStringLiteral("3 arguments")));
    err.clear();
    QVERIFY(PageDimParse::parse(QStringLiteral("min(1, 2)"), &err).unit == PageDim::Unit::Unset);
    QVERIFY(err.contains(QStringLiteral("Unknown identifier")));
    err.clear();
    QVERIFY(PageDimParse::parsePair(QStringLiteral("1, 2, 3"), &err).x.unit == PageDim::Unit::Unset);
    QVERIFY(err.contains(QStringLiteral("Expected x,y pair")));
}

void PageDimTest::expressionDoesNotInventMonitor()
{
    const PageDim w = PageDimParse::parse(QStringLiteral("A_ScreenWidth"));
    QCOMPARE(w.resolve(1920, 1080), 0.0);
    QCOMPARE(w.resolve(1920, 1080, 0.0, 0.0), 0.0);
    const QRectF desk(0, 0, 1920, 1040);
    PageDimPair size;
    size.x = PageDimParse::parse(QStringLiteral("A_ScreenHeight/9*16"));
    size.y = PageDimParse::parse(QStringLiteral("A_ScreenHeight"));
    const QRectF r = PageDimParse::placeRect(desk, PageAnchor::Top, {}, size);
    QCOMPARE(r.width(), 1040.0 * 16.0 / 9.0);
    QCOMPARE(r.height(), 1040.0);
}

void PageDimTest::placeRectBottom()
{
    const QRectF bounds(0, 0, 1920, 1080);
    PageDimPair size;
    size.x = PageDim::pixels(300);
    size.y = PageDim::pixels(150);
    PageDimPair offset;
    offset.x = PageDim::pixels(0);
    offset.y = PageDim::pixels(0);
    const QRectF r = PageDimParse::placeRect(bounds, PageAnchor::Bottom, offset, size);
    QCOMPARE(r.width(), 300.0);
    QCOMPARE(r.height(), 150.0);
    QCOMPARE(r.left(), 810.0);
    QCOMPARE(r.top(), 930.0);
}

void PageDimTest::placeRectHeightSquareAtPoint()
{
    const QRectF desk(0, 0, 1920, 1040);
    PageDimPair size;
    size.x = PageDimParse::parse(QStringLiteral("0.25h"));
    size.y = PageDimParse::parse(QStringLiteral("0.25h"));
    const QPointF origin(1000, 500);
    PageDimPair offset;
    offset.x = PageDim::pixels(origin.x() - desk.center().x());
    offset.y = PageDim::pixels(origin.y() - desk.center().y());
    const QRectF r = PageDimParse::placeRect(desk, PageAnchor::Center, offset, size);
    QCOMPARE(r.width(), 260.0);
    QCOMPARE(r.height(), 260.0);
    QCOMPARE(r.center().x(), origin.x());
    QCOMPARE(r.center().y(), origin.y());
}

QObject* createPageDimTest()
{
    return new PageDimTest;
}

#include "PageDimTest.moc"
