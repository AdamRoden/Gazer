#include "app/AppSettings.h"
#include "assist/ComboMouseHit.h"
#include "assist/GazeFollowProfile.h"
#include "assist/LtsIndicator.h"
#include "assist/LtsScrollMode.h"
#include "assist/LtsSpeed.h"
#include "ui/PickStyle.h"
#include "ui/ThemeScheme.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <optional>

using namespace gazer;

class AppSettingsTest final : public QObject {
    Q_OBJECT

private slots:
    void defaultConstructIsFactory();
    void factoryUsesDomainConstants();
    void dwellCustomDoesNotClobberUnmatched();
    void dwellCustomRestoresWhenLeavingPack();
    void dwellPresetsSplitStandardAndRapid();
    void rapidFastFirstStepIsOneHundred();
    void parseDwellSequenceAllowsZero();
    void loadOmitsRapidKeepsDefault();
    void rapidDwellRoundTrip();
    void namedColorsResolveFromPalette();
    void toneTokensFollowSeed();
    void activeOffsetStepsDown();
    void customThemeUsesFluent();
    void secondaryColorIsSixtyPercent();
    void loadCustomThemeKeys();
    void saveRoundTripCustomFlag();
    void saturationScalesCustomAccent();
    void lightAppearanceIsLight();
    void tintedWashesNeutrals();
    void appearanceSwitchesCustomNeutrals();
    void backgroundShadeAndTintFamily();
    void speechSettingsRoundTrip();
    void headPoseMapsRoundTrip();
    void hoverBorderFollowsProgressAndRoundTrips();
    void showSplashRoundTrip();
};

void AppSettingsTest::defaultConstructIsFactory()
{
    const AppSettings a;
    const AppSettings b = AppSettings::defaults();
    QCOMPARE(a.dwellSequence, b.dwellSequence);
    QCOMPARE(a.rapidDwellSequence, b.rapidDwellSequence);
    QCOMPARE(a.scanGraceMs, b.scanGraceMs);
    QCOMPARE(a.magFollowProfile, b.magFollowProfile);
    QCOMPARE(a.progressColor, b.progressColor);
    QCOMPARE(a.comboInnerRadiusPx, b.comboInnerRadiusPx);
    QCOMPARE(a.speechModel, b.speechModel);
    QCOMPARE(a.themeSaturation, b.themeSaturation);
    QCOMPARE(a.savedSpeechTags, b.savedSpeechTags);
}

