#include "app/SettingsUiInternal.h"
#include "ui/ColorField.h"

#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QtTest>

using namespace gazer;
using namespace gazer::SettingsUiInternal;

class ColorPickerHexTest final : public QObject {
    Q_OBJECT

private slots:
    void seedOmitsAlphaAtFullOpacity();
    void seedIncludesAlphaWhenTranslucent();
    void sixDigitsKeepAlpha();
    void eightDigitsSetAlpha();
    void sevenDigitsExpandNibble();
    void rejectShort();
    void hsvFieldCorners();
};

void ColorPickerHexTest::seedOmitsAlphaAtFullOpacity()
{
    QColor c(0x33, 0x33, 0x33, 255);
    QCOMPARE(hexSeedFromColor(c), QStringLiteral("333333"));
}

void ColorPickerHexTest::seedIncludesAlphaWhenTranslucent()
{
    QColor c(0x33, 0x33, 0x33, 128);
    QCOMPARE(hexSeedFromColor(c), QStringLiteral("80333333"));
}

void ColorPickerHexTest::sixDigitsKeepAlpha()
{
    const HexApplyResult r = parseHexDraft(QStringLiteral("#AABBCC"));
    QVERIFY(r.ok);
    QVERIFY(!r.setAlpha);
    QCOMPARE(r.rgb.red(), 0xAA);
    QCOMPARE(r.rgb.green(), 0xBB);
    QCOMPARE(r.rgb.blue(), 0xCC);
}

void ColorPickerHexTest::eightDigitsSetAlpha()
{
    const HexApplyResult r = parseHexDraft(QStringLiteral("80AABBCC"));
    QVERIFY(r.ok);
    QVERIFY(r.setAlpha);
    QCOMPARE(r.alpha, 0x80);
    QCOMPARE(r.rgb.red(), 0xAA);
    QCOMPARE(r.rgb.green(), 0xBB);
    QCOMPARE(r.rgb.blue(), 0xCC);
}

void ColorPickerHexTest::sevenDigitsExpandNibble()
{
    const HexApplyResult r = parseHexDraft(QStringLiteral("AABBCCF"));
    QVERIFY(r.ok);
    QVERIFY(r.setAlpha);
    QCOMPARE(r.alpha, 0xFF);
    QCOMPARE(r.rgb, QColor(0xAA, 0xBB, 0xCC));
}

void ColorPickerHexTest::rejectShort()
{
    QVERIFY(!parseHexDraft(QStringLiteral("ABC")).ok);
    QVERIFY(!parseHexDraft(QStringLiteral("AABBCCDDEE")).ok);
}

void ColorPickerHexTest::hsvFieldCorners()
{
    ColorField::Visual g;
    g.field = QRectF(0, 0, 200, 200);
    g.handleR = 0;
    double s = -1, v = -1;
    g.svAt(QPointF(0, 0), &s, &v);
    QCOMPARE(s, 0.0);
    QCOMPARE(v, 1.0);
    g.svAt(QPointF(200, 200), &s, &v);
    QCOMPARE(s, 1.0);
    QCOMPARE(v, 0.0);
    g.svAt(QPointF(100, 100), &s, &v);
    QCOMPARE(s, 0.5);
    QCOMPARE(v, 0.5);
}

QObject* createColorPickerHexTest()
{
    return new ColorPickerHexTest;
}

#include "ColorPickerHexTest.moc"
