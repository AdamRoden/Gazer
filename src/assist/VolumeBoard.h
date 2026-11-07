#pragma once

#include "assist/GazeTrack.h"
#include "layout/PageHit.h"

#include <QPointF>
#include <optional>

namespace gazer {

/// Page side of the Speak output volume. `SystemVolume` owns the endpoint. This stamps
/// `caption="volume"` sliders and maps gaze on them. The sliders stay passive,
/// so `PageHit` does not dwell them. Gaze must stay for the cell's `scanGrace`,
/// or `kTrackLockMs` when the cell omits it, before `GazeTrack` may move the level.
namespace VolumeBoard {

void stamp(PageDocument& doc, int percent);

struct Gaze {
    QString key;
    /// 0–1 through scan grace. 1 once the level may follow.
    double arm = 0;
    /// Scan grace has elapsed. The shell then drags with `GazeTrack`.
    bool ready = false;
    /// Gaze and handle travel along the bar, in screen pixels.
    double gazePx = 0;
    double track0 = 0;
    double track1 = 0;
};

/// Front volume slider under @p gaze. Empty when a key sits in front, or when
/// gaze is elsewhere. A miss leaves @p arm so the caller can release that scrub.
[[nodiscard]] std::optional<Gaze> at(const QVector<PageTarget>& targets,
                                     const QVector<PageGridPaint>& grids, double drawerScale,
                                     const QPointF& gaze, qint64 nowMs, GazeTrack::Lock* arm);

} // namespace VolumeBoard
} // namespace gazer