void AppSettingsTest::factoryUsesDomainConstants()
{
    const AppSettings s = AppSettings::defaults();
    const AppSettings::TimingPack pack = AppSettings::defaultTimingPack();
    QCOMPARE(s.dwellSequence, AppSettings::defaultDwellSequence());
    QCOMPARE(s.rapidDwellSequence, AppSettings::defaultRapidDwellSequence());
    QCOMPARE(s.customTiming.sequence, pack.sequence);
    QCOMPARE(s.customTiming.rapidSequence, pack.rapidSequence);
    QCOMPARE(s.mouseMoveDwellMs, pack.mouseMoveDwellMs);
    QCOMPARE(s.magPickDwellMs, pack.magPickDwellMs);
    QCOMPARE(s.dwellGraceMs, pack.blinkGraceMs);
    QCOMPARE(s.scanGraceMs, 100);
    QVERIFY(s.showSplash);
    QCOMPARE(s.magPickStyle, PickStyle::kDefaultMagPick);
    QCOMPARE(s.mousePickStyle, PickStyle::kDefaultMousePick);
    QCOMPARE(s.comboInnerRadiusPx, ComboMouseHit::kDefaultInnerRadiusPx);
    QCOMPARE(s.comboSharedRadiusPx, ComboMouseHit::kDefaultSharedRadiusPx);
    QCOMPARE(s.comboOuterRadiusPx, ComboMouseHit::kDefaultOuterRadiusPx);
    QCOMPARE(s.comboInnerColor, AppSettings::colorToHex(ComboMouseHit::kDefaultInnerFill));
    QCOMPARE(s.comboOuterColor, AppSettings::colorToHex(ComboMouseHit::kDefaultOuterFill));
    QCOMPARE(ComboMouseHit::kDefaultInnerFill.alpha(), qRound(0.20 * 255));
    QCOMPARE(ComboMouseHit::kDefaultOuterFill.alpha(), qRound(0.60 * 255));
    QCOMPARE(s.magFollowProfile, GazeFollowProfile::Sticky);
    QCOMPARE(s.ltsIndicatorStyle, LtsIndicator::Filled);
    QCOMPARE(s.ltsScrollMode, LtsScrollMode::Both);
    QCOMPARE(s.themeAppearance, ThemeAppearance::Dark);
    QCOMPARE(s.themeSaturation, kThemeSaturationDefault);
    QCOMPARE(s.themeBrightness, kThemeBrightnessDefault);
    QCOMPARE(s.themeTintFamily, ThemeTintFamily::None);
    QCOMPARE(s.resolvedTheme().bgMain, QColor(0x0A, 0x0A, 0x0A));
    QCOMPARE(s.resolvedTheme().accent, QColor(0x1E, 0x97, 0xF3));
    QCOMPARE(s.resolvedTheme().accentHover,
             ThemeScheme::fluent(s.themeAppearance, s.themeSaturation, s.themeSeeds().primary,
                                 s.themeSeeds().secondary)
                 .colors.accentHover);
    QCOMPARE(s.resolvedTheme().danger,
             ThemeScheme::fluent(s.themeAppearance, s.themeSaturation, s.themeSeeds().primary,
                                 s.themeSeeds().secondary)
                 .colors.danger);
    QCOMPARE(s.resolvedPalette().progress, QColor(0xFF, 0x47, 0x3D, kProgressFillAlpha));
    QCOMPARE(s.resolvedPalette().progressFill, QColor(0xFF, 0x47, 0x3D, kProgressFillAlpha));
    QCOMPARE(s.progressColor, QStringLiteral("#99FF473D"));
    QCOMPARE(s.progressFillColor, QStringLiteral("#99FF473D"));
    QCOMPARE(s.hoverColor, QStringLiteral("#99FF473D"));
    QCOMPARE(s.hoverBorderWeight, 4);
    QCOMPARE(s.flashMs, 60);
    QCOMPARE(s.pickWindowRound, false);
    QCOMPARE(s.ltsDeadzonePx, 80);
    QCOMPARE(s.ltsFalloffPx, 300);
    QCOMPARE(s.ltsAccelPerSec, kLtsAccelDefault);
    QCOMPARE(s.ltsMaxNotchesPerSec, kLtsSpeedDefault);
    QVERIFY(!s.hoverCustom);
    QVERIFY(!s.flashCustom);
    QCOMPARE(s.resolvedHoverBorder(), s.colorKey(QStringLiteral("progressColor")));
    QCOMPARE(s.speechModel, QStringLiteral("sapi"));
    QCOMPARE(s.speechVolume, 1.0);
    QCOMPARE(s.savedSpeechTags, AppSettings::defaultSavedSpeechTags());
    QVERIFY(s.savedSpeechVoices.isEmpty());
    QVERIFY(s.progress.radial);
    QVERIFY(!s.progress.pie);
    QVERIFY(s.mouseProgress.radial);
    QVERIFY(!s.mouseProgress.pie);
    QCOMPARE(s.dwellPreset(), 1);
}

