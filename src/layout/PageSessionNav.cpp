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

bool PageSession::applyNav(const PageAction& action, const QString& sourcePageId,
                           const QString& sourceTargetId, QString* error)
{
    Q_UNUSED(sourceTargetId);
    if (action.type == PageActionType::GoBack) {
        return goBack(error);
    }
    if (action.type != PageActionType::Nav) {
        if (error) {
            *error = QStringLiteral("Not a page navigation action");
        }
        return false;
    }
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
    if (!applyNavPage(action.verb, action.targetScope, action.targetId, sourcePageId, error)) {
        return false;
    }
    if (action.breadcrumb) {
        m_crumbs.push_back(std::move(snap));
    }
    return true;
}

bool PageSession::showLayers(const QVector<PageAction>& actions, const QString& sourcePageId,
                             const QString& sourceTargetId, QString* error)
{
    Q_UNUSED(sourceTargetId);
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
        if (a.type != PageActionType::ShowLayers) {
            if (error) {
                *error = QStringLiteral("showLayers is ShowLayers only");
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
        if (!applyShowLayers(a.layers, sourcePageId, error)) {
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

PageDocument* PageSession::navPage(const QString& sourcePageId)
{
    if (PageDocument* d = PageNav::page(navDocs(), sourcePageId)) {
        return d;
    }
    // Source page already closed in this activation (ClosePage then ShowLayers).
    return hasRoot() ? &m_root : nullptr;
}

bool PageSession::applyShowLayers(const QVector<int>& layers, const QString& sourcePageId,
                                 QString* error)
{
    PageDocument* doc = navPage(sourcePageId);
    if (!doc) {
        if (error) {
            *error = QStringLiteral("No page for ShowLayers");
        }
        return false;
    }
    doc->showLayers = layers.isEmpty() ? defaultLayers() : layers;
    return true;
}

void PageSession::emitShowChanged()
{
    rebuild();
    raise();
    emit sessionChanged();
}

} // namespace gazer
