#pragma once

#include "layout/PageTypes.h"

#include <QVector>

namespace gazer {
namespace PageResolve {

/// Cell / nested SubGrid: leaf ← … ← SubGrid ← Grid ← Page ← System (per field).
[[nodiscard]] PageChrome style(const PageDocument& page, const PageChrome& system,
                               const QVector<const PageGrid*>& gridChain, const QString& leafStyleId,
                               const PageChrome& leafStyle);

[[nodiscard]] PageDwell dwell(const PageDocument& page, const PageDwell& system,
                              const QVector<const PageGrid*>& gridChain, const QString& leafDwellId,
                              const PageDwell& leafDwell);

/// Zone: leaf ← Page ← System.
[[nodiscard]] PageChrome zoneStyle(const PageDocument& page, const PageChrome& system,
                                   const PageZone& zone);

[[nodiscard]] PageDwell zoneDwell(const PageDocument& page, const PageDwell& system,
                                  const PageZone& zone);

} // namespace PageResolve
} // namespace gazer