void AppSettingsTest::dwellCustomDoesNotClobberUnmatched()
{
    AppSettings s;
    s.setDwellPreset(0);
    QCOMPARE(s.dwellPreset(), 0);
    s.dwellGraceMs += 50;
    QCOMPARE(s.dwellPreset(), 3);
    const int kept = s.dwellGraceMs;
    s.setDwellPreset(3);
    QCOMPARE(s.dwellGraceMs, kept);
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

void AppSettingsTest::dwellPresetsSplitStandardAndRapid()
{
    AppSettings s;
    QCOMPARE(s.dwellPreset(), 1);
    QCOMPARE(s.dwellSequence, (QVector<int>{800, 700, 600, 500, 400, 200}));
    QCOMPARE(s.rapidDwellSequence, AppSettings::defaultRapidDwellSequence());
    QCOMPARE(s.scanGraceMs, 100);

    s.scanGraceMs = 80;
    s.setDwellPreset(0);
    QCOMPARE(s.dwellPreset(), 0);
    QCOMPARE(s.dwellSequence, (QVector<int>{1200, 1000, 800, 600, 400}));
    QCOMPARE(s.rapidDwellSequence, (QVector<int>{800, 700, 600, 500, 400, 200}));
    QCOMPARE(s.scanGraceMs, 80);

    s.setDwellPreset(2);
    QCOMPARE(s.dwellPreset(), 2);
    QCOMPARE(s.dwellSequence, (QVector<int>{400, 600, 400, 250, 150, 50}));
    QCOMPARE(s.rapidDwellSequence, (QVector<int>{100, 600, 400, 250, 150, 50}));
    QCOMPARE(s.scanGraceMs, 80);
}

void AppSettingsTest::rapidFastFirstStepIsOneHundred()
{
    AppSettings s;
    s.setDwellPreset(2);
    s.clamp();
    QCOMPARE(s.rapidDwellSequence.front(), 100);
    QCOMPARE(s.dwellPreset(), 2);
}

void AppSettingsTest::parseDwellSequenceAllowsZero()
{
    QString err;
    const QVector<int> seq = AppSettings::parseDwellSequence(QStringLiteral("0,600,400"), &err);
    QCOMPARE(seq, (QVector<int>{0, 600, 400}));
    QVERIFY(err.isEmpty());
}

void AppSettingsTest::loadOmitsRapidKeepsDefault()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.json"));
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(R"({"dwellSequence":[1200,1000,800,600,400],"scanGraceMs":200,"dwellGraceMs":250,"mouseMoveDwellMs":1200,"magPickDwellMs":1200})");
    f.close();

    AppSettings s;
    QVERIFY(s.loadFromFile(path));
    QCOMPARE(s.dwellSequence, (QVector<int>{1200, 1000, 800, 600, 400}));
    QCOMPARE(s.rapidDwellSequence, AppSettings::defaultRapidDwellSequence());
    QCOMPARE(s.scanGraceMs, 200);
    QCOMPARE(s.magPickDwellMs, 1200);
    QCOMPARE(s.dwellPreset(), 3);
}

void AppSettingsTest::rapidDwellRoundTrip()
{
    AppSettings s;
    s.setDwellPreset(2);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.json"));
    QVERIFY(s.saveToFile(path));
    QFile jsonFile(path);
    QVERIFY(jsonFile.open(QIODevice::ReadOnly));
    const QByteArray json = jsonFile.readAll();
    jsonFile.close();
    QVERIFY(json.contains("\"rapidDwellSequence\""));
    QVERIFY(json.contains("\"customRapidDwellSequence\""));
    QVERIFY(!json.contains("\"customrapidDwellSequence\""));
    QVERIFY(!json.contains("\"themeCustom\""));
    QVERIFY(!json.contains("\"themeMode\""));
    QVERIFY(!json.contains("\"themeScheme\""));
    AppSettings b;
    QVERIFY(b.loadFromFile(path));
    QCOMPARE(b.dwellPreset(), 2);
    QCOMPARE(b.rapidDwellSequence, (QVector<int>{100, 600, 400, 250, 150, 50}));
    QCOMPARE(b.scanGraceMs, 100);
}

