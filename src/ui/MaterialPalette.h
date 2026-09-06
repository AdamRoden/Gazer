#pragma once

#include <QColor>
#include <QString>

namespace gazer {
namespace MaterialPalette {

/// Material Design 2 shade steps (50 … 900). Index 0 is 50, 9 is 900.
constexpr int kShadeCount = 10;
constexpr int kShades[kShadeCount] = {50, 100, 200, 300, 400, 500, 600, 700, 800, 900};

enum class Family {
    Primary,
    Complementary,
    Analogous1,
    Analogous2,
    Triadic1,
    Triadic2
};

constexpr int kFamilyCount = 6;

struct Palettes {
    QColor primary[kShadeCount];
    QColor complementary[kShadeCount];
    QColor analogous1[kShadeCount];
    QColor analogous2[kShadeCount];
    QColor triadic1[kShadeCount];
    QColor triadic2[kShadeCount];
};

[[nodiscard]] Palettes generate(const QColor& source);
[[nodiscard]] const QColor* familyColors(const Palettes& palettes, Family family);
[[nodiscard]] QColor shade(const Palettes& palettes, Family family, int index);
[[nodiscard]] const char* familyId(Family family);
[[nodiscard]] const char* familyLabel(Family family);
[[nodiscard]] bool parseFamily(const QString& id, Family* family);
[[nodiscard]] int shadeIndexForWeight(int weight);
[[nodiscard]] bool sameRgb(const QColor& a, const QColor& b);

} // namespace MaterialPalette
} // namespace gazer
