#pragma once

#include "layout/PageHit.h"

#include <QPointF>
#include <optional>

namespace gazer {

/// Page side of master volume. `SystemVolume` owns the endpoint. This walks
/// boards: stamp `caption="volume"` sliders, and map gaze on them. Passive
/// sliders are included. `PageHit::at` skips those, so dwell never sees them.
namespace VolumeBoard {

void stamp(PageDocument& doc, int percent);

/// Front target under @p gaze, as a 0–100 level. nullopt when that target is
/// not a volume slider (including when a key sits in front of one).
[[nodiscard]] std::optional<int> percentAt(const QVector<PageTarget>& targets,
                                           const QVector<PageGridPaint>& grids,
                                           double drawerScale, const QPointF& gaze);

} // namespace VolumeBoard
} // namespace gazer