void AppSettingsTest::namedColorsResolveFromPalette()
{
    AppSettings s = AppSettings::defaults();
    s.applyTheme();
    const ThemePalette pal = s.resolvedPalette();
    const ThemeColors t = s.resolvedTheme();
    QCOMPARE(t.progress, pal.progress);
    QCOMPARE(t.resolveToken(QStringLiteral("progress")).value_or(QColor()), pal.progress);
    QCOMPARE(t.resolveToken(QStringLiteral("accent")).value_or(QColor()), t.accent);
    QCOMPARE(t.resolveToken(QStringLiteral("foreground")).value_or(QColor()), t.text);
    QCOMPARE(t.resolveToken(QStringLiteral("background")).value_or(QColor()), t.bgMain);
    QVERIFY(ThemeColors::isNamedColor(QStringLiteral("bg100")));
    QVERIFY(ThemeColors::isNamedColor(QStringLiteral("accent100")));
    QVERIFY(ThemeColors::isNamedColor(QStringLiteral("bg80")));
    QVERIFY(ThemeColors::isNamedColor(QStringLiteral("bg05")));
    QVERIFY(ThemeColors::isNamedColor(QStringLiteral("accent95")));
    QVERIFY(ThemeColors::isNamedColor(QStringLiteral("accent")));
    QVERIFY(ThemeColors::isNamedColor(QStringLiteral("danger")));
    QVERIFY(ThemeColors::isNamedColor(QStringLiteral("red80")));
    QVERIFY(ThemeColors::isNamedColor(QStringLiteral("red05")));
    QVERIFY(ThemeColors::isNamedColor(QStringLiteral("neutral95")));
    QVERIFY(!ThemeColors::isNamedColor(QStringLiteral("red")));
    QVERIFY(!ThemeColors::isNamedColor(QStringLiteral("red100")));
    QVERIFY(!ThemeColors::isNamedColor(QStringLiteral("surface")));
    QVERIFY(!ThemeColors::isNamedColor(QStringLiteral("tertiary")));
    QVERIFY(!ThemeColors::isNamedColor(QStringLiteral("window")));
    QVERIFY(!ThemeColors::isNamedColor(QStringLiteral("primary")));
    QCOMPARE(t.resolveToken(QStringLiteral("bg100")).value_or(QColor()), t.bgMain);
    QCOMPARE(t.resolveToken(QStringLiteral("accent100")).value_or(QColor()), t.accent);
    QCOMPARE(t.resolveToken(QStringLiteral("bg90")).value_or(QColor()), t.bgAt(90));
    QCOMPARE(t.resolveToken(QStringLiteral("accent50")).value_or(QColor()), t.accentAt(50));
    QCOMPARE(t.resolveToken(QStringLiteral("red05")).value_or(QColor()).name(QColor::HexRgb),
             QStringLiteral("#fef2f2"));
    QCOMPARE(t.resolveToken(QStringLiteral("red95")).value_or(QColor()).name(QColor::HexRgb),
             QStringLiteral("#460809"));
    QCOMPARE(t.resolveToken(QStringLiteral("red50")).value_or(QColor()).name(QColor::HexRgb),
             QStringLiteral("#fb2c36"));
    const QColor red80 = t.resolveToken(QStringLiteral("red80")).value_or(QColor());
    const QColor red70 = t.resolveToken(QStringLiteral("red70")).value_or(QColor());
    QCOMPARE(t.resolveFill(QStringLiteral("red80"), t.bgMain, true, false), red70);
    QCOMPARE(t.resolveFill(QStringLiteral("red80"), t.bgMain, false, false), red80);
}

void AppSettingsTest::toneTokensFollowSeed()
{
    AppSettings s = AppSettings::defaults();
    const ThemeColors dark = s.resolvedTheme();
    const QColor bg80Dark = dark.bgAt(80);
    s.setThemeBrightness(4);
    const ThemeColors lifted = s.resolvedTheme();
    QVERIFY(lifted.bgMain.lightness() > dark.bgMain.lightness());
    QVERIFY(lifted.bgAt(80).lightness() > bg80Dark.lightness());

    const QColor local = dark.resolveToken(QStringLiteral("bg80"), dark.accent).value_or(QColor());
    QVERIFY(local.isValid());
    QVERIFY(local != dark.bgAt(80));

    s.setThemeAppearance(ThemeAppearance::DarkTinted);
    QVERIFY(s.resolvedTheme().bgMain.hsvSaturation() > dark.bgMain.hsvSaturation());
}

