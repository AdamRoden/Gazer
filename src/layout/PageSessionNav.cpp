#include "layout/PageSession.h"

#include "layout/PageNav.h"

#include <QStringList>
#include <utility>

namespace gazer {

namespace {

QString sourcePage(const PageSession& s, const QString& sourcePageId)
{
    return sourcePageId.isEmpty() ? s.topPageId() : sourcePageId;
}

} // namespace

PageSession::PageBreadcrumb PageSession::captureBreadcrumb() const
{
    PageBreadcrumb b;
    b.root = m_root;
    b.attached.reserve(m_attached.size());
    for (const AttachedPage& a : m_attached) {
        b.attached.push_back(a.doc);
    }
    return b;
}

void PageSession::restoreBreadcrumb(PageBreadcrumb snap)
{
    QSet<QString> keep;
    for (const PageDocument& d : snap.attached) {
        if (!d.id.isEmpty()) {
            keep.insert(d.id);
        }
    }
    if (m_loopStopPage) {
        for (const AttachedPage& a : m_attached) {
            if (!keep.contains(a.doc.id)) {
                m_loopStopPage(a.doc.id);
            }
        }
    }
    if (!keep.contains(m_leaveGatePage)) {
        clearLeaveGate();
    }

    m_root = std::move(snap.root);
    m_attached.clear();
    for (PageDocument& doc : snap.attached) {
        AttachedPage ap;
        ap.doc = std::move(doc);
        m_attached.push_back(std::move(ap));
    }
    resetDrawerAnim();
    rebuild();
    raise();
    emit sessionChanged();
}

void PageSession::closePagesExcept(const QString& keepId)
{
    QStringList closeIds;
    for (const AttachedPage& a : m_attached) {
        if (!a.doc.id.isEmpty() && a.doc.id != keepId) {
            closeIds.push_back(a.doc.id);
        }
    }
    for (const QString& id : closeIds) {
        closePage(id);
    }
}

bool PageSession::goBack(QString* error)
{
    if (m_crumbs.isEmpty()) {
        if (error) {
            *error = QStringLiteral("No page breadcrumb to go back to");
        }
        return false;
    }
    restoreBreadcrumb(m_crumbs.takeLast());
    return true;
}

bool PageSession::applyPageAction(PageVerb verb, PageTargetKind kind, const QString& id,
                                  QString* error)
{
    PageAction a;
    a.type = PageActionType::Nav;
    a.verb = verb;
    a.targetKind = kind;
    a.targetScope = PageNavScope::Id;
    a.targetId = id;
    return applyNav(a, {}, {}, error);
}

bool PageSession::applyNav(const PageAction& action, const QString& sourcePageId,
                           const QString& sourceTargetId, QString* error)
{
    if (action.type == PageActionType::GoBack) {
        return goBack(error);
    }
    if (action.type != PageActionType::Nav) {
        if (error) {
            *error = QStringLiteral("Not a page navigation action");
        }
        return false;
    }
    if (action.targetKind == PageTargetKind::Page) {
        if (!hasRoot()) {
            if (error) {
                *error = QStringLiteral("No root page");
            }
            return false;
        }
        PageBreadcrumb snap;
        if (action.breadcrumb) {
            snap = captureBreadcrumb();
        }
        if (!applyNavMutation(action, sourcePageId, sourceTargetId, error)) {
            return false;
        }
        if (action.breadcrumb) {
            m_crumbs.push_back(std::move(snap));
        }
        return true;
    }
    QVector<PageAction> one;
    one.push_back(action);
    return applyNavs(one, sourcePageId, sourceTargetId, error);
}

bool PageSession::applyNavs(const QVector<PageAction>& actions, const QString& sourcePageId,
                            const QString& sourceTargetId, QString* error)
{
    if (actions.isEmpty()) {
        return true;
    }
    if (!hasRoot()) {
        if (error) {
            *error = QStringLiteral("No root page");
        }
        return false;
    }

    bool wantCrumb = false;
    for (const PageAction& a : actions) {
        if (a.type != PageActionType::Nav || a.targetKind == PageTargetKind::Page) {
            if (error) {
                *error = QStringLiteral("applyNavs is ShowGrid/HideGrid (and zone/cell) only");
            }
            return false;
        }
        wantCrumb = wantCrumb || a.breadcrumb;
    }

    PageBreadcrumb snap;
    if (wantCrumb) {
        snap = captureBreadcrumb();
    }
    const bool wasDrawer = drawerMotionShown();
    bool ok = true;
    for (const PageAction& a : actions) {
        if (!applyNavMutation(a, sourcePageId, sourceTargetId, error)) {
            ok = false;
            break;
        }
    }
    if (wantCrumb && ok) {
        m_crumbs.push_back(std::move(snap));
    }
    syncDrawerAnim(wasDrawer);
    emitShowChanged();
    return ok;
}

bool PageSession::applyNavMutation(const PageAction& action, const QString& sourcePageId,
                                   const QString& sourceTargetId, QString* error)
{
    switch (action.targetKind) {
    case PageTargetKind::Page:
        return applyNavPage(action.verb, action.targetScope, action.targetId, sourcePageId, error);
    case PageTargetKind::Grid:
    case PageTargetKind::Zone:
    case PageTargetKind::Cell:
        return applyShowNav(action.verb, action.targetKind, action.targetScope, action.targetId,
                            sourcePageId, sourceTargetId, error);
    }
    if (error) {
        *error = QStringLiteral("Unknown navigation target kind");
    }
    return false;
}

bool PageSession::applyNavPage(PageVerb verb, PageNavScope scope, const QString& id,
                               const QString& sourcePageId, QString* error)
{
    const QString self = sourcePage(*this, sourcePageId);

    if (scope == PageNavScope::All) {
        if (verb == PageVerb::Close || verb == PageVerb::Toggle) {
            closeAttached();
            return true;
        }
        if (error) {
            *error = QStringLiteral("Open Page -all is not valid");
        }
        return false;
    }
    if (scope == PageNavScope::Others) {
        if (verb == PageVerb::Close || verb == PageVerb::Toggle) {
            closePagesExcept(self);
            return true;
        }
        if (error) {
            *error = QStringLiteral("Open Page -!self is not valid");
        }
        return false;
    }

    const QString tid = (scope == PageNavScope::Self) ? self : id;
    if (tid.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Page target is empty");
        }
        return false;
    }
    if (tid == m_root.id) {
        if (verb == PageVerb::Close) {
            if (error) {
                *error = QStringLiteral("Cannot close the root page");
            }
            return false;
        }
        raise();
        return true;
    }
    const bool attached = hasPage(tid);
    if (verb == PageVerb::Close || (verb == PageVerb::Toggle && attached)) {
        if (attached) {
            closePage(tid);
            return true;
        }
        if (error) {
            *error = QStringLiteral("Page not open: %1").arg(tid);
        }
        return false;
    }
    if (verb == PageVerb::Open || verb == PageVerb::Toggle) {
        return openPage(tid, error);
    }
    if (error) {
        *error = QStringLiteral("No handler for Page %1").arg(tid);
    }
    return false;
}

