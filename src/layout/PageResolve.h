#pragma once

#include "layout/PageTypes.h"

namespace gazer {
namespace PageResolve {

/// Per-field: page ← named id ← inline.
/// Paint/hit apply thickness/radius defaults; session globals apply dwell fallbacks.
/// Grids never contribute style or dwell to cells, zones, or nested grids.
[[nodiscard]] PageChrome style(const PageDocument& page, const QString& styleId,
                               const PageChrome& inlineStyle);
/// Same inherit as `style`, then drops foreground / progress (grids never paint those).
[[nodiscard]] PageChrome gridStyle(const PageDocument& page, const QString& styleId,
                                   const PageChrome& inlineStyle);

[[nodiscard]] PageDwell dwell(const PageDocument& page, const QString& dwellId,
                              const PageDwell& inlineDwell);

[[nodiscard]] PageChrome zoneStyle(const PageDocument& page, const PageZone& zone);

[[nodiscard]] PageDwell zoneDwell(const PageDocument& page, const PageZone& zone);

} // namespace PageResolve
} // namespace gazer