void AppSettingsTest::activeOffsetStepsDown()
{
    QCOMPARE(ThemeColors::toneRef(QStringLiteral("bg100")).family, QStringLiteral("bg"));
    QCOMPARE(ThemeColors::toneRef(QStringLiteral("bg100")).weight, 100);
    QCOMPARE(ThemeColors::toneRef(QStringLiteral("accent80")).family, QStringLiteral("accent"));
    QCOMPARE(ThemeColors::toneRef(QStringLiteral("accent80")).weight, 80);
    QCOMPARE(ThemeColors::toneRef(QStringLiteral("background")).family, QStringLiteral("bg"));
    QCOMPARE(ThemeColors::toneRef(QString()).family, QStringLiteral("bg"));
    QCOMPARE(ThemeColors::toneRef(QStringLiteral("accent")).family, QStringLiteral("accent"));
    QVERIFY(ThemeColors::toneRef(QStringLiteral("#112233")).family.isEmpty());
    QCOMPARE(ThemeColors::steppedWeight(100, 1), 95);
    QCOMPARE(ThemeColors::steppedWeight(80, 1), 70);

    AppSettings s = AppSettings::defaults();
    const ThemeColors t = s.resolvedTheme();
    const QColor rest = t.resolveFill(QStringLiteral("bg80"), t.bgMain, false, false);
    const QColor active = t.resolveFill(QStringLiteral("bg80"), t.bgMain, false, true);
    QCOMPARE(rest, t.bgAt(80));
    QCOMPARE(active,
             ThemeColors::mix(ThemeColors::shadeTowardBlack(rest, kHoverToneSteps, 80), t.accent,
                              kActiveAccentMix));
    QVERIFY(ThemeColors::shadeTowardBlack(rest, kHoverToneSteps, 80).lightness() < rest.lightness());
    QCOMPARE(t.resolveToken(QStringLiteral("bg100")).value_or(QColor()), t.bgAt(100));
    QCOMPARE(t.defaultActive(),
             ThemeColors::mix(ThemeColors::shadeTowardBlack(t.bgMain, kHoverToneSteps), t.accent,
                              kActiveAccentMix));
    QCOMPARE(t.resolveFill({}, t.bgMain, false, false), t.bgMain);
    const QColor darkerBg = ThemeColors::shadeTowardBlack(t.bgMain, kHoverToneSteps);
    QCOMPARE(t.resolveFill(QStringLiteral("accent80"), t.bgMain, false, true), darkerBg);
    QCOMPARE(t.resolveFill(QStringLiteral("red80"), t.bgMain, false, true), darkerBg);
    const QColor neuRest = t.resolveFill(QStringLiteral("neutral80"), t.bgMain, false, false);
    const QColor neuActive = t.resolveFill(QStringLiteral("neutral80"), t.bgMain, false, true);
    QCOMPARE(neuActive,
             ThemeColors::mix(ThemeColors::shadeTowardBlack(neuRest, kHoverToneSteps, 80), t.accent,
                              kActiveAccentMix));

    s.setThemeAppearance(ThemeAppearance::Light);
    const ThemeColors light = s.resolvedTheme();
    const QColor lightRest = light.resolveFill(QStringLiteral("bg80"), light.bgMain, false, false);
    QVERIFY(ThemeColors::shadeTowardBlack(lightRest, kHoverToneSteps, 80).lightness()
            < lightRest.lightness());

    const QColor lightFill(250, 250, 250);
    const QColor ink = t.readableForeground(lightFill);
    QVERIFY(ink.lightness() < 128);
    QVERIFY(ThemeColors::contrastRatio(ink, lightFill) >= ThemeColors::kReadableContrast);
    const QColor mid = t.bgAt(50);
    QVERIFY(t.readableForeground(mid).lightness() > 128);
    QVERIFY(ThemeColors::contrastOn(mid).lightness() > 128);
    const QColor clear(0, 0, 0, 0);
    QVERIFY(light.readableForeground(clear, {}, light.bgMain).lightness() < 128);
    QVERIFY(t.readableForeground(clear, {}, t.bgMain).lightness() > 128);
}