const PageTarget* PageSession::sourceCell(const QString& sourcePageId,
                                          const QString& sourceTargetId) const
{
    if (sourceTargetId.isEmpty()) {
        return nullptr;
    }
    if (!sourcePageId.isEmpty()) {
        if (const PageTarget* t =
                findTarget(sourcePageId + QLatin1Char('/') + sourceTargetId)) {
            return t;
        }
    }
    if (const PageTarget* t = findTarget(sourceTargetId)) {
        return t;
    }
    for (const PageTarget& t : m_targets) {
        if (t.id == sourceTargetId
            && (sourcePageId.isEmpty() || t.pageId == sourcePageId)) {
            return &t;
        }
    }
    return nullptr;
}

PageNav::Docs PageSession::navDocs()
{
    PageNav::Docs d;
    d.root = &m_root;
    d.attached.reserve(m_attached.size());
    for (AttachedPage& a : m_attached) {
        d.attached.push_back(&a.doc);
    }
    return d;
}

bool PageSession::resolveShowSelf(PageTargetKind kind, const QString& sourcePageId,
                                  const QString& sourceTargetId, QString* itemId, QString* preferPage,
                                  QString* error)
{
    if (kind == PageTargetKind::Zone) {
        const PageNav::Docs docs = navDocs();
        if (!PageNav::locate(docs, sourcePageId, sourceTargetId, &PageDocument::findZone)
            && !PageNav::locate(docs, {}, sourceTargetId, &PageDocument::findZone)) {
            if (error) {
                *error = QStringLiteral("Zone -self requires a zone source");
            }
            return false;
        }
        *itemId = sourceTargetId;
        return true;
    }
    const PageTarget* src = sourceCell(sourcePageId, sourceTargetId);
    if (kind == PageTargetKind::Grid) {
        if (!src || src->gridId.isEmpty()) {
            if (error) {
                *error = QStringLiteral("Grid -self requires a grid source");
            }
            return false;
        }
        *itemId = src->gridId;
        *preferPage = src->pageId.isEmpty() ? m_root.id : src->pageId;
        return true;
    }
    if (!src || src->kind != PageTarget::Kind::Cell) {
        if (error) {
            *error = QStringLiteral("Cell -self requires a cell source");
        }
        return false;
    }
    *itemId = src->id;
    *preferPage = src->pageId.isEmpty() ? m_root.id : src->pageId;
    return true;
}

