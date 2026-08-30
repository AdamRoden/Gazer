#include "app/AppSettings.h"
#include "ui/ThemeScheme.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace gazer;

class AppSettingsTest final : public QObject {
    Q_OBJECT

private slots:
    void dwellCustomDoesNotClobberUnmatched();
    void dwellCustomRestoresWhenLeavingPack();
    void brandedThemeUsesFluent();
    void customThemeUsesFluent();
    void loadLegacyNamedSchemeMapsToBrand();
    void loadLegacyCustom();
    void saveRoundTripCustomFlag();
    void progressAccentMatchesBrand();
    void saturationScalesCustomAccent();
    void lightAppearanceIsLight();
    void tintedWashesNeutrals();
    void appearanceSwitchesCustomNeutrals();
};

void AppSettingsTest::dwellCustomDoesNotClobberUnmatched()
{
    AppSettings s;
    s.setDwellPreset(0);
    QCOMPARE(s.dwellPreset(), 0);
    s.scanGraceMs += 50;
    QCOMPARE(s.dwellPreset(), 3);
    const int kept = s.scanGraceMs;
    s.setDwellPreset(3);
    QCOMPARE(s.scanGraceMs, kept);
    QCOMPARE(s.dwellPreset(), 3);
}

void AppSettingsTest::dwellCustomRestoresWhenLeavingPack()
{
    AppSettings s;
    s.setDwellPreset(0);
    s.saveDwellCustom();
    s.setDwellPreset(1);
    QCOMPARE(s.dwellPreset(), 1);
    s.setDwellPreset(3);
    QCOMPARE(s.dwellPreset(), 0);
}

void AppSettingsTest::brandedThemeUsesFluent()
{
    AppSettings s = AppSettings::defaults();
    QCOMPARE(s.themeCustom, false);
    const ThemePalette pal = ThemeScheme::resolve(
        s.themeAppearance, s.themeSaturation, s.themePrimaryIndex, s.themeSecondaryIndex, false);
    QCOMPARE(s.resolvedTheme().accent, pal.colors.accent);
    QCOMPARE(s.progressColor, AppSettings::colorToHex(pal.progress));
}

void AppSettingsTest::customThemeUsesFluent()
{
    AppSettings s = AppSettings::defaults();
    s.setThemeCustom(true);
    QVERIFY(s.themeCustom);
    const ThemePalette pal = ThemeScheme::fluent(s.themeAppearance, s.themeSaturation,
                                                 s.themeSeeds().primary, s.themeSeeds().secondary);
    QCOMPARE(s.resolvedTheme().accent, pal.colors.accent);
}

void AppSettingsTest::loadLegacyNamedSchemeMapsToBrand()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.json"));
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(R"({"themeMode":"dark","themeScheme":"meadow"})");
    f.close();

    AppSettings s;
    QVERIFY(s.loadFromFile(path));
    QCOMPARE(s.themeCustom, false);
    QCOMPARE(s.themePrimaryIndex, 1);
}

void AppSettingsTest::loadLegacyCustom()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.json"));
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(R"({"themeMode":"custom","customPrimaryColor":"#FF0000"})");
    f.close();

    AppSettings s;
    QVERIFY(s.loadFromFile(path));
    QVERIFY(s.themeCustom);
    QCOMPARE(s.themeSeeds().primary, QColor(QStringLiteral("#FF0000")));
}

void AppSettingsTest::saveRoundTripCustomFlag()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.json"));

    AppSettings s = AppSettings::defaults();
    s.setThemeCustom(true);
    QVERIFY(s.saveToFile(path));

    AppSettings loaded;
    QVERIFY(loaded.loadFromFile(path));
    QVERIFY(loaded.themeCustom);
    QCOMPARE(loaded.resolvedTheme().accent, s.resolvedTheme().accent);
}

void AppSettingsTest::progressAccentMatchesBrand()
{
    AppSettings s = AppSettings::defaults();
    s.setThemePrimaryIndex(0);
    s.setThemeSecondaryIndex(0);
    s.setThemeSaturation(kThemeSaturationDefault);
    QCOMPARE(s.resolvedTheme().accent, QColor(0x25, 0x63, 0xEB));
    QCOMPARE(s.resolvedPalette().progress, QColor(0x1D, 0x4E, 0xD8));
}

void AppSettingsTest::saturationScalesCustomAccent()
{
    AppSettings s = AppSettings::defaults();
    s.setThemeCustom(true);
    s.customPrimaryColor = QStringLiteral("#FF0000");
    s.setThemeSaturation(kThemeSaturationDefault);
    const QColor atDefault = s.resolvedTheme().accent;
    s.setThemeSaturation(kThemeSaturationMax);
    const QColor atMax = s.resolvedTheme().accent;
    QVERIFY(atMax.hsvSaturation() >= atDefault.hsvSaturation());
}

void AppSettingsTest::lightAppearanceIsLight()
{
    AppSettings s = AppSettings::defaults();
    s.setThemeAppearance(ThemeAppearance::Light);
    QVERIFY(s.resolvedTheme().bgMain.lightness() > 180);
    QVERIFY(s.resolvedTheme().text.lightness() < 80);
    s.setThemeAppearance(ThemeAppearance::LightTinted);
    QVERIFY(s.resolvedTheme().bgMain.lightness() > 180);
    QVERIFY(s.resolvedTheme().text.lightness() < 80);
    s.setThemeAppearance(ThemeAppearance::Dark);
    QVERIFY(s.resolvedTheme().bgMain.lightness() < 80);
    QVERIFY(s.resolvedTheme().text.lightness() > 180);
}

void AppSettingsTest::tintedWashesNeutrals()
{
    AppSettings s = AppSettings::defaults();
    s.setThemeAppearance(ThemeAppearance::Dark);
    const int darkSat = s.resolvedTheme().bgSurface.hsvSaturation();
    s.setThemeAppearance(ThemeAppearance::DarkTinted);
    QVERIFY(s.resolvedTheme().bgSurface.hsvSaturation() > darkSat + 8);
    QVERIFY(s.resolvedTheme().bgMain.lightness() < 80);

    s.setThemeAppearance(ThemeAppearance::Light);
    const int lightSat = s.resolvedTheme().bgSurface.hsvSaturation();
    s.setThemeAppearance(ThemeAppearance::LightTinted);
    QVERIFY(s.resolvedTheme().bgSurface.hsvSaturation() > lightSat + 8);
    QVERIFY(s.resolvedTheme().bgMain.lightness() > 180);
}

void AppSettingsTest::appearanceSwitchesCustomNeutrals()
{
    AppSettings s = AppSettings::defaults();
    s.setThemeCustom(true);
    QVERIFY(s.resolvedTheme().bgMain.lightness() < 80);
    s.setThemeAppearance(ThemeAppearance::Light);
    QVERIFY(s.themeCustom);
    QVERIFY(s.resolvedTheme().bgMain.lightness() > 180);
    QVERIFY(s.resolvedTheme().text.lightness() < 80);
}

QObject* createAppSettingsTest()
{
    return new AppSettingsTest;
}

#include "AppSettingsTest.moc"