void AppSettingsTest::customThemeUsesFluent()
{
    AppSettings s = AppSettings::defaults();
    const ThemePalette pal = ThemeScheme::fluent(s.themeAppearance, s.themeSaturation,
                                                 s.themeSeeds().primary, s.themeSeeds().secondary);
    QCOMPARE(s.resolvedTheme().accent, pal.colors.accent);
    QCOMPARE(s.resolvedPalette().progress.alpha(), kProgressFillAlpha);
}

void AppSettingsTest::secondaryColorIsSixtyPercent()
{
    AppSettings s = AppSettings::defaults();
    QVERIFY(s.setColorKey(QStringLiteral("customSecondaryColor"), QColor(255, 0, 0), false));
    QCOMPARE(s.colorKey(QStringLiteral("customSecondaryColor")).alpha(), kProgressFillAlpha);
    QCOMPARE(s.resolvedPalette().progress.alpha(), kProgressFillAlpha);
}

void AppSettingsTest::loadCustomThemeKeys()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.json"));
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(R"({"customPrimaryColor":"#FF0000"})");
    f.close();

    AppSettings s;
    QVERIFY(s.loadFromFile(path));
    QCOMPARE(s.themeSeeds().primary, QColor(QStringLiteral("#FF0000")));
}

void AppSettingsTest::saveRoundTripCustomFlag()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.json"));

    AppSettings s = AppSettings::defaults();
    QVERIFY(s.saveToFile(path));

    AppSettings loaded;
    QVERIFY(loaded.loadFromFile(path));
    QCOMPARE(loaded.resolvedTheme().accent, s.resolvedTheme().accent);
}

void AppSettingsTest::saturationScalesCustomAccent()
{
    AppSettings s = AppSettings::defaults();
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
    const int darkSat = s.resolvedTheme().bgMain.hsvSaturation();
    s.setThemeAppearance(ThemeAppearance::DarkTinted);
    QVERIFY(s.resolvedTheme().bgMain.hsvSaturation() > darkSat + 8);
    QVERIFY(s.resolvedTheme().bgMain.lightness() < 80);

    s.setThemeAppearance(ThemeAppearance::Light);
    const int lightSat = s.resolvedTheme().bgMain.hsvSaturation();
    s.setThemeAppearance(ThemeAppearance::LightTinted);
    QVERIFY(s.resolvedTheme().bgMain.hsvSaturation() > lightSat + 8);
    QVERIFY(s.resolvedTheme().bgMain.lightness() > 180);
}

void AppSettingsTest::appearanceSwitchesCustomNeutrals()
{
    AppSettings s = AppSettings::defaults();
    QVERIFY(s.resolvedTheme().bgMain.lightness() < 80);
    s.setThemeAppearance(ThemeAppearance::Light);
    QVERIFY(s.resolvedTheme().bgMain.lightness() > 180);
    QVERIFY(s.resolvedTheme().text.lightness() < 80);
}

