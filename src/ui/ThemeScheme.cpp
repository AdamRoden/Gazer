#include "ui/ThemeScheme.h"

#include <QtGlobal>

#include <cmath>

namespace gazer {
namespace ThemeScheme {
namespace {

// Apple iOS 13 system colors (the 9 chromatic UIColors). Light / dark pairs
// from UIColor.systemRed … systemPink.
const ThemeBrandInfo kBrands[] = {
    {"red", "Red", QColor(0xFF, 0x3B, 0x30), QColor(0xFF, 0x45, 0x3A)},
    {"orange", "Orange", QColor(0xFF, 0x95, 0x00), QColor(0xFF, 0x9F, 0x0A)},
    {"yellow", "Yellow", QColor(0xFF, 0xCC, 0x00), QColor(0xFF, 0xD6, 0x0A)},
    {"green", "Green", QColor(0x34, 0xC7, 0x59), QColor(0x30, 0xD1, 0x58)},
    {"teal", "Teal", QColor(0x5A, 0xC8, 0xFA), QColor(0x64, 0xD2, 0xFF)},
    {"blue", "Blue", QColor(0x00, 0x7A, 0xFF), QColor(0x0A, 0x84, 0xFF)},
    {"indigo", "Indigo", QColor(0x58, 0x56, 0xD6), QColor(0x5E, 0x5C, 0xE6)},
    {"purple", "Purple", QColor(0xAF, 0x52, 0xDE), QColor(0xBF, 0x5A, 0xF2)},
    {"pink", "Pink", QColor(0xFF, 0x2D, 0x55), QColor(0xFF, 0x37, 0x5F)},
};

static_assert(sizeof(kBrands) / sizeof(kBrands[0]) == kThemeBrandCount);
static_assert(kThemeDefaultBrandIndex >= 0 && kThemeDefaultBrandIndex < kThemeBrandCount);

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
    int surf = 30;
    int hover = 46;
    int border = 50;
};

NeutralTone appearanceNeutrals(bool dark, int brightness)
{
    const int b = qBound(kThemeBrightnessMin, brightness, kThemeBrightnessMax);
    // Five shades, two steps darker than the previous Light/Dark ramps.
    static const NeutralTone kDark[kThemeBrightnessLevels] = {
        {4, 8, 16, 18}, {7, 12, 20, 22}, {10, 16, 26, 28},
        {14, 22, 36, 40}, {20, 30, 46, 50},
    };
    static const NeutralTone kLight[kThemeBrightnessLevels] = {
        {243, 251, 235, 208}, {232, 244, 222, 196}, {216, 230, 208, 180},
        {196, 212, 192, 162}, {172, 190, 174, 142},
    };
    return dark ? kDark[b] : kLight[b];
}

} // namespace

const ThemeBrandInfo* brands()
{
    return kBrands;
}

int brandCount()
{
    return kThemeBrandCount;
}

QColor brandCanonical(int index)
{
    const int i = qBound(0, index, kThemeBrandCount - 1);
    return kBrands[i].light;
}

QColor brandAccent(int index, ThemeAppearance appearance)
{
    const int i = qBound(0, index, kThemeBrandCount - 1);
    return kBrands[i].colorFor(appearance);
}

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
    const QColor fallbackPrimary = brandAccent(kThemeDefaultBrandIndex, appearance);
    const QColor primary =
        scaleSaturation(primaryIn.isValid() ? primaryIn : fallbackPrimary, saturation);
    const bool tinted = surfaceTintIn.isValid();
    const QColor mixHue = tinted ? scaleSaturation(surfaceTintIn, saturation) : primary;
    const double t = qBound(0, saturation, 100) / 100.0;
    // Untinted Light/Dark stay neutral gray. A tint family washes hue onto those
    // same brightnesses. Brightness 2 keeps the legacy wash amount.
    double bgTint = 0.0;
    double surfaceTintAmt = 0.0;
    double accentW = 0.05 + 0.03 * t;
    if (tinted) {
        const double tintVis = 1.0 + 0.18 * (brightness - kThemeBrightnessDefault);
        bgTint = ((dark ? 0.14 : 0.16) + 0.10 * t) * 0.5 * tintVis;
        surfaceTintAmt = ((dark ? 0.22 : 0.24) + 0.12 * t) * 0.5 * tintVis;
        accentW = 0.065 + 0.035 * t;
    }

    const NeutralTone tone = appearanceNeutrals(dark, brightness);
    const QColor nBg = gray(tone.bg);
    const QColor nSurf = gray(tone.surf);
    const QColor nHover = gray(tone.hover);
    const QColor nBorder = gray(tone.border);
    const QColor body = dark ? QColor(0xFF, 0xFF, 0xFF) : QColor(0x1A, 0x1A, 0x1A);

    ThemePalette out;
    ThemeColors& c = out.colors;
    c.bgMain = keepValue(ThemeColors::mix(nBg, mixHue, bgTint), nBg);
    c.bgSurface = keepValue(ThemeColors::mix(nSurf, mixHue, surfaceTintAmt), nSurf);
    c.bgSurfaceHover = keepValue(ThemeColors::mix(nHover, mixHue, surfaceTintAmt), nHover);
    c.cellBg = c.bgSurface;
    c.cellHover = c.bgSurfaceHover;
    c.text = body;
    c.textSecondary = dark ? QColor(0xC8, 0xC8, 0xC8) : QColor(0x5D, 0x5D, 0x5D);
    c.border = keepValue(ThemeColors::mix(nBorder, mixHue, bgTint), nBorder);
    c.accent = primary;
    c.accentHover = scaleSaturation(appearanceTone(primary, appearance), saturation);
    c.danger = brandAccent(0, appearance);

    c.cellActive =
        keepValue(ThemeColors::mix(c.bgSurface, primary, qBound(0.16, surfaceTintAmt + 0.16, 0.42)),
                  c.bgSurface);
    c.bgSurfaceActive =
        keepValue(ThemeColors::mix(c.bgSurface, primary, qBound(0.10, accentW * 2.0, 0.34)),
                  c.bgSurface);

    const QColor progress = withProgressFillAlpha(
        secondaryIn.isValid() ? scaleSaturation(secondaryIn, saturation) : primary);
    out.progress = progress;
    out.progressFill = progress;
    c.progress = progress;
    c.appearance = appearance;
    return out;
}

ThemePalette resolve(ThemeAppearance appearance, int saturation, int primaryIndex,
                     int secondaryIndex, bool custom, const ThemeSeeds& customSeeds,
                     int brightness, const QColor& surfaceTint)
{
    if (custom) {
        const QColor accent =
            customSeeds.primary.isValid() ? customSeeds.primary
                                          : brandAccent(kThemeDefaultBrandIndex, appearance);
        const QColor progress =
            customSeeds.secondary.isValid() ? customSeeds.secondary : accent;
        return fluent(appearance, saturation, accent, progress, brightness, surfaceTint);
    }
    return fluent(appearance, saturation, brandAccent(primaryIndex, appearance),
                  brandAccent(secondaryIndex, appearance), brightness, surfaceTint);
}

} // namespace ThemeScheme
} // namespace gazer
