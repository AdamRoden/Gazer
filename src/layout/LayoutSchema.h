#pragma once

#include "layout/LayoutTypes.h"

#include <QString>
#include <QStringList>

namespace gazer {
namespace LayoutSchema {

[[nodiscard]] QString actionTypeName(LayoutAction::Type t);
[[nodiscard]] LayoutAction::Type actionTypeFromName(const QString& s);
[[nodiscard]] QStringList actionTypeNames();

[[nodiscard]] QString windowAnchorName(LayoutWindowPlacement::Anchor a);
[[nodiscard]] LayoutWindowPlacement::Anchor windowAnchorFromName(const QString& s);
[[nodiscard]] QStringList windowAnchorNames();

[[nodiscard]] QString screenAnchorName(LayoutDwellRegion::ScreenAnchor a);
[[nodiscard]] LayoutDwellRegion::ScreenAnchor screenAnchorFromName(const QString& s);
[[nodiscard]] QStringList screenAnchorNames();

[[nodiscard]] QString boundsModeName(BoundsMode m);
[[nodiscard]] BoundsMode boundsModeFromName(const QString& s,
                                            BoundsMode fallback = BoundsMode::Desktop);

} // namespace LayoutSchema
} // namespace gazer
