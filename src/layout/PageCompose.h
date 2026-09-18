#pragma once

#include "layout/PageTypes.h"

#include <QHash>
#include <QSet>
#include <QString>
#include <functional>

namespace gazer {
namespace PageCompose {

using LoadFn = std::function<bool(const QString& id, PageDocument& out, QString* error)>;

void forEachSrc(const PageDocument& doc, const std::function<void(const PageGrid&)>& fn);
/// Ids this page loads into a slot (`src` or HostPage target).
void collectSlotTargets(const PageDocument& doc, QSet<QString>& ids);

[[nodiscard]] const PageGrid* findSrcSlot(const PageDocument& doc);
[[nodiscard]] PageGrid* findSrcSlot(PageDocument& doc);
[[nodiscard]] QString firstSrc(const PageDocument& doc);

/// Fragment grid to inline. `srcGrid` empty → first top-level grid.
[[nodiscard]] const PageGrid* sourcedGrid(const PageDocument& fragment, const QString& srcGrid,
                                          QString* error = nullptr);

/// Load every transitive `src` page into @p out. Detects cycles.
[[nodiscard]] bool resolveAll(const PageDocument& root, const LoadFn& load,
                              QHash<QString, PageDocument>& out, QString* error = nullptr);

[[nodiscard]] QHash<QString, const PageDocument*> pointers(const QHash<QString, PageDocument>& docs);

} // namespace PageCompose
} // namespace gazer
