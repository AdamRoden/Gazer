#pragma once

#include <QColor>
#include <QPoint>
#include <QString>
#include <QtGlobal>

namespace gazer {

/// Color-picker swatches: 18 families (red → neutral, left → right) × 11 shades
/// (950 at top → 50 at bottom). Index is family-major, shade 50…950.
constexpr int kPickerFamilyCount = 18;
constexpr int kPickerShadeCount = 11;
constexpr int kPickerPaletteCount = kPickerFamilyCount * kPickerShadeCount;
constexpr int kPickerShades[kPickerShadeCount] = {50,  100, 200, 300, 400, 500,
                                                  600, 700, 800, 900, 950};
/// Page tokens for those hexes: `red05` (Tailwind 50, lightest) … `red95` (950, darkest).
constexpr int kPickerToneWeights[kPickerShadeCount] = {5, 10, 20, 30, 40, 50, 60, 70, 80, 90, 95};
constexpr const char* kPickerFamilies[kPickerFamilyCount] = {
    "red",     "orange", "amber",  "yellow",  "lime",    "green", "emerald", "teal", "cyan",
    "sky",     "blue",   "indigo", "violet",  "purple",  "fuchsia", "pink",  "rose", "neutral",
};

/// sRGB hex, family-major, shade 50…950. Converted from Tailwind OKLCH.
constexpr const char* kPickerHex[kPickerPaletteCount] = {
    "#fef2f2", "#ffe2e2", "#ffc9c9", "#ffa2a2", "#ff6467", "#fb2c36",
    "#e7000b", "#c10007", "#9f0712", "#82181a", "#460809", // red
    "#fff7ed", "#ffedd4", "#ffd6a7", "#ffb86a", "#ff8904", "#ff6900",
    "#f54900", "#ca3500", "#9f2d00", "#7e2a0c", "#441306", // orange
    "#fffbeb", "#fef3c6", "#fee685", "#ffd230", "#ffb900", "#fe9a00",
    "#e17100", "#bb4d00", "#973c00", "#7b3306", "#461901", // amber
    "#fefce8", "#fef9c2", "#fff085", "#ffdf20", "#fdc700", "#f0b100",
    "#d08700", "#a65f00", "#894b00", "#733e0a", "#432004", // yellow
    "#f7fee7", "#ecfcca", "#d8f999", "#bbf451", "#9ae600", "#7ccf00",
    "#5ea500", "#497d00", "#3c6300", "#35530e", "#192e03", // lime
    "#f0fdf4", "#dcfce7", "#b9f8cf", "#7bf1a8", "#05df72", "#00c950",
    "#00a63e", "#008236", "#016630", "#0d542b", "#032e15", // green
    "#ecfdf5", "#d0fae5", "#a4f4cf", "#5ee9b5", "#00d492", "#00bc7d",
    "#009966", "#007a55", "#006045", "#004f3b", "#002c22", // emerald
    "#f0fdfa", "#cbfbf1", "#96f7e4", "#46ecd5", "#00d5be", "#00bba7",
    "#009689", "#00786f", "#005f5a", "#0b4f4a", "#022f2e", // teal
    "#ecfeff", "#cefafe", "#a2f4fd", "#53eafd", "#00d3f2", "#00b8db",
    "#0092b8", "#007595", "#005f78", "#104e64", "#053345", // cyan
    "#f0f9ff", "#dff2fe", "#b8e6fe", "#74d4ff", "#00bcff", "#00a6f4",
    "#0084d1", "#0069a8", "#00598a", "#024a70", "#052f4a", // sky
    "#eff6ff", "#dbeafe", "#bedbff", "#8ec5ff", "#51a2ff", "#2b7fff",
    "#155dfc", "#1447e6", "#193cb8", "#1c398e", "#162456", // blue
    "#eef2ff", "#e0e7ff", "#c6d2ff", "#a3b3ff", "#7c86ff", "#615fff",
    "#4f39f6", "#432dd7", "#372aac", "#312c85", "#1e1a4d", // indigo
    "#f5f3ff", "#ede9fe", "#ddd6ff", "#c4b4ff", "#a684ff", "#8e51ff",
    "#7f22fe", "#7008e7", "#5d0ec0", "#4d179a", "#2f0d68", // violet
    "#faf5ff", "#f3e8ff", "#e9d4ff", "#dab2ff", "#c27aff", "#ad46ff",
    "#9810fa", "#8200db", "#6e11b0", "#59168b", "#3c0366", // purple
    "#fdf4ff", "#fae8ff", "#f6cfff", "#f4a8ff", "#ed6aff", "#e12afb",
    "#c800de", "#a800b7", "#8a0194", "#721378", "#4b004f", // fuchsia
    "#fdf2f8", "#fce7f3", "#fccee8", "#fda5d5", "#fb64b6", "#f6339a",
    "#e60076", "#c6005c", "#a3004c", "#861043", "#510424", // pink
    "#fff1f2", "#ffe4e6", "#ffccd3", "#ffa1ad", "#ff637e", "#ff2056",
    "#ec003f", "#c70036", "#a50036", "#8b0836", "#4d0218", // rose
    "#fafafa", "#f5f5f5", "#e5e5e5", "#d4d4d4", "#a1a1a1", "#737373",
    "#525252", "#404040", "#262626", "#171717", "#0a0a0a", // neutral
};

static_assert(sizeof(kPickerHex) / sizeof(kPickerHex[0]) == kPickerPaletteCount);
static_assert(sizeof(kPickerFamilies) / sizeof(kPickerFamilies[0]) == kPickerFamilyCount);
static_assert(sizeof(kPickerToneWeights) / sizeof(kPickerToneWeights[0]) == kPickerShadeCount);

/// Column is family (red = 0). Row is shade with 950 at top (row 0).
[[nodiscard]] inline QPoint pickerPaletteRowCol(int index)
{
    if (index < 0 || index >= kPickerPaletteCount) {
        return QPoint(-1, -1);
    }
    const int family = index / kPickerShadeCount;
    const int shade = index % kPickerShadeCount;
    return QPoint(family, kPickerShadeCount - 1 - shade);
}

[[nodiscard]] inline QColor pickerPaletteColor(int index, int alpha = 255)
{
    if (index < 0 || index >= kPickerPaletteCount) {
        return {};
    }
    QColor c(QLatin1String(kPickerHex[index]));
    if (!c.isValid()) {
        return {};
    }
    c.setAlpha(qBound(0, alpha, 255));
    return c;
}

[[nodiscard]] inline int pickerFamilyIndex(const QString& family)
{
    const QString f = family.trimmed().toLower();
    for (int i = 0; i < kPickerFamilyCount; ++i) {
        if (f == QLatin1String(kPickerFamilies[i])) {
            return i;
        }
    }
    return -1;
}

[[nodiscard]] inline int pickerShadeIndexForWeight(int weight)
{
    for (int i = 0; i < kPickerShadeCount; ++i) {
        if (kPickerToneWeights[i] == weight) {
            return i;
        }
    }
    return -1;
}

[[nodiscard]] inline int pickerPaletteIndexAt(int family, int weight)
{
    const int shade = pickerShadeIndexForWeight(weight);
    if (family < 0 || family >= kPickerFamilyCount || shade < 0) {
        return -1;
    }
    return family * kPickerShadeCount + shade;
}

[[nodiscard]] inline QColor pickerPaletteColorAt(int family, int weight, int alpha = 255)
{
    return pickerPaletteColor(pickerPaletteIndexAt(family, weight), alpha);
}

[[nodiscard]] inline QString pickerPaletteToken(int index)
{
    if (index < 0 || index >= kPickerPaletteCount) {
        return {};
    }
    const int family = index / kPickerShadeCount;
    const int shade = index % kPickerShadeCount;
    const int w = kPickerToneWeights[shade];
    if (w == 5) {
        return QLatin1String(kPickerFamilies[family]) + QStringLiteral("05");
    }
    return QLatin1String(kPickerFamilies[family]) + QString::number(w);
}

} // namespace gazer
