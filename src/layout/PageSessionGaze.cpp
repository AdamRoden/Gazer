#include "layout/PageSession.h"

#include "layout/PageHit.h"
#include "ui/PageHostWindow.h"
#include "utils/ScreenGrab.h"

#include <QStringList>
#include <QTransform>
#include <QtGlobal>

namespace gazer {

void PageSession::setLayoutAutoClose(bool on, int idleMs)
{
    m_layoutAutoClose = on;
    m_layoutAutoCloseIdleMs = qBound(500, idleMs, 120000);
    syncAutoClose();
}

int PageSession::autoCloseIdleMs() const
{
    // Duration is the global Settings value. XML autoClose flags only opt boards in.
    if (!m_layoutAutoClose) {
        return -1;
    }
    bool any = false;
    for (const PageGrid& g : m_root.grids) {
        if (g.autoClose && layersVisible(g.layers, m_root.showLayers) && !g.id.isEmpty()) {
            any = true;
            break;
        }
    }
    if (!any) {
        for (const AttachedPage& a : m_attached) {
            if (a.doc.autoClose) {
                any = true;
                break;
            }
            for (const PageGrid& g : a.doc.grids) {
                if (g.autoClose) {
                    any = true;
                    break;
                }
            }
            if (any) {
                break;
            }
        }
    }
    return any ? m_layoutAutoCloseIdleMs : -1;
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
    hideRootAutoClose(true);
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

void PageSession::applyDwellFor(const PageTarget& t)
{
    const bool daily = usesDailyDriverDwell(t.actions, t.phases);
    int scan = daily ? m_dailyScanGraceMs : m_globalScanGraceMs;
    int grace = m_globalGraceMs;
    QVector<int> seq = daily ? m_dailySequence : m_globalSequence;
    if (t.dwell.scanGrace) {
        scan = *t.dwell.scanGrace;
    }
    if (t.dwell.dwellGrace) {
        grace = *t.dwell.dwellGrace;
    }
    if (t.dwell.activation && !t.dwell.activation->isEmpty()) {
        seq = *t.dwell.activation;
    } else if (!t.phases.isEmpty()) {
        const int ms = seq.isEmpty() ? 400 : seq.first();
        seq = {ms};
    }
    m_dwell.setScanGraceMs(scan);
    m_dwell.setInvalidGraceMs(grace);
    m_dwell.setDwellSequence(seq);
}

QTransform PageSession::hitXf() const
{
    return PageHit::drawerTransform(m_targets, m_drawerScale, m_gridPaints);
}

const PageTarget* PageSession::findLiveTarget(const QString& pageId, const QString& targetId) const
{
    if (targetId.isEmpty()) {
        return nullptr;
    }
    if (const PageTarget* t = findTarget(targetId)) {
        return t;
    }
    for (const PageTarget& cand : m_targets) {
        if (cand.id == targetId && (pageId.isEmpty() || cand.pageId == pageId)) {
            return &cand;
        }
    }
    return nullptr;
}

PageSession::GazeHit PageSession::classifyGaze(const GazePoint& point) const
{
    GazeHit out;
    if (!hasRoot() || !point.valid) {
        return out;
    }
    const QTransform xf = hitXf();
    const QPointF g = point.toPointF();
    const QString engaged =
        (m_dwell.isScanGraceComplete() && !m_hoverId.isEmpty()) ? m_hoverId : QString();
    const PageTarget* hit = PageHit::at(m_targets, g, m_drawerScale, engaged, m_gridPaints, &xf);
    out.target = hit;
    if (hit) {
        out.overBoard = true;
        out.overMaster = hit->master;
        out.overActivator = !m_aimActivator.isEmpty() && sessionKey(*hit) == m_aimActivator;
    }
    if (!out.overMaster) {
        for (int i = m_gridPaints.size() - 1; i >= 0; --i) {
            const PageGridPaint& gp = m_gridPaints.at(i);
            if (!gp.master || gp.visual.isEmpty()) {
                continue;
            }
            const QRectF z = PageHit::mapDrawer(gp.drawerMotion, gp.visual, xf, m_drawerScale);
            if (PageHit::shapeContains(z, gp.chrome, g)) {
                out.overMaster = true;
                out.overBoard = true;
                break;
            }
        }
    }
    if (!out.overBoard) {
        out.overBoard =
            !PageHit::coveringPageId(m_gridPaints, g, m_drawerScale, m_targets, &xf).isEmpty();
    }
    return out;
}

bool PageSession::feedGaze(const GazePoint& point, const GazeHit& classified, GazeScope scope)
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
    const PageTarget* hit = classified.target;
    if (scope == GazeScope::MasterAndActivator) {
        const bool allow = (hit && hit->master) || classified.overActivator;
        if (!allow) {
            leaveGaze();
            return classified.overMaster;
        }
    }
    if (blockedByLeaveGate(hit)) {
        leaveGaze();
        return classified.overBoard;
    }
    const QString id = hit ? sessionKey(*hit) : QString();
    if (hit && !m_dwellSuspended) {
        noteActivity();
    }
    if (hit && id != m_hoverId) {
        if (m_loopLatchClear) {
            m_loopLatchClear();
        }
        applyDwellFor(*hit);
    }
    m_dwell.onGazeSample(point, id);
    return hit != nullptr;
}

bool PageSession::onGaze(const GazePoint& point, GazeScope scope)
{
    return feedGaze(point, classifyGaze(point), scope);
}

bool PageSession::hitsChrome(const GazePoint& point) const
{
    return classifyGaze(point).overBoard;
}

QRect PageSession::targetScreenRect(const QString& pageId, const QString& targetId) const
{
    const PageTarget* t = findLiveTarget(pageId, targetId);
    if (!t) {
        return {};
    }
    const QTransform xf = hitXf();
    QRectF r = PageHit::gazeHitRect(*t, {});
    r = PageHit::mapDrawer(*t, r, xf, m_drawerScale);
    return r.toAlignedRect();
}

void PageSession::setAimActivator(const QString& pageId, const QString& targetId)
{
    m_aimActivator.clear();
    if (const PageTarget* t = findLiveTarget(pageId, targetId)) {
        m_aimActivator = sessionKey(*t);
    }
}

void PageSession::clearAimActivator()
{
    m_aimActivator.clear();
}

bool PageSession::hitsPage(const QString& id, const QPointF& pos) const
{
    if (id.isEmpty()) {
        return false;
    }
    const QTransform xf = hitXf();
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
    const QTransform xf = hitXf();
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

void PageSession::stampLivePhases()
{
    for (PageTarget& t : m_targets) {
        t.phaseIndex = m_dwellPhases.current(sessionKey(t)).value_or(-1);
    }
}

void PageSession::setLivePhase(const QString& key, int index)
{
    for (PageTarget& live : m_targets) {
        if (sessionKey(live) == key) {
            live.phaseIndex = index;
            break;
        }
    }
    if (m_host) {
        m_host->setTargetPhase(key, index);
    }
}

void PageSession::advanceDwellPhase(const PageTarget& t)
{
    const QString key = sessionKey(t);
    const int before = m_dwellPhases.current(key).value_or(-1);
    m_dwellPhases.onActivated(key, t.phases.size());
    const int idx = m_dwellPhases.current(key).value_or(0);
    setLivePhase(key, idx);
    if (m_host && idx != before) {
        m_host->flash(key);
    }
}

void PageSession::commitDwellPhase(const QString& targetId)
{
    const std::optional<int> idx = m_dwellPhases.takeCommit(targetId);
    if (!idx) {
        return;
    }
    const PageTarget* t = findTarget(targetId);
    if (!t || t->phases.isEmpty()) {
        return;
    }
    const int i = qBound(0, *idx, t->phases.size() - 1);
    const QVector<PageAction> actions = t->phases[i].actions;
    const QString pageId = t->pageId.isEmpty() ? m_root.id : t->pageId;
    setLivePhase(targetId, -1);
    if (m_host) {
        m_host->flash(targetId);
    }
    emit targetActivated(pageId, t->id);
    if (m_dispatch) {
        m_dispatch(actions, pageId, t->id);
    }
}

void PageSession::activateTarget(const QString& targetId)
{
    const PageTarget* t = findTarget(targetId);
    if (!t) {
        return;
    }
    if (!t->phases.isEmpty()) {
        advanceDwellPhase(*t);
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