bool PageSession::resolveShowSkip(PageTargetKind kind, const QString& sourcePageId,
                                  const QString& sourceTargetId, QString* skipId, QString* skipPage,
                                  QString* error)
{
    if (kind == PageTargetKind::Zone) {
        const PageNav::Docs docs = navDocs();
        if (!PageNav::locate(docs, sourcePageId, sourceTargetId, &PageDocument::findZone)
            && !PageNav::locate(docs, {}, sourceTargetId, &PageDocument::findZone)) {
            if (error) {
                *error = QStringLiteral("Zone -!self requires a zone source");
            }
            return false;
        }
        *skipId = sourceTargetId;
        *skipPage = sourcePageId;
        return true;
    }
    const PageTarget* src = sourceCell(sourcePageId, sourceTargetId);
    if (kind == PageTargetKind::Grid) {
        if (!src || src->gridId.isEmpty()) {
            if (error) {
                *error = QStringLiteral("Grid -!self requires a grid source");
            }
            return false;
        }
        *skipId = src->gridId;
        *skipPage = src->pageId;
        return true;
    }
    if (!src || src->kind != PageTarget::Kind::Cell) {
        if (error) {
            *error = QStringLiteral("Cell -!self requires a cell source");
        }
        return false;
    }
    *skipId = sourceTargetId;
    *skipPage = sourcePageId;
    return true;
}

bool PageSession::applyShowNav(PageVerb verb, PageTargetKind kind, PageNavScope scope,
                               const QString& id, const QString& sourcePageId,
                               const QString& sourceTargetId, QString* error)
{
    const QString label = PageNav::kindLabel(kind);
    if (scope == PageNavScope::All || scope == PageNavScope::Others) {
        if (verb == PageVerb::Open && scope == PageNavScope::All) {
            if (error) {
                *error = QStringLiteral("Show %1 -all is not valid").arg(label);
            }
            return false;
        }
        QString skipId;
        QString skipPage;
        if (scope == PageNavScope::Others
            && !resolveShowSkip(kind, sourcePageId, sourceTargetId, &skipId, &skipPage, error)) {
            return false;
        }
        PageNav::applyScope(navDocs(), kind, verb, skipId, skipPage);
        return true;
    }

    QString itemId = id;
    QString preferPage = sourcePageId;
    if (scope == PageNavScope::Self
        && !resolveShowSelf(kind, sourcePageId, sourceTargetId, &itemId, &preferPage, error)) {
        return false;
    }
    if (itemId.isEmpty()) {
        if (error) {
            *error = QStringLiteral("%1 target is empty").arg(label);
        }
        return false;
    }

    const PageNav::Docs docs = navDocs();
    if (kind == PageTargetKind::Grid) {
        PageGrid* g = PageNav::locate(docs, preferPage, itemId, &PageDocument::findGrid);
        if (!g) {
            if (error) {
                *error = QStringLiteral("Unknown grid: %1").arg(itemId);
            }
            return false;
        }
        g->show = PageNav::shownAfter(verb, g->show);
        return true;
    }
    if (kind == PageTargetKind::Zone) {
        PageZone* z = PageNav::locate(docs, preferPage, itemId, &PageDocument::findZone);
        if (!z) {
            if (error) {
                *error = QStringLiteral("Unknown zone: %1").arg(itemId);
            }
            return false;
        }
        z->show = PageNav::shownAfter(verb, z->show);
        return true;
    }
    PageCell* c = PageNav::locate(docs, preferPage, itemId, &PageDocument::findCell);
    if (!c) {
        if (error) {
            *error = QStringLiteral("Unknown cell: %1").arg(itemId);
        }
        return false;
    }
    c->show = PageNav::shownAfter(verb, c->show);
    return true;
}

void PageSession::emitShowChanged()
{
    rebuild();
    raise();
    emit sessionChanged();
}

} // namespace gazer
