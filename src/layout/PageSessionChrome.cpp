#include "layout/PageSession.h"

#include "layout/PageNav.h"
#include "ui/PageHostWindow.h"

#include <QtGlobal>

namespace gazer {

void PageSession::syncExpanded()
{
    m_props.insert(QStringLiteral("expanded"),
                   m_drawerPhase == DrawerPhase::Dismiss || anyRootGridShown());
}

bool PageSession::anyRootGridShown() const
{
    for (const PageGrid& g : m_root.grids) {
        if (layersVisible(g.layers, m_root.showLayers)) {
            return true;
        }
    }
    return false;
}

bool PageSession::drawerMotionShown() const
{
    for (const PageGrid& g : m_root.grids) {
        if (g.drawerMotion && layersVisible(g.layers, m_root.showLayers)) {
            return true;
        }
    }
    return false;
}

bool PageSession::nonDrawerRootShown() const
{
    for (const PageGrid& g : m_root.grids) {
        if (!g.drawerMotion && layersVisible(g.layers, m_root.showLayers)) {
            return true;
        }
    }
    return false;
}

void PageSession::resetDrawerAnim()
{
    m_drawerTimer.stop();
    m_drawerPhase = DrawerPhase::Idle;
    m_drawerScale = 1.0;
}

void PageSession::snapHideDrawerMotion()
{
    resetDrawerAnim();
    QVector<int> drop;
    for (const PageGrid& g : m_root.grids) {
        if (g.drawerMotion) {
            for (int n : g.layers) {
                if (!drop.contains(n)) {
                    drop.push_back(n);
                }
            }
        }
    }
    if (!drop.isEmpty()) {
        PageNav::dropLayers(m_root.showLayers, drop);
    }
}

void PageSession::hideRootAutoClose(bool animate)
{
    const bool wasDrawer = drawerMotionShown();
    QVector<int> drop;
    for (const PageGrid& g : m_root.grids) {
        if (g.autoClose && layersVisible(g.layers, m_root.showLayers)) {
            for (int n : g.layers) {
                if (!drop.contains(n)) {
                    drop.push_back(n);
                }
            }
        }
    }
    if (drop.isEmpty()) {
        return;
    }
    PageNav::dropLayers(m_root.showLayers, drop);
    if (!animate) {
        snapHideDrawerMotion();
        emitShowChanged();
        return;
    }
    syncDrawerAnim(wasDrawer);
    emitShowChanged();
}

void PageSession::syncDrawerAnim(bool wasDrawer)
{
    switch (PageNav::reconcileDrawer(wasDrawer, drawerMotionShown(), nonDrawerRootShown(),
                                     m_drawerPhase == DrawerPhase::Dismiss)) {
    case PageNav::DrawerAnim::Appear:
        playDrawerAppear();
        break;
    case PageNav::DrawerAnim::Dismiss:
        playDrawerDismiss();
        break;
    case PageNav::DrawerAnim::Snap:
        resetDrawerAnim();
        break;
    case PageNav::DrawerAnim::Keep:
        break;
    }
}

void PageSession::playDrawerAppear()
{
    m_drawerPhase = DrawerPhase::Appear;
    m_drawerScale = kDrawerMinScale;
    m_drawerClock.restart();
    m_drawerTimer.start();
}

void PageSession::playDrawerDismiss()
{
    m_drawerPhase = DrawerPhase::Dismiss;
    m_drawerClock.restart();
    if (!m_drawerTimer.isActive()) {
        m_drawerTimer.start();
    }
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
        rebuild();
        emit sessionChanged();
        return;
    }
    m_drawerPhase = DrawerPhase::Idle;
    m_drawerScale = 1.0;
    syncDrawerScale();
}

} // namespace gazer
