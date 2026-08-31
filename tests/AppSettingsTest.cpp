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
    void appleSystemColorsFollowAppearance();
    void loadLegacyFourBrandRemapsToApple();
    void loadNamedSecondarySchemeWins();
    void loadNewSchemeWithoutSecondaryKeyKeepsIndex();
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
    QCOMPARE(s.themePrimaryIndex, kThemeDefaultBrandIndex);
    QCOMPARE(s.themeSecondaryIndex, kThemeDefaultBrandIndex);
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
    QCOMPARE(s.themePrimaryIndex, 3);
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
    s.setThemePrimaryIndex(kThemeDefaultBrandIndex);
    s.setThemeSecondaryIndex(kThemeDefaultBrandIndex);
    s.setThemeSaturation(kThemeSaturationDefault);
    QCOMPARE(s.themePrimaryIndex, 5);
    QCOMPARE(s.resolvedTheme().accent, QColor(0x0A, 0x84, 0xFF));
    QCOMPARE(s.resolvedPalette().progress, QColor(0x0A, 0x84, 0xFF));
    s.setThemeAppearance(ThemeAppearance::Light);
    QCOMPARE(s.resolvedTheme().accent, QColor(0x00, 0x7A, 0xFF));
    QCOMPARE(s.resolvedPalette().progress, QColor(0x00, 0x7A, 0xFF));
}

void AppSettingsTest::appleSystemColorsFollowAppearance()
{
    QCOMPARE(ThemeScheme::brandCount(), 9);
    QCOMPARE(ThemeScheme::brandAccent(0, ThemeAppearance::Light), QColor(0xFF, 0x3B, 0x30));
    QCOMPARE(ThemeScheme::brandAccent(0, ThemeAppearance::Dark), QColor(0xFF, 0x45, 0x3A));
    QCOMPARE(ThemeScheme::brandAccent(5, ThemeAppearance::Light), QColor(0x00, 0x7A, 0xFF));
    QCOMPARE(ThemeScheme::brandAccent(8, ThemeAppearance::Dark), QColor(0xFF, 0x37, 0x5F));
    QCOMPARE(QString::fromLatin1(ThemeScheme::brands()[0].name), QStringLiteral("Red"));
    QCOMPARE(QString::fromLatin1(ThemeScheme::brands()[5].key), QStringLiteral("blue"));
}

void AppSettingsTest::loadLegacyFourBrandRemapsToApple()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.json"));
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(R"({"themeAppearance":"dark","themeScheme":"blue","themePrimaryIndex":0,"themeSecondaryIndex":0})");
    f.close();

    AppSettings s;
    QVERIFY(s.loadFromFile(path));
    QCOMPARE(s.themeCustom, false);
    QCOMPARE(s.themePrimaryIndex, kThemeDefaultBrandIndex);
    QCOMPARE(s.themeSecondaryIndex, kThemeDefaultBrandIndex);
}

void AppSettingsTest::loadNamedSecondarySchemeWins()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.json"));
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(R"({"themeScheme":"blue","themeSecondaryScheme":"red","themePrimaryIndex":0,"themeSecondaryIndex":0})");
    f.close();

    AppSettings s;
    QVERIFY(s.loadFromFile(path));
    QCOMPARE(s.themePrimaryIndex, kThemeDefaultBrandIndex);
    QCOMPARE(s.themeSecondaryIndex, 0);
}

void AppSettingsTest::loadNewSchemeWithoutSecondaryKeyKeepsIndex()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.json"));
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(R"({"themeScheme":"yellow","themePrimaryIndex":2,"themeSecondaryIndex":2})");
    f.close();

    AppSettings s;
    QVERIFY(s.loadFromFile(path));
    QCOMPARE(s.themePrimaryIndex, 2);
    QCOMPARE(s.themeSecondaryIndex, 2);
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
