#include "layout/PageSession.h"

#include "layout/PageHit.h"
#include "ui/PageHostWindow.h"
#include "utils/ScreenGrab.h"

#include <QStringList>
#include <QTransform>
#include <QtGlobal>

namespace gazer {

int PageSession::autoCloseIdleMs() const
{
    int best = -1;
    auto consider = [&](int ms) {
        if (ms <= 0) {
            ms = 3000;
        }
        best = (best < 0) ? ms : qMin(best, ms);
    };
    if (m_chrome == RootChrome::Drawer) {
        for (const PageGrid& g : m_root.grids) {
            if (!g.autoClose || g.id.isEmpty() || hiddenRootGrids().contains(g.id)) {
                continue;
            }
            consider(g.autoCloseIdleMs);
        }
    }
    for (const AttachedPage& a : m_attached) {
        if (a.doc.autoClose) {
            consider(a.doc.autoCloseIdleMs);
            continue;
        }
        for (const PageGrid& g : a.doc.grids) {
            if (g.autoClose) {
                consider(g.autoCloseIdleMs);
            }
        }
    }
    return best;
}

void PageSession::noteActivity()
{
    m_idleClock.start();
    m_idleClockRunning = true;
}

void PageSession::syncAutoClose()
{
    if (m_dwellSuspended || autoCloseIdleMs() < 0) {
        m_autoCloseTimer.stop();
        m_idleClockRunning = false;
        return;
    }
    if (!m_idleClockRunning) {
        noteActivity();
    }
    if (!m_autoCloseTimer.isActive()) {
        m_autoCloseTimer.start();
    }
}

void PageSession::tickAutoClose()
{
    if (m_dwellSuspended) {
        return;
    }
    const int idle = autoCloseIdleMs();
    if (idle < 0 || !m_idleClockRunning) {
        return;
    }
    if (m_idleClock.elapsed() < idle) {
        return;
    }
    QStringList drop;
    for (const AttachedPage& a : m_attached) {
        bool ac = a.doc.autoClose;
        for (const PageGrid& g : a.doc.grids) {
            ac = ac || g.autoClose;
        }
        if (ac) {
            drop.push_back(a.doc.id);
        }
    }
    for (const QString& id : drop) {
        closePage(id);
    }
    if (m_chrome == RootChrome::Drawer) {
        bool rootAc = false;
        for (const PageGrid& g : m_root.grids) {
            if (g.autoClose && g.rootSlot == PageRootSlot::Drawer) {
                rootAc = true;
                break;
            }
        }
        if (rootAc) {
            setRootChrome(RootChrome::Docked);
        }
    }
}

const PageTarget* PageSession::findTarget(const QString& id) const
{
    for (const PageTarget& t : m_targets) {
        if (sessionKey(t) == id) {
            return &t;
        }
    }
    return nullptr;
}

void PageSession::applyDwellFor(const PageTarget* t)
{
    int scan = m_globalScanGraceMs;
    int grace = m_globalGraceMs;
    QVector<int> seq = m_globalSequence;
    if (t) {
        if (t->dwell.scanGrace) {
            scan = *t->dwell.scanGrace;
        }
        if (t->dwell.dwellGrace) {
            grace = *t->dwell.dwellGrace;
        }
        if (t->dwell.activation && !t->dwell.activation->isEmpty()) {
            seq = *t->dwell.activation;
        }
    }
    m_dwell.setScanGraceMs(scan);
    m_dwell.setInvalidGraceMs(grace);
    m_dwell.setDwellSequence(seq);
}

bool PageSession::onGaze(const GazePoint& point)
{
    if (!hasRoot()) {
        return false;
    }
    if (!point.valid) {
        GazePoint invalid = point;
        invalid.valid = false;
        m_dwell.onGazeSample(invalid, {});
        return !m_hoverId.isEmpty();
    }
    m_lastGaze = point;
    const QTransform* xf = m_host ? &m_host->drawerXf() : nullptr;
    const QString engaged =
        (m_dwell.isScanGraceComplete() && !m_hoverId.isEmpty()) ? m_hoverId : QString();
    const PageTarget* hit = PageHit::at(m_targets, point.toPointF(), m_drawerScale, engaged,
                                        m_gridPaints, xf);
    if (blockedByLeaveGate(hit)) {
        leaveGaze();
        return hitsChrome(point);
    }
    const QString id = hit ? sessionKey(*hit) : QString();
    if (hit && !m_dwellSuspended) {
        noteActivity();
    }
    if (id != m_hoverId) {
        if (m_loopLatchClear) {
            m_loopLatchClear();
        }
        applyDwellFor(hit);
    }
    m_dwell.onGazeSample(point, id);
    return hit != nullptr;
}

bool PageSession::hitsChrome(const GazePoint& point) const
{
    if (!hasRoot() || !point.valid) {
        return false;
    }
    const QTransform* xf = m_host ? &m_host->drawerXf() : nullptr;
    const QPointF g = point.toPointF();
    if (PageHit::at(m_targets, g, m_drawerScale, {}, m_gridPaints, xf)) {
        return true;
    }
    return !PageHit::coveringPageId(m_gridPaints, g, m_drawerScale, m_targets, xf).isEmpty();
}

bool PageSession::hitsPage(const QString& id, const QPointF& pos) const
{
    if (id.isEmpty()) {
        return false;
    }
    const QTransform xf = m_host ? m_host->drawerXf()
                                 : PageHit::drawerTransform(m_targets, m_drawerScale, m_gridPaints);
    for (const PageGridPaint& g : m_gridPaints) {
        if (g.pageId != id || g.visual.isEmpty()) {
            continue;
        }
        const QRectF z = PageHit::mapDrawer(g.drawerMotion, g.visual, xf, m_drawerScale);
        if (PageHit::shapeContains(z, g.chrome, pos)) {
            return true;
        }
    }
    return false;
}

void PageSession::leaveGaze()
{
    if (m_loopLatchClear) {
        m_loopLatchClear();
    }
    m_dwell.leave();
    m_hoverId.clear();
}

void PageSession::raise()
{
    if (m_host) {
        m_host->raiseHost();
    }
}

void PageSession::hideHost()
{
    if (m_host) {
        m_host->hide();
    }
}

QVector<QRect> PageSession::unpauseGapRects() const
{
    QVector<QRect> gaps;
    const QTransform xf = m_host ? m_host->drawerXf()
                                 : PageHit::drawerTransform(m_targets, m_drawerScale, m_gridPaints);
    const QString engaged =
        (m_dwell.isScanGraceComplete() && !m_hoverId.isEmpty()) ? m_hoverId : QString();
    auto isUnpause = [](const PageTarget& t) {
        for (const PageAction& a : t.actions) {
            if (a.type == PageActionType::Command
                && (a.command == QLatin1String("toggleDwellSuspend")
                    || a.command == QLatin1String("resumeDwell"))) {
                return true;
            }
        }
        return false;
    };
    for (const PageTarget& t : m_targets) {
        if (!t.interactive || !isUnpause(t)) {
            continue;
        }
        const QString key = (sessionKey(t) == engaged) ? engaged : QString();
        QRectF r = PageHit::gazeHitRect(t, key);
        r = PageHit::mapDrawer(t, r, xf, m_drawerScale);
        const QRectF vis = QRectF(virtualDesktop()).intersected(r);
        if (!vis.isEmpty()) {
            gaps.push_back(vis.toRect().adjusted(-16, -16, 16, 16));
        }
    }
    return gaps;
}

void PageSession::activateTarget(const QString& targetId)
{
    const PageTarget* t = findTarget(targetId);
    if (!t) {
        return;
    }
    const QVector<PageAction> actions = t->actions;
    const QString pageId = t->pageId.isEmpty() ? m_root.id : t->pageId;
    if (m_host) {
        m_host->flash(sessionKey(*t));
    }
    emit targetActivated(pageId, t->id);
    if (t->actionLoop && m_loopToggle) {
        m_loopToggle(*t, pageId);
        return;
    }
    if (m_dispatch) {
        m_dispatch(actions, pageId, t->id);
    }
}

} // namespace gazer
