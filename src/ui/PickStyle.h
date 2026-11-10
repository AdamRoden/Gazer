#pragma once

#include "ui/ProgressVisuals.h"

#include <QPainter>
#include <QPointF>
#include <QString>

namespace gazer {
namespace PickStyle {

/// Combinable glyphs for move-to targeting. The zoom region uses the same set.
enum Flag : int {
    None = 0,
    Cursor = 1 << 0,
    Dot = 1 << 1,
    Crosshair = 1 << 2,
};

constexpr int kMousePickMask = Cursor | Dot | Crosshair;
constexpr int kDefaultMousePick = Cursor;

[[nodiscard]] int sanitizeMouse(int flags);

[[nodiscard]] bool has(int flags, Flag f);

void paint(QPainter& p, const QPointF& center, int flags, double progress,
           const ProgressVisuals& visuals, bool flashing = false);

[[nodiscard]] QString label(int flags);

} // namespace PickStyle
} // namespace gazer
