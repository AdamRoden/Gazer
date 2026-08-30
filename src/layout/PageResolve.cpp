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

PageChrome style(const PageDocument& page, const QString& styleId, const PageChrome& inlineStyle)
{
    return page.style.withOverrides(namedOr(page, styleId, inlineStyle));
}

PageChrome gridStyle(const PageDocument& page, const QString& styleId, const PageChrome& inlineStyle)
{
    return style(page, styleId, inlineStyle).withoutItemPaint();
}

PageDwell dwell(const PageDocument& page, const QString& dwellId, const PageDwell& inlineDwell)
{
    return page.dwell.withOverrides(namedOr(page, dwellId, inlineDwell));
}

PageChrome zoneStyle(const PageDocument& page, const PageZone& zone)
{
    return style(page, zone.styleId, zone.style);
}

PageDwell zoneDwell(const PageDocument& page, const PageZone& zone)
{
    return dwell(page, zone.dwellId, zone.dwell);
}

} // namespace PageResolve
} // namespace gazer
