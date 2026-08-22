#include "layout/PageResolve.h"

namespace gazer {
namespace PageResolve {

namespace {

PageChrome namedOr(const PageDocument& page, const QString& id, const PageChrome& inlineStyle)
{
    PageChrome c;
    if (!id.isEmpty()) {
        const auto it = page.styles.constFind(id);
        if (it != page.styles.cend()) {
            c = it.value();
        }
    }
    return c.withOverrides(inlineStyle);
}

PageDwell namedOr(const PageDocument& page, const QString& id, const PageDwell& inlineDwell)
{
    PageDwell d;
    if (!id.isEmpty()) {
        const auto it = page.dwells.constFind(id);
        if (it != page.dwells.cend()) {
            d = it.value();
        }
    }
    return d.withOverrides(inlineDwell);
}

} // namespace

PageChrome style(const PageDocument& page, const PageChrome& system,
                 const QVector<const PageGrid*>& gridChain, const QString& leafStyleId,
                 const PageChrome& leafStyle)
{
    PageChrome c = system;
    c = c.withOverrides(page.style);
    for (const PageGrid* g : gridChain) {
        if (!g) {
            continue;
        }
        c = c.withOverrides(namedOr(page, g->styleId, g->style));
    }
    c = c.withOverrides(namedOr(page, leafStyleId, leafStyle));
    return c;
}

PageDwell dwell(const PageDocument& page, const PageDwell& system,
                const QVector<const PageGrid*>& gridChain, const QString& leafDwellId,
                const PageDwell& leafDwell)
{
    PageDwell d = system;
    d = d.withOverrides(page.dwell);
    for (const PageGrid* g : gridChain) {
        if (!g) {
            continue;
        }
        d = d.withOverrides(namedOr(page, g->dwellId, g->dwell));
    }
    d = d.withOverrides(namedOr(page, leafDwellId, leafDwell));
    return d;
}

PageChrome zoneStyle(const PageDocument& page, const PageChrome& system, const PageZone& zone)
{
    PageChrome c = system;
    c = c.withOverrides(page.style);
    c = c.withOverrides(namedOr(page, zone.styleId, zone.style));
    return c;
}

PageDwell zoneDwell(const PageDocument& page, const PageDwell& system, const PageZone& zone)
{
    PageDwell d = system;
    d = d.withOverrides(page.dwell);
    d = d.withOverrides(namedOr(page, zone.dwellId, zone.dwell));
    return d;
}

} // namespace PageResolve
} // namespace gazer