void AppSettingsTest::backgroundShadeAndTintFamily()
{
    AppSettings s = AppSettings::defaults();
    s.setThemeAppearance(ThemeAppearance::Dark);
    s.setThemeBrightness(0);
    const int dark0 = s.resolvedTheme().bgMain.lightness();
    s.setThemeBrightness(4);
    const int dark4 = s.resolvedTheme().bgMain.lightness();
    QVERIFY(dark4 > dark0 + 10);

    s.setThemeAppearance(ThemeAppearance::Light);
    s.setThemeBrightness(0);
    const int light0 = s.resolvedTheme().bgMain.lightness();
    s.setThemeBrightness(4);
    const int light4 = s.resolvedTheme().bgMain.lightness();
    QVERIFY(light0 > light4 + 10);

    s.setThemeAppearance(ThemeAppearance::Dark);
    s.setThemeBrightness(kThemeBrightnessDefault);
    s.setThemeTintFamily(ThemeTintFamily::Complementary);
    QVERIFY(s.themeTintFamily == ThemeTintFamily::Complementary);
    QCOMPARE(s.themeAppearance, ThemeAppearance::DarkTinted);
    QVERIFY(s.resolvedTheme().bgMain.hsvSaturation() > 8);
    s.setThemeDark(false);
    QCOMPARE(s.themeTintFamily, ThemeTintFamily::Complementary);
    QCOMPARE(s.themeAppearance, ThemeAppearance::LightTinted);
    QVERIFY(s.resolvedTheme().bgMain.lightness() > 180);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.json"));
    s.setThemeBrightness(4);
    s.setThemeTintFamily(ThemeTintFamily::Analogous1);
    QVERIFY(s.saveToFile(path));
    AppSettings loaded;
    QVERIFY(loaded.loadFromFile(path));
    QCOMPARE(loaded.themeBrightness, 4);
    QCOMPARE(loaded.themeTintFamily, ThemeTintFamily::Analogous1);
}

void AppSettingsTest::speechSettingsRoundTrip()
{
    AppSettings s = AppSettings::defaults();
    s.speechModel = QStringLiteral("eleven_v3");
    s.speechSpeed = 1.4;
    s.speechPitch = 1.2;
    s.speechVolume = 2.5;
    s.elevenVoiceId = QStringLiteral("abc123");
    s.speechLangFilter = QStringLiteral("en");
    s.elevenFavoriteVoiceIds = {QStringLiteral("abc"), QString()};
    s.savedSpeechTags = {AppSettings::SavedSpeechTag{QStringLiteral("[laugh]"), {}, {}},
                         AppSettings::SavedSpeechTag{QStringLiteral("  "), {}, {}},
                         AppSettings::SavedSpeechTag{QStringLiteral("cry"), {}, {}}};
    AppSettings::SavedSpeechVoice preset;
    preset.id = QStringLiteral("v1");
    preset.name = QStringLiteral("Rachel laugh");
    preset.model = QStringLiteral("eleven_v3");
    preset.voiceId = QStringLiteral("abc123");
    preset.speed = 1.2;
    preset.volume = 2.0;
    s.savedSpeechVoices = {preset};
    s.clamp();
    QCOMPARE(s.speechSpeed, 1.4);
    QCOMPARE(s.speechVolume, 2.5);
    QCOMPARE(s.elevenFavoriteVoiceIds.size(), 1);
    QCOMPARE(s.savedSpeechTags.size(), 2);
    QCOMPARE(s.savedSpeechTags.front().name, QStringLiteral("laugh"));
    QCOMPARE(s.savedSpeechVoices.size(), 1);
    QCOMPARE(AppSettings::normalizeSpeechTag(QStringLiteral("[ loud ]")), QStringLiteral("loud"));
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.json"));
    QVERIFY(s.saveToFile(path));
    AppSettings b;
    QVERIFY(b.loadFromFile(path));
    QCOMPARE(b.speechModel, QStringLiteral("eleven_v3"));
    QCOMPARE(b.speechSpeed, 1.4);
    QCOMPARE(b.speechPitch, 1.2);
    QCOMPARE(b.speechVolume, 2.5);
    QCOMPARE(b.elevenVoiceId, QStringLiteral("abc123"));
    QCOMPARE(b.speechLangFilter, QStringLiteral("en"));
    QCOMPARE(b.elevenFavoriteVoiceIds, s.elevenFavoriteVoiceIds);
    QCOMPARE(b.savedSpeechTags, s.savedSpeechTags);
    QCOMPARE(b.savedSpeechVoices.size(), 1);
    QCOMPARE(b.savedSpeechVoices.front().id, QStringLiteral("v1"));
    QCOMPARE(b.savedSpeechVoices.front().name, QStringLiteral("Rachel laugh"));
    QCOMPARE(b.savedSpeechVoices.front().voiceId, QStringLiteral("abc123"));
    QCOMPARE(b.savedSpeechVoices.front().volume, 2.0);
}

