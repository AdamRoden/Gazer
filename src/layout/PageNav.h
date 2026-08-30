#pragma once

#include "layout/PageTypes.h"

namespace gazer {
namespace PageNav {

[[nodiscard]] inline bool shownAfter(PageVerb verb, bool currently)
{
    if (verb == PageVerb::Close) {
        return false;
    }
    if (verb == PageVerb::Toggle) {
        return !currently;
    }
    return true;
}

/// After grid `show` flags change. `wasDrawer` / `nowDrawer` are top-level
/// `drawerMotion && show` on the master page. `otherRoot` is any other shown
/// top-level master grid. `dismissing` is the drawer animator already in Dismiss.
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

[[nodiscard]] inline QString kindLabel(PageTargetKind kind)
{
    switch (kind) {
    case PageTargetKind::Grid:
        return QStringLiteral("Grid");
    case PageTargetKind::Zone:
        return QStringLiteral("Zone");
    case PageTargetKind::Cell:
        return QStringLiteral("Cell");
    case PageTargetKind::Page:
        return QStringLiteral("Page");
    }
    return QStringLiteral("Target");
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

template<typename T>
[[nodiscard]] T* locate(const Docs& docs, const QString& preferPageId, const QString& id,
                        T* (PageDocument::*find)(const QString&))
{
    if (id.isEmpty() || !docs.root) {
        return nullptr;
    }
    if (PageDocument* d = page(docs, preferPageId)) {
        if (T* t = (d->*find)(id)) {
            return t;
        }
    }
    if (T* t = (docs.root->*find)(id)) {
        return t;
    }
    for (PageDocument* d : docs.attached) {
        if (d) {
            if (T* t = (d->*find)(id)) {
                return t;
            }
        }
    }
    return nullptr;
}

template<typename Fn>
void forEachGridFlag(QVector<PageGrid>& nodes, Fn&& fn)
{
    for (PageGrid& g : nodes) {
        if (!g.id.isEmpty()) {
            fn(g.show, g.id);
        }
        forEachGridFlag(g.subGrids, fn);
    }
}

template<typename Fn>
void forEachCellFlag(QVector<PageGrid>& nodes, Fn&& fn)
{
    for (PageGrid& g : nodes) {
        for (PageCell& c : g.cells) {
            if (!c.id.isEmpty()) {
                fn(c.show, c.id);
            }
        }
        forEachCellFlag(g.subGrids, fn);
    }
}

template<typename Fn>
void forEachFlag(PageDocument& doc, PageTargetKind kind, Fn&& fn)
{
    switch (kind) {
    case PageTargetKind::Grid:
        forEachGridFlag(doc.grids, fn);
        break;
    case PageTargetKind::Zone:
        for (PageZone& z : doc.zones) {
            if (!z.id.isEmpty()) {
                fn(z.show, z.id);
            }
        }
        break;
    case PageTargetKind::Cell:
        forEachCellFlag(doc.grids, fn);
        break;
    case PageTargetKind::Page:
        break;
    }
}

template<typename Fn>
void forEachDoc(const Docs& docs, Fn&& fn)
{
    if (docs.root) {
        fn(*docs.root);
    }
    for (PageDocument* d : docs.attached) {
        if (d) {
            fn(*d);
        }
    }
}

inline void applyScope(const Docs& docs, PageTargetKind kind, PageVerb verb, const QString& skipId,
                       const QString& skipPage)
{
    forEachDoc(docs, [&](PageDocument& doc) {
        forEachFlag(doc, kind, [&](bool& show, const QString& id) {
            if (!skipId.isEmpty() && id == skipId
                && (skipPage.isEmpty() || doc.id == skipPage)) {
                return;
            }
            show = shownAfter(verb, show);
        });
    });
}

} // namespace PageNav
} // namespace gazer
