#include "layout/PageSession.h"

#include "ui/PageHostWindow.h"

#include <QtGlobal>

namespace gazer {

PageSession::RootChrome PageSession::chromeForSlot(PageRootSlot slot) const
{
    switch (slot) {
    case PageRootSlot::Drawer:
        return RootChrome::Drawer;
    case PageRootSlot::Quit:
        return RootChrome::Quit;
    case PageRootSlot::None:
        break;
    }
    return RootChrome::Docked;
}

QSet<QString> PageSession::hiddenRootGrids() const
{
    QSet<QString> hidden;
    for (const PageGrid& g : m_root.grids) {
        if (g.id.isEmpty() || g.rootSlot == PageRootSlot::None) {
            continue;
        }
        if (chromeForSlot(g.rootSlot) != m_chrome) {
            hidden.insert(g.id);
        }
    }
    return hidden;
}

void PageSession::setRootChrome(RootChrome next, bool animate)
{
    if (next == m_chrome) {
        if (next == RootChrome::Drawer && m_drawerPhase == DrawerPhase::Dismiss) {
            m_drawerTimer.stop();
            m_drawerPhase = DrawerPhase::Idle;
            m_drawerScale = 1.0;
            rebuild();
            emit sessionChanged();
        }
        raise();
        return;
    }

    const RootChrome prev = m_chrome;
    if (animate && next == RootChrome::Docked && prev == RootChrome::Drawer
        && m_drawerPhase != DrawerPhase::Dismiss) {
        playDrawerDismiss();
        return;
    }

    m_chrome = next;
    m_props.insert(QStringLiteral("expanded"), next != RootChrome::Docked);

    if (next == RootChrome::Drawer) {
        playDrawerAppear();
        return;
    }

    m_drawerTimer.stop();
    m_drawerPhase = DrawerPhase::Idle;
    m_drawerScale = 1.0;
    rebuild();
    raise();
    emit sessionChanged();
}
void PageSession::playDrawerAppear()
{
    m_drawerPhase = DrawerPhase::Appear;
    m_drawerScale = kDrawerMinScale;
    m_drawerClock.restart();
    rebuild();
    raise();
    m_drawerTimer.start();
    emit sessionChanged();
}

void PageSession::playDrawerDismiss()
{
    m_drawerPhase = DrawerPhase::Dismiss;
    m_drawerClock.restart();
    if (!m_drawerTimer.isActive()) {
        m_drawerTimer.start();
    }
    syncDrawerScale();
}

void PageSession::syncDrawerScale()
{
    if (m_host) {
        m_host->setDrawerScale(m_drawerScale);
    }
}

void PageSession::tickDrawer()
{
    if (m_drawerPhase == DrawerPhase::Idle) {
        m_drawerTimer.stop();
        return;
    }
    const int dur = m_drawerPhase == DrawerPhase::Appear ? kDrawerAppearMs : kDrawerDismissMs;
    const double t = qBound(0.0, double(m_drawerClock.elapsed()) / double(dur), 1.0);
    if (m_drawerPhase == DrawerPhase::Appear) {
        const double e = 1.0 - (1.0 - t) * (1.0 - t);
        m_drawerScale = kDrawerMinScale + (1.0 - kDrawerMinScale) * e;
    } else {
        const double e = t * t;
        m_drawerScale = 1.0 - (1.0 - kDrawerMinScale) * e;
    }
    if (t < 1.0) {
        syncDrawerScale();
        return;
    }
    m_drawerTimer.stop();
    if (m_drawerPhase == DrawerPhase::Dismiss) {
        m_drawerPhase = DrawerPhase::Idle;
        m_drawerScale = 1.0;
        m_chrome = RootChrome::Docked;
        m_props.insert(QStringLiteral("expanded"), false);
        rebuild();
        emit sessionChanged();
        return;
    }
    m_drawerPhase = DrawerPhase::Idle;
    m_drawerScale = 1.0;
    syncDrawerScale();
}

} // namespace gazer
