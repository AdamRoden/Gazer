#pragma once

#include "layout/PageTypes.h"

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
