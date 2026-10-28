#pragma once

#include "layout/PageHit.h"

#include <QPointF>
#include <optional>

namespace gazer {

/// Page side of master volume. `SystemVolume` owns the endpoint. This stamps
/// `caption="volume"` sliders and maps gaze on them. The sliders stay passive,
/// so `PageHit` does not dwell them. Gaze must stay for the cell's `scanGrace`
/// (or @p fallbackGraceMs) before the level is reported.
namespace VolumeBoard {

void stamp(PageDocument& doc, int percent);

/// Clock for one continuous look at a slider. The shell keeps it across samples.
struct Arm {
    QString key;
    qint64 sinceMs = -1;
};

/// Speak slider follow. The first call seeds the clock and returns @p current.
/// Later calls move at most @p perSec percent each second. A gap longer than
/// 200 ms counts as 200 ms, so a look that returns does not jump the level.
struct Slew {
    double credit = 0;
    qint64 atMs = -1;
};

inline constexpr double kSpeakSlewPercentPerSec = 20.0;

[[nodiscard]] int stepToward(int current, int target, qint64 nowMs, double perSec, Slew* slew);

struct Gaze {
    QString key;
    /// 0–1 through scan grace. 1 once the level may follow.
    double arm = 0;
    /// Set only after scan grace. Absent while the ring is still arming.
    std::optional<int> percent;
};

/// Front volume slider under @p gaze. Empty when a key sits in front, or when
/// gaze is elsewhere (that also clears @p arm).
[[nodiscard]] std::optional<Gaze> at(const QVector<PageTarget>& targets,
                                     const QVector<PageGridPaint>& grids, double drawerScale,
                                     const QPointF& gaze, qint64 nowMs, int fallbackGraceMs,
                                     Arm* arm);

} // namespace VolumeBoard
} // namespace gazer
