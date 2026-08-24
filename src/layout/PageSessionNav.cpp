#include "layout/PageSession.h"

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
    b.chrome = m_chrome;
    b.hiddenZones = m_hiddenZones;
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

    m_attached.clear();
    for (PageDocument& doc : snap.attached) {
        AttachedPage ap;
        ap.doc = std::move(doc);
        m_attached.push_back(std::move(ap));
    }
    m_hiddenZones = std::move(snap.hiddenZones);
    m_chrome = snap.chrome;
    m_props.insert(QStringLiteral("expanded"), m_chrome != RootChrome::Docked);
    m_drawerTimer.stop();
    m_drawerPhase = DrawerPhase::Idle;
    m_drawerScale = 1.0;
    rebuild();
    raise();
    emit sessionChanged();
}

const PageZone* PageSession::findZoneAnywhere(const QString& id) const
{
    if (id.isEmpty()) {
        return nullptr;
    }
    if (const PageZone* z = m_root.findZone(id)) {
        return z;
    }
    for (const AttachedPage& a : m_attached) {
        if (const PageZone* z = a.doc.findZone(id)) {
            return z;
        }
    }
    return nullptr;
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

bool PageSession::applyNavMutation(const PageAction& action, const QString& sourcePageId,
                                   const QString& sourceTargetId, QString* error)
{
    switch (action.targetKind) {
    case PageTargetKind::Page:
        return applyNavPage(action.verb, action.targetScope, action.targetId, sourcePageId, error);
    case PageTargetKind::Grid:
        return applyNavGrid(action.verb, action.targetScope, action.targetId, error);
    case PageTargetKind::Zone:
        return applyNavZone(action.verb, action.targetScope, action.targetId, sourceTargetId,
                            error);
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
        setRootChrome(RootChrome::Drawer);
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

bool PageSession::applyNavGrid(PageVerb verb, PageNavScope scope, const QString& id, QString* error)
{
    if (scope == PageNavScope::Others) {
        if (error) {
            *error = QStringLiteral("Grid target -!self is not supported");
        }
        return false;
    }
    if (scope == PageNavScope::All || scope == PageNavScope::Self) {
        if (verb == PageVerb::Open) {
            if (error) {
                *error = QStringLiteral("Open Grid %1 is not valid")
                             .arg(scope == PageNavScope::All ? QStringLiteral("-all")
                                                             : QStringLiteral("-self"));
            }
            return false;
        }
        setRootChrome(RootChrome::Docked);
        return true;
    }

    const PageGrid* g = m_root.findGrid(id);
    if (!g || g->rootSlot == PageRootSlot::None) {
        if (error) {
            *error = QStringLiteral("Unknown root chrome grid: %1").arg(id);
        }
        return false;
    }
    const RootChrome slot = chromeForSlot(g->rootSlot);
    const bool showing = (m_chrome == slot);
    bool show = true;
    if (verb == PageVerb::Close) {
        show = false;
    } else if (verb == PageVerb::Toggle) {
        show = !showing;
    }
    setRootChrome(show ? slot : RootChrome::Docked);
    return true;
}

bool PageSession::applyNavZone(PageVerb verb, PageNavScope scope, const QString& id,
                               const QString& sourceTargetId, QString* error)
{
    QStringList ids;
    if (scope == PageNavScope::All) {
        auto collect = [&](const PageDocument& doc) {
            for (const PageZone& z : doc.zones) {
                if (!z.id.isEmpty()) {
                    ids.push_back(z.id);
                }
            }
        };
        collect(m_root);
        for (const AttachedPage& a : m_attached) {
            collect(a.doc);
        }
    } else if (scope == PageNavScope::Self) {
        if (!findZoneAnywhere(sourceTargetId)) {
            if (error) {
                *error = QStringLiteral("Zone -self requires a zone source");
            }
            return false;
        }
        ids.push_back(sourceTargetId);
    } else if (scope == PageNavScope::Others) {
        if (!findZoneAnywhere(sourceTargetId)) {
            if (error) {
                *error = QStringLiteral("Zone -!self requires a zone source");
            }
            return false;
        }
        auto collect = [&](const PageDocument& doc) {
            for (const PageZone& z : doc.zones) {
                if (!z.id.isEmpty() && z.id != sourceTargetId) {
                    ids.push_back(z.id);
                }
            }
        };
        collect(m_root);
        for (const AttachedPage& a : m_attached) {
            collect(a.doc);
        }
    } else {
        if (id.isEmpty()) {
            if (error) {
                *error = QStringLiteral("Zone target is empty");
            }
            return false;
        }
        ids.push_back(id);
    }

    if (verb == PageVerb::Open && scope == PageNavScope::All) {
        if (error) {
            *error = QStringLiteral("Open Zone -all is not valid");
        }
        return false;
    }

    for (const QString& one : ids) {
        const bool nowHidden = m_hiddenZones.contains(one);
        bool show = true;
        if (verb == PageVerb::Close) {
            show = false;
        } else if (verb == PageVerb::Toggle) {
            show = nowHidden;
        }
        if (show) {
            m_hiddenZones.remove(one);
        } else {
            m_hiddenZones.insert(one);
        }
    }
    rebuild();
    raise();
    emit sessionChanged();
    return true;
}

} // namespace gazer
