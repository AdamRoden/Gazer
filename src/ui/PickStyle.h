#pragma once

#include "ui/ProgressVisuals.h"

#include <QPainter>
#include <QPointF>
#include <QString>

namespace gazer {
namespace PickStyle {

/// Combinable glyphs for mouse-move / mag-pick targeting.
enum Flag : int {
    None = 0,
    Cursor = 1 << 0,
    Dot = 1 << 1,
    Crosshair = 1 << 2,
    GazeIndicator = 1 << 4,
};

constexpr int kMagPickMask = Cursor | Dot | Crosshair | GazeIndicator;
constexpr int kMousePickMask = Cursor | Dot | Crosshair;
constexpr int kDefaultMagPick = Cursor;
constexpr int kDefaultMousePick = Cursor;

[[nodiscard]] int sanitizeMag(int flags);
[[nodiscard]] int sanitizeMouse(int flags);

[[nodiscard]] bool has(int flags, Flag f);

void paint(QPainter& p, const QPointF& center, int flags, double progress,
           const ProgressVisuals& visuals, bool flashing = false);

[[nodiscard]] QString label(int flags);

} // namespace PickStyle
} // namespace gazer
