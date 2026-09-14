#include "ui/ThemeScheme.h"

#include <QtGlobal>

#include <cmath>

namespace gazer {
namespace ThemeScheme {
namespace {

const QColor kDefaultAccent(0x1E, 0x97, 0xF3);
const QColor kDangerSeed(0xE5, 0x39, 0x35);

QColor hsv(int h, int s, int v, int a = 255)
{
    const QColor c = QColor::fromHsv((h % 360 + 360) % 360, qBound(0, s, 255), qBound(0, v, 255),
                                     qBound(0, a, 255));
    return QColor(c.red(), c.green(), c.blue(), c.alpha());
}

void hsvParts(const QColor& c, int* h, int* s, int* v, int* a)
{
    int alpha = 255;
    c.getHsv(h, s, v, &alpha);
    if (*h < 0) {
        *h = 0;
    }
    if (a) {
        *a = alpha;
    }
}

int hsvValue(const QColor& c)
{
    int h = 0, s = 0, v = 0;
    hsvParts(c, &h, &s, &v, nullptr);
    return v;
}

QColor keepValue(const QColor& color, const QColor& valueSource)
{
    int h = 0, s = 0, v = 0, a = 255;
    hsvParts(color, &h, &s, &v, &a);
    return hsv(h, s, hsvValue(valueSource), a);
}

QColor appearanceTone(const QColor& brand, ThemeAppearance appearance)
{
    int h = 0, s = 0, v = 0, a = 255;
    hsvParts(brand, &h, &s, &v, &a);
    if (themeAppearanceIsDark(appearance)) {
        v = qBound(170, v + 36, 255);
    } else {
        v = qBound(70, v - 28, 160);
    }
    return hsv(h, s, v, a);
}

QColor gray(int v)
{
    const int n = qBound(0, v, 255);
    return QColor(n, n, n);
}

struct NeutralTone {
    int bg = 20;
    int border = 50;
};

NeutralTone appearanceNeutrals(bool dark, int brightness)
{
    const int b = qBound(kThemeBrightnessMin, brightness, kThemeBrightnessMax);
    static const NeutralTone kDark[kThemeBrightnessLevels] = {
        {4, 18}, {7, 22}, {10, 28}, {14, 40}, {20, 50},
    };
    static const NeutralTone kLight[kThemeBrightnessLevels] = {
        {243, 208}, {232, 196}, {216, 180}, {196, 162}, {172, 142},
    };
    return dark ? kDark[b] : kLight[b];
}

} // namespace

QColor scaleSaturation(const QColor& c, int saturationPercent)
{
    if (!c.isValid()) {
        return c;
    }
    const int sat = qBound(kThemeSaturationMin, saturationPercent, kThemeSaturationMax);
    if (sat == kThemeSaturationDefault) {
        return c;
    }
    int h = 0, s = 0, v = 0, a = 255;
    hsvParts(c, &h, &s, &v, &a);
    double scale = 1.0;
    if (sat >= kThemeSaturationDefault) {
        scale = 1.0
                + (sat - kThemeSaturationDefault)
                      / double(kThemeSaturationMax - kThemeSaturationDefault) * 0.55;
    } else {
        scale = 0.18 + sat / double(kThemeSaturationDefault) * 0.82;
    }
    const int ns = qBound(0, int(std::lround(s * scale)), 255);
    return hsv(h, ns, v, a);
}

ThemePalette fluent(ThemeAppearance appearance, int saturation, const QColor& primaryIn,
                    const QColor& secondaryIn, int brightness, const QColor& surfaceTintIn)
{
    const bool dark = themeAppearanceIsDark(appearance);
    const QColor primary =
        scaleSaturation(primaryIn.isValid() ? primaryIn : kDefaultAccent, saturation);
    const bool tinted = surfaceTintIn.isValid();
    const QColor mixHue = tinted ? scaleSaturation(surfaceTintIn, saturation) : primary;
    const double t = qBound(0, saturation, 100) / 100.0;
    double bgTint = 0.0;
    if (tinted) {
        const double tintVis = 1.0 + 0.18 * (brightness - kThemeBrightnessDefault);
        bgTint = ((dark ? 0.14 : 0.16) + 0.10 * t) * 0.5 * tintVis;
    }

    const NeutralTone tone = appearanceNeutrals(dark, brightness);
    const QColor nBg = gray(tone.bg);
    const QColor nBorder = gray(tone.border);
    const QColor body = dark ? QColor(0xFF, 0xFF, 0xFF) : QColor(0x1A, 0x1A, 0x1A);

    ThemePalette out;
    ThemeColors& c = out.colors;
    c.bgMain = keepValue(ThemeColors::mix(nBg, mixHue, bgTint), nBg);
    c.text = body;
    c.textSecondary = dark ? QColor(0xC8, 0xC8, 0xC8) : QColor(0x5D, 0x5D, 0x5D);
    c.border = keepValue(ThemeColors::mix(nBorder, mixHue, bgTint), nBorder);
    c.accent = primary;
    c.accentHover = scaleSaturation(appearanceTone(primary, appearance), saturation);
    c.danger = scaleSaturation(appearanceTone(kDangerSeed, appearance), saturation);

    const QColor progress = withProgressFillAlpha(
        secondaryIn.isValid() ? scaleSaturation(secondaryIn, saturation) : primary);
    out.progress = progress;
    out.progressFill = progress;
    c.progress = progress;
    c.appearance = appearance;
    return out;
}

} // namespace ThemeScheme
} // namespace gazer