void AppSettingsTest::headPoseMapsRoundTrip()
{
    AppSettings s = AppSettings::defaults();
    s.headPoseEnabled = true;
    s.headPoseOriginSet = true;
    s.headPoseOrigin.yaw = 4.5;
    s.headPoseOrigin.pitch = -1.0;
    s.headPoseOrigin.rotationValid = true;
    HeadPoseMap m = AppSettings::makeDefaultHeadPoseMap();
    m.source = HeadPoseAxis::Pitch;
    m.dest = HeadPoseDest::ScrollV;
    m.points = {{-20.0, -8.0}, {0.0, 0.0}, {20.0, 8.0}};
    s.headPoseMaps.push_back(m);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.json"));
    QString err;
    QVERIFY2(s.saveToFile(path, &err), qPrintable(err));
    AppSettings b;
    QVERIFY2(b.loadFromFile(path, &err), qPrintable(err));
    QCOMPARE(b.headPoseEnabled, true);
    QCOMPARE(b.headPoseOriginSet, true);
    QCOMPARE(b.headPoseOrigin.yaw, 4.5);
    QCOMPARE(b.headPoseMaps.size(), 1);
    QCOMPARE(b.headPoseMaps.front().source, HeadPoseAxis::Pitch);
    QCOMPARE(b.headPoseMaps.front().dest, HeadPoseDest::ScrollV);
    QCOMPARE(b.headPoseMaps.front().points.size(), 3);
}

void AppSettingsTest::hoverBorderFollowsProgressAndRoundTrips()
{
    AppSettings s = AppSettings::defaults();
    QVERIFY(!s.hoverCustom);
    QCOMPARE(s.resolvedHoverBorder(), s.colorKey(QStringLiteral("progressColor")));
    QVERIFY(s.setColorKey(QStringLiteral("hoverColor"), QColor(0, 255, 0), false));
    QCOMPARE(s.resolvedHoverBorder(), s.colorKey(QStringLiteral("progressColor")));
    s.hoverCustom = true;
    s.hoverBorderWeight = 4;
    QCOMPARE(s.resolvedHoverBorder(), s.colorKey(QStringLiteral("hoverColor")));

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.json"));
    QVERIFY(s.saveToFile(path));
    AppSettings loaded;
    QVERIFY(loaded.loadFromFile(path));
    QVERIFY(loaded.hoverCustom);
    QCOMPARE(loaded.hoverBorderWeight, 4);
    QCOMPARE(loaded.hoverColor, s.hoverColor);

    const QString legacy = dir.filePath(QStringLiteral("legacy.json"));
    QFile f(legacy);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(R"({"progressBorderColor":"#80AABBCC","hoverUseProgressColor":false,"flashUseForeground":false})");
    f.close();
    AppSettings migrated;
    QVERIFY(migrated.loadFromFile(legacy));
    QCOMPARE(migrated.hoverColor, QStringLiteral("#80AABBCC"));
    QVERIFY(migrated.hoverCustom);
    QVERIFY(migrated.flashCustom);
}

void AppSettingsTest::showSplashRoundTrip()
{
    AppSettings s = AppSettings::defaults();
    QVERIFY(s.showSplash);
    s.showSplash = false;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.json"));
    QVERIFY(s.saveToFile(path));
    AppSettings loaded;
    QVERIFY(loaded.loadFromFile(path));
    QVERIFY(!loaded.showSplash);

    const QString legacy = dir.filePath(QStringLiteral("legacy-splash.json"));
    QFile f(legacy);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(R"({"startDocked":true})");
    f.close();
    AppSettings migrated = AppSettings::defaults();
    QVERIFY(migrated.loadFromFile(legacy));
    QVERIFY(migrated.showSplash);
    QVERIFY(migrated.startDocked);
}

QObject* createAppSettingsTest()
{
    return new AppSettingsTest;
}

#include "AppSettingsTest.moc"
