#pragma once

#include "layout/PageTypes.h"

#include <QSet>

namespace gazer {
namespace PageNav {

/// Empty result becomes {1} (same as `normalizedLayers`).
inline void dropLayers(QVector<int>& shown, const QVector<int>& drop)
{
    QVector<int> next;
    next.reserve(shown.size());
    for (int n : shown) {
        if (!drop.contains(n)) {
            next.push_back(n);
        }
    }
    shown = next.isEmpty() ? defaultLayers() : next;
}

/// After shown layers change. `wasDrawer` / `nowDrawer` are top-level
/// `drawerMotion` grids on the master page whose layers intersect `showLayers`.
/// `otherRoot` is any other shown top-level master grid. `dismissing` is the
/// drawer animator already in Dismiss.
enum class DrawerAnim { Keep, Appear, Dismiss, Snap };

[[nodiscard]] inline DrawerAnim reconcileDrawer(bool wasDrawer, bool nowDrawer, bool otherRoot,
                                                bool dismissing)
{
    if (nowDrawer) {
        return (!wasDrawer || dismissing) ? DrawerAnim::Appear : DrawerAnim::Keep;
    }
    if (otherRoot) {
        return DrawerAnim::Snap;
    }
    if (wasDrawer && !dismissing) {
        return DrawerAnim::Dismiss;
    }
    return DrawerAnim::Keep;
}

struct Docs {
    PageDocument* root = nullptr;
    QVector<PageDocument*> attached;
};

/// Page flag, or any grid on the page. Attached pages close as a whole.
[[nodiscard]] inline bool optsIntoAutoClose(const PageDocument& doc)
{
    if (doc.autoClose) {
        return true;
    }
    for (const PageGrid& g : doc.grids) {
        if (g.autoClose) {
            return true;
        }
    }
    return false;
}

/// A shown root grid that idle-close will hide. The root page itself stays open.
[[nodiscard]] inline bool visibleAutoClose(const PageGrid& grid, const QVector<int>& shown)
{
    return grid.autoClose && layersVisible(grid.layers, shown);
}

[[nodiscard]] inline bool rootWillAutoClose(const PageDocument& root)
{
    for (const PageGrid& g : root.grids) {
        if (visibleAutoClose(g, root.showLayers)) {
            return true;
        }
    }
    return false;
}

/// Page ids painted at the idle-close warning opacity.
/// The root id is included only when a visible auto-close grid will hide.
/// An attached id is included only when that page will close.
[[nodiscard]] inline QSet<QString> autoCloseFadePages(const Docs& docs)
{
    QSet<QString> ids;
    if (docs.root && rootWillAutoClose(*docs.root) && !docs.root->id.isEmpty()) {
        ids.insert(docs.root->id);
    }
    for (const PageDocument* d : docs.attached) {
        if (d && !d->id.isEmpty() && optsIntoAutoClose(*d)) {
            ids.insert(d->id);
        }
    }
    return ids;
}

[[nodiscard]] inline PageDocument* page(const Docs& docs, const QString& pageId)
{
    if (pageId.isEmpty() || !docs.root) {
        return nullptr;
    }
    if (docs.root->id == pageId) {
        return docs.root;
    }
    for (PageDocument* d : docs.attached) {
        if (d && d->id == pageId) {
            return d;
        }
    }
    return nullptr;
}

} // namespace PageNav
} // namespace gazer
