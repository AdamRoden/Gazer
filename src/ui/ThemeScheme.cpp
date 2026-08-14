#include "ui/ThemeScheme.h"

#include <QtGlobal>

#include <cmath>

namespace gazer {
namespace ThemeScheme {
namespace {

double srgbLin(int channel)
{
    const double s = channel / 255.0;
    return s <= 0.04045 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4);
}

double relativeY(const QColor& c)
{
    return 0.2126 * srgbLin(c.red()) + 0.7152 * srgbLin(c.green()) + 0.0722 * srgbLin(c.blue());
}

double toneOf(const QColor& c)
{
    const double y = relativeY(c);
    return y > 0.008856 ? 116.0 * std::cbrt(y) - 16.0 : 903.3 * y;
}

double wrapHue(double h)
{
    h = std::fmod(h, 360.0);
    return h < 0.0 ? h + 360.0 : h;
}

double hueLerp(double a, double b, double t)
{
    double d = b - a;
    if (d > 180.0) {
        d -= 360.0;
    } else if (d < -180.0) {
        d += 360.0;
    }
    return wrapHue(a + d * t);
}

double hueDelta(double a, double b)
{
    double d = b - a;
    if (d > 180.0) {
        d -= 360.0;
    } else if (d < -180.0) {
        d += 360.0;
    }
    return d;
}

void hsvOf(const QColor& c, int* h, int* s, int* v)
{
    int a = 255;
    c.getHsv(h, s, v, &a);
    if (*h < 0) {
        *h = 0;
    }
}

double hueOf(const QColor& c)
{
    int h = 0, s = 0, v = 0;
    hsvOf(c, &h, &s, &v);
    return double(h);
}

double chromaOf(const QColor& c)
{
    int h = 0, s = 0, v = 0;
    hsvOf(c, &h, &s, &v);
    return (s / 255.0) * (0.35 + 0.65 * (v / 255.0)) * 50.0;
}

QColor fromHct(double hue, double chroma, double tone)
{
    hue = wrapHue(hue);
    chroma = qBound(0.0, chroma, 80.0);
    tone = qBound(0.0, tone, 100.0);
    const int hi = int(std::lround(hue)) % 360;
    const int sat = qBound(0, int(std::lround(chroma * 2.35)), 255);
    int lo = 0;
    int hiV = 255;
    QColor best = QColor::fromHsv(hi, sat, qBound(0, int(std::lround(tone * 2.55)), 255));
    double bestErr = 1e9;
    for (int i = 0; i < 14; ++i) {
        const int mid = (lo + hiV) / 2;
        const QColor cand = QColor::fromHsv(hi, sat, mid);
        const double tn = toneOf(cand);
        const double err = std::abs(tn - tone);
        if (err < bestErr) {
            bestErr = err;
            best = cand;
        }
        if (tn < tone) {
            lo = mid + 1;
        } else {
            hiV = mid - 1;
        }
        if (lo > hiV) {
            break;
        }
    }
    return best;
}

struct ToneSpec {
    double bg = 6;
    double surface = 12;
    double surfaceHi = 17;
    double fg = 90;
    double fgMuted = 70;
    double accent = 80;
    double container = 30;
    double outline = 30;
    double chromaMul = 1.0;
    double chromaFloor = 24;
};

ToneSpec lerpSpec(const ToneSpec& a, const ToneSpec& b, double t)
{
    const auto mix = [t](double x, double y) { return x + (y - x) * t; };
    return {mix(a.bg, b.bg),
            mix(a.surface, b.surface),
            mix(a.surfaceHi, b.surfaceHi),
            mix(a.fg, b.fg),
            mix(a.fgMuted, b.fgMuted),
            mix(a.accent, b.accent),
            mix(a.container, b.container),
            mix(a.outline, b.outline),
            mix(a.chromaMul, b.chromaMul),
            mix(a.chromaFloor, b.chromaFloor)};
}

ToneSpec specFor(bool dark, int contrastPercent)
{
    const double t = qBound(0.0, snapContrastPercent(contrastPercent) / 100.0, 1.0);
    if (dark) {
        return lerpSpec({22, 26, 30, 52, 44, 48, 42, 18, 0.45, 8},
                        {0, 3, 6, 100, 94, 96, 16, 50, 1.38, 44}, t);
    }
    return lerpSpec({86, 82, 78, 46, 54, 56, 78, 76, 0.45, 8},
                    {100, 98, 95, 0, 10, 18, 92, 32, 1.38, 44}, t);
}

bool isDarkTone(double tone)
{
    return tone < 50.0;
}

ThemeSeeds remap(const ThemeSeeds& in, bool suggest)
{
    ThemeSeeds out = in;
    const double bgTone = toneOf(in.background.isValid() ? in.background : QColor(10, 10, 11));
    const bool dark = isDarkTone(bgTone);
    const ToneSpec sp = specFor(dark, in.contrastPercent);

    const QColor bg = in.background.isValid() ? in.background : QColor(10, 10, 11);
    const double bgH = hueOf(bg);
    const double bgC = chromaOf(bg);

    double srcH = bgC < 8.0 ? 231.0 : bgH;
    double srcC = qMax(bgC, sp.chromaFloor);

    if (suggest) {
        out.primary = fromHct(srcH, srcC * sp.chromaMul, sp.accent);
        out.secondary = fromHct(wrapHue(srcH + 48.0), srcC * 0.95 * sp.chromaMul, sp.accent);
        out.tertiary = fromHct(wrapHue(srcH + 120.0), srcC * 0.85 * sp.chromaMul, dark ? sp.container
                                                                                     : 70.0);
    } else {
        srcH = hueOf(in.primary.isValid() ? in.primary : fromHct(srcH, srcC, sp.accent));
        srcC = qMax(chromaOf(in.primary.isValid() ? in.primary : QColor()), sp.chromaFloor * 0.6);
        const double secH = hueOf(in.secondary.isValid() ? in.secondary : fromHct(srcH + 48, srcC, sp.accent));
        const double secC = qMax(chromaOf(in.secondary.isValid() ? in.secondary : QColor()),
                                 sp.chromaFloor * 0.6);
        const double terH = hueOf(in.tertiary.isValid() ? in.tertiary : fromHct(srcH + 120, srcC, 40));
        const double terC = qMax(chromaOf(in.tertiary.isValid() ? in.tertiary : QColor()),
                                 sp.chromaFloor * 0.5);
        out.primary = fromHct(srcH, srcC * sp.chromaMul, sp.accent);
        out.secondary = fromHct(secH, secC * sp.chromaMul, sp.accent);
        out.tertiary = fromHct(terH, terC * sp.chromaMul, dark ? sp.container : qBound(60.0, 100.0 - sp.accent, 92.0));
    }

    out.background = fromHct(bgH, qMin(qMax(bgC, 2.0), 18.0), sp.bg);
    out.contrastPercent = snapContrastPercent(in.contrastPercent);
    return out;
}

QColor fallbackPrimary(const ThemeSeeds& seeds, const ToneSpec& sp)
{
    const QColor bg = seeds.background.isValid() ? seeds.background : QColor(10, 10, 11);
    const double bgC = chromaOf(bg);
    const double bgH = bgC < 8.0 ? 231.0 : hueOf(bg);
    return seeds.primary.isValid() ? seeds.primary : fromHct(bgH, qMax(bgC, sp.chromaFloor), sp.accent);
}

QColor fallbackSecondary(const ThemeSeeds& seeds, const ToneSpec& sp, const QColor& primary)
{
    if (seeds.secondary.isValid()) {
        return seeds.secondary;
    }
    return fromHct(wrapHue(hueOf(primary) + 48.0), qMax(chromaOf(primary), sp.chromaFloor) * 0.95,
                   sp.accent);
}

} // namespace

ThemeSeeds suggestFromBackground(const QColor& background, int contrastPercent)
{
    ThemeSeeds s;
    s.background = background.isValid() ? background : QColor(10, 10, 11);
    s.contrastPercent = snapContrastPercent(contrastPercent);
    return remap(s, true);
}

ThemeSeeds suggestFromAccent(const QColor& accent, int contrastPercent,
                             const QColor& currentBackground)
{
    const QColor bgHint = currentBackground.isValid() ? currentBackground : QColor(10, 10, 11);
    const bool dark = isDarkTone(toneOf(bgHint));
    const ToneSpec sp = specFor(dark, contrastPercent);
    const QColor acc = accent.isValid() ? accent : fromHct(231.0, sp.chromaFloor, sp.accent);
    const double aH = hueOf(acc);
    const double aC = qMax(chromaOf(acc), sp.chromaFloor);

    ThemeSeeds s;
    s.contrastPercent = snapContrastPercent(contrastPercent);
    s.primary = fromHct(aH, aC * sp.chromaMul, sp.accent);
    s.secondary = fromHct(wrapHue(aH + 48.0), aC * 0.95 * sp.chromaMul, sp.accent);
    s.tertiary = fromHct(wrapHue(aH + 120.0), aC * 0.85 * sp.chromaMul, dark ? sp.container : 70.0);
    s.background = fromHct(aH, qMin(aC * 0.4, 16.0), sp.bg);
    return s;
}

ThemeSeeds fitContrast(const ThemeSeeds& seeds)
{
    return remap(seeds, false);
}

QColor suggestColor(const ThemeSeeds& seeds, ThemeColorRole role)
{
    const QColor bg = seeds.background.isValid() ? seeds.background : QColor(10, 10, 11);
    const bool dark = isDarkTone(toneOf(bg));
    const ToneSpec sp = specFor(dark, seeds.contrastPercent);
    const QColor primary = fallbackPrimary(seeds, sp);
    const QColor secondary = fallbackSecondary(seeds, sp, primary);
    const double bgH = hueOf(bg);
    const double bgC = chromaOf(bg);
    const double priH = hueOf(primary);
    const double priC = qMax(chromaOf(primary), sp.chromaFloor);
    const double secH = hueOf(secondary);
    const double secC = qMax(chromaOf(secondary), sp.chromaFloor);
    const double bgT = toneOf(bg);
    const double surfaceT = dark ? qMin(bgT + std::abs(sp.surface - sp.bg), 48.0)
                                 : qMax(bgT - std::abs(sp.bg - sp.surface), 62.0);

    switch (role) {
    case ThemeColorRole::Background:
        return fromHct(priH, qMin(priC * 0.4, 16.0), sp.bg);
    case ThemeColorRole::Surface:
        return fromHct(bgH, bgC * 0.55, surfaceT);
    case ThemeColorRole::Primary:
        return fromHct(wrapHue(secH - 48.0), qMax(secC, qMax(bgC, sp.chromaFloor)) * sp.chromaMul,
                       sp.accent);
    case ThemeColorRole::Secondary:
        return fromHct(wrapHue(priH + 48.0), qMax(priC, bgC) * 0.95 * sp.chromaMul, sp.accent);
    case ThemeColorRole::Tertiary: {
        double terH = wrapHue(hueLerp(priH, secH, 0.5) + 120.0);
        if (bgC >= 8.0 && std::abs(hueDelta(terH, bgH)) < 28.0) {
            terH = wrapHue(terH + (hueDelta(terH, bgH) >= 0.0 ? 40.0 : -40.0));
        }
        const double terC =
            (priC * 0.45 + secC * 0.40 + qMax(bgC, 4.0) * 0.15) * 0.85 * sp.chromaMul;
        return fromHct(terH, qMax(terC, sp.chromaFloor * 0.5), dark ? sp.container : 70.0);
    }
    case ThemeColorRole::Foreground:
        return fromHct(bgH, qMin(bgC, 8.0), sp.fg);
    case ThemeColorRole::Danger:
        return fromHct(25.0, 68.0 * sp.chromaMul, sp.accent);
    }
    return primary;
}

ThemeSliders inferSliders(const QColor& background, const QColor& primary)
{
    ThemeSliders out;
    const QColor bg = background.isValid() ? background : QColor(10, 10, 11);
    const QColor pri = primary.isValid() ? primary : QColor(138, 180, 248);
    const bool dark = isDarkTone(toneOf(bg));
    const double gap = std::abs(toneOf(pri) - toneOf(bg));
    int best = kThemeContrastMediumPct;
    double bestErr = 1e9;
    for (int pct : {kThemeContrastLowPct, kThemeContrastMediumPct, kThemeContrastHighPct}) {
        const ToneSpec sp = specFor(dark, pct);
        const double err = std::abs((sp.accent - sp.bg) - gap);
        if (err < bestErr) {
            bestErr = err;
            best = pct;
        }
    }
    out.contrast = best;
    const ToneSpec sp = specFor(dark, best);
    const QColor specBg = fromHct(hueOf(bg), qMin(qMax(chromaOf(bg), 2.0), 18.0), sp.bg);
    const QColor specPri = fromHct(hueOf(pri), qMax(chromaOf(pri), sp.chromaFloor), sp.accent);
    int h = 0, s = 0, bgV = 0, specBgV = 0, priV = 0, specPriV = 0;
    bg.getHsv(&h, &s, &bgV);
    specBg.getHsv(&h, &s, &specBgV);
    pri.getHsv(&h, &s, &priV);
    specPri.getHsv(&h, &s, &specPriV);
    const double dV = ((bgV - specBgV) + (priV - specPriV)) * 0.5;
    out.brightness = qBound(0, 4 + int(std::lround(dV / 18.0)), 8);
    return out;
}

ThemePalette build(const ThemeSeeds& seeds, bool fitTones)
{
    const ThemeSeeds fitted = fitTones ? fitContrast(seeds) : seeds;
    const QColor bg = fitted.background.isValid() ? fitted.background : QColor(10, 10, 11);
    const double bgH = hueOf(bg);
    const double bgC = chromaOf(bg);
    const bool dark = isDarkTone(toneOf(bg));
    const ToneSpec sp = specFor(dark, fitted.contrastPercent);

    const QColor primary = fallbackPrimary(fitted, sp);
    const QColor secondary = fallbackSecondary(fitted, sp, primary);
    const QColor tertiary = fitted.tertiary.isValid()
                                ? fitted.tertiary
                                : fromHct(wrapHue(hueOf(primary) + 120.0),
                                          chromaOf(primary) * 0.85, dark ? sp.container : 70.0);

    const double priH = hueOf(primary);
    const double priC = qMax(chromaOf(primary), sp.chromaFloor * 0.5);
    const double secH = hueOf(secondary);
    const double secC = qMax(chromaOf(secondary), sp.chromaFloor * 0.5);
    const double terH = hueOf(tertiary);
    const double terC = qMax(chromaOf(tertiary), sp.chromaFloor * 0.4);

    ThemePalette out;
    ThemeColors& c = out.colors;
    const double bgT = toneOf(bg);
    const double surfaceT = dark ? qMin(bgT + std::abs(sp.surface - sp.bg), 48.0)
                                 : qMax(bgT - std::abs(sp.bg - sp.surface), 62.0);
    const double surfaceHiT = dark ? qMin(bgT + std::abs(sp.surfaceHi - sp.bg), 54.0)
                                   : qMax(bgT - std::abs(sp.bg - sp.surfaceHi), 56.0);
    c.bgMain = bg;
    c.bgSurface = fromHct(bgH, bgC * 0.55, surfaceT);
    c.bgSurfaceHover = fromHct(bgH, bgC * 0.45, surfaceHiT);
    c.bgSurfaceActive = fitTones ? fromHct(terH, terC * 0.75, sp.container) : tertiary;
    c.cellBg = c.bgSurface;
    c.cellHover = fromHct(terH, terC * 0.45, sp.surfaceHi);
    c.cellActive = fitTones ? fromHct(terH, terC,
                                      dark ? sp.container
                                           : qBound(70.0, 100.0 - sp.accent + 20.0, 94.0))
                            : tertiary;
    c.text = fromHct(bgH, qMin(bgC, 8.0), sp.fg);
    c.textSecondary = fromHct(bgH, qMin(bgC, 6.0), sp.fgMuted);
    c.border = fromHct(bgH, qMin(qMax(bgC, 6.0), 12.0), sp.outline);
    c.accent = fitTones ? fromHct(priH, priC, sp.accent) : primary;
    c.accentHover = fromHct(priH, priC * 1.08, dark ? qMin(sp.accent + 8.0, 96.0)
                                                    : qMax(sp.accent - 8.0, 8.0));
    c.danger = fromHct(25.0, 68.0 * sp.chromaMul, sp.accent);

    out.progress = fitTones ? fromHct(secH, secC, sp.accent) : secondary;
    out.progressBorder = out.progress;
    QColor fill = out.progress;
    fill.setAlpha(70);
    out.progressFill = fill;
    return out;
}

} // namespace ThemeScheme
} // namespace gazer
