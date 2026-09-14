#include "app/SettingsUiInternal.h"
#include "ui/ColorField.h"
#include "ui/PickerPalette.h"

#include <QColor>
#include <QPoint>
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
    void pickerPaletteLayout();
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

void ColorPickerHexTest::pickerPaletteLayout()
{
    QCOMPARE(kPickerFamilyCount, 18);
    QCOMPARE(kPickerShadeCount, 11);
    QCOMPARE(kPickerPaletteCount, 198);
    QCOMPARE(kPickerShades[0], 50);
    QCOMPARE(kPickerShades[10], 950);
    QVERIFY(!pickerPaletteColor(-1).isValid());
    QVERIFY(!pickerPaletteColor(kPickerPaletteCount).isValid());
    QCOMPARE(pickerPaletteRowCol(-1), QPoint(-1, -1));
    QCOMPARE(pickerPaletteRowCol(0), QPoint(0, 10));
    QCOMPARE(pickerPaletteRowCol(10), QPoint(0, 0));
    QCOMPARE(pickerPaletteRowCol(11), QPoint(1, 10));
    QCOMPARE(pickerPaletteRowCol(21), QPoint(1, 0));
    QCOMPARE(pickerPaletteRowCol(kPickerPaletteCount - 1), QPoint(17, 0));
    const auto hex = [](const QColor& c) { return c.name(QColor::HexRgb).toLower(); };
    QCOMPARE(hex(pickerPaletteColor(0)), QStringLiteral("#fef2f2"));
    QCOMPARE(hex(pickerPaletteColor(5)), QStringLiteral("#fb2c36"));
    QCOMPARE(hex(pickerPaletteColor(10)), QStringLiteral("#460809"));
    QCOMPARE(hex(pickerPaletteColor(11)), QStringLiteral("#fff7ed"));
    QCOMPARE(hex(pickerPaletteColor(10 * 11 + 5)), QStringLiteral("#2b7fff"));
    QCOMPARE(hex(pickerPaletteColor(kPickerPaletteCount - 1)), QStringLiteral("#0a0a0a"));
    QCOMPARE(kPickerFamilies[0], "red");
    QCOMPARE(kPickerFamilies[17], "neutral");
    QCOMPARE(kPickerToneWeights[0], 5);
    QCOMPARE(kPickerToneWeights[10], 95);
    QCOMPARE(pickerPaletteToken(-1), QString());
    QCOMPARE(pickerPaletteToken(0), QStringLiteral("red05"));
    QCOMPARE(pickerPaletteToken(5), QStringLiteral("red50"));
    QCOMPARE(pickerPaletteToken(10), QStringLiteral("red95"));
    QCOMPARE(pickerPaletteToken(11), QStringLiteral("orange05"));
    QCOMPARE(pickerPaletteToken(kPickerPaletteCount - 1), QStringLiteral("neutral95"));
    QCOMPARE(pickerFamilyIndex(QStringLiteral("Blue")), 10);
    QCOMPARE(pickerShadeIndexForWeight(80), 8);
    QCOMPARE(pickerPaletteIndexAt(0, 5), 0);
    QCOMPARE(pickerPaletteIndexAt(0, 95), 10);
    QCOMPARE(hex(pickerPaletteColorAt(0, 5)), QStringLiteral("#fef2f2"));
    QCOMPARE(hex(pickerPaletteColorAt(0, 95)), QStringLiteral("#460809"));
}

QObject* createColorPickerHexTest()
{
    return new ColorPickerHexTest;
}

#include "ColorPickerHexTest.moc"
