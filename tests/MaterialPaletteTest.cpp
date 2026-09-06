#include "ui/MaterialPalette.h"

#include <QtTest>

using namespace gazer;

class MaterialPaletteTest final : public QObject {
    Q_OBJECT

private slots:
    void pink500MatchesSource();
    void pinkFamiliesMatchMaterialGenerator();
    void parseFamilyIds();
};

void MaterialPaletteTest::pink500MatchesSource()
{
    const QColor src(QStringLiteral("#E91E63"));
    const MaterialPalette::Palettes pal = MaterialPalette::generate(src);
    QCOMPARE(pal.primary[5].name(QColor::HexRgb).toLower(), QStringLiteral("#e91e63"));
}

void MaterialPaletteTest::pinkFamiliesMatchMaterialGenerator()
{
    const MaterialPalette::Palettes pal = MaterialPalette::generate(QColor(QStringLiteral("#E91E63")));
    const auto hex = [](const QColor& c) { return c.name(QColor::HexRgb).toLower(); };
    QCOMPARE(hex(pal.primary[0]), QStringLiteral("#fce4ec"));
    QCOMPARE(hex(pal.primary[9]), QStringLiteral("#880e4f"));
    QCOMPARE(hex(pal.complementary[5]), QStringLiteral("#00d97c"));
    QCOMPARE(hex(pal.analogous1[5]), QStringLiteral("#d100ac"));
    QCOMPARE(hex(pal.analogous2[5]), QStringLiteral("#ff4d27"));
    QCOMPARE(hex(pal.triadic1[5]), QStringLiteral("#ebe439"));
    QCOMPARE(hex(pal.triadic2[5]), QStringLiteral("#62e91e"));
}

void MaterialPaletteTest::parseFamilyIds()
{
    MaterialPalette::Family f = MaterialPalette::Family::Primary;
    QVERIFY(MaterialPalette::parseFamily(QStringLiteral("complementary"), &f));
    QCOMPARE(f, MaterialPalette::Family::Complementary);
    QVERIFY(MaterialPalette::parseFamily(QStringLiteral("analogous-2"), &f));
    QCOMPARE(f, MaterialPalette::Family::Analogous2);
    QCOMPARE(MaterialPalette::familyId(MaterialPalette::Family::Triadic1), "triadic1");
    QCOMPARE(MaterialPalette::kShades[0], 50);
    QCOMPARE(MaterialPalette::shadeIndexForWeight(700), 7);
}

QObject* createMaterialPaletteTest()
{
    return new MaterialPaletteTest;
}

#include "MaterialPaletteTest.moc"
