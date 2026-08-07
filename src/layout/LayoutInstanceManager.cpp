#include "layout/LayoutInstanceManager.h"

#include "utils/Log.h"

#include <algorithm>

namespace gazer {

LayoutInstanceManager::LayoutInstanceManager(LayoutManager& catalog, QObject* parent)
    : QObject(parent)
    , m_catalog(catalog)
{
}

QString LayoutInstanceManager::makeInstanceId(const QString& layoutId)
{
    return QStringLiteral("%1#%2").arg(layoutId).arg(m_nextSerial++);
}

void LayoutInstanceManager::wireInstance(LayoutInstance* inst)
{
    if (!inst) {
        return;
    }
    if (m_edgeBubbles) {
        inst->setEdgeBubbleOverlay(m_edgeBubbles);
    }
    inst->setDwellSuspended(m_dwellSuspended);
    inst->setTheme(m_theme);
    inst->setProgressVisuals(m_progressVisuals);
    connect(inst, &LayoutInstance::itemActivated, this, &LayoutInstanceManager::itemActivated);
    connect(inst, &LayoutInstance::windowCloseRequested, this,
            &LayoutInstanceManager::onWindowCloseRequested);
    connect(inst, &LayoutInstance::dwellEngagementEnded, this,
            &LayoutInstanceManager::dwellEngagementEnded);
}

void LayoutInstanceManager::fireLifecycle(const QVector<LayoutAction>& actions,
                                          const QString& instanceId) const
{
    if (actions.isEmpty() || !m_lifecycleRunner) {
        return;
    }
    m_lifecycleRunner(actions, instanceId);
}

void LayoutInstanceManager::setDwellSuspended(bool suspended)
{
    if (m_dwellSuspended == suspended) {
        return;
    }
    m_dwellSuspended = suspended;
    for (const auto& p : m_instances) {
        if (p) {
            p->setDwellSuspended(suspended);
            if (suspended) {
                p->leaveGaze();
            }
        }
    }
    if (suspended) {
        leaveActiveGaze();
    }
    GAZER_INFO << "Dwell capture" << (suspended ? "SUSPENDED" : "resumed");
    emit dwellSuspendChanged(m_dwellSuspended);
}

LayoutInstance* LayoutInstanceManager::instance(const QString& instanceId) const
{
    for (const auto& p : m_instances) {
        if (p && p->instanceId() == instanceId) {
            return p.get();
        }
    }
    return nullptr;
}

LayoutInstance* LayoutInstanceManager::focusedInstance() const
{
    return instance(m_focusedId);
}

LayoutInstance* LayoutInstanceManager::masterInstance() const
{
    return instance(m_masterId);
}

QVector<LayoutInstance*> LayoutInstanceManager::instances() const
{
    QVector<LayoutInstance*> out;
    out.reserve(static_cast<int>(m_instances.size()));
    for (const auto& p : m_instances) {
        if (p) {
            out.push_back(p.get());
        }
    }
    return out;
}

QVector<InstanceTarget> LayoutInstanceManager::otherInstanceTargets() const
{
    QVector<InstanceTarget> out;
    for (const auto& p : m_instances) {
        if (!p || p->instanceId() == m_focusedId) {
            continue;
        }
        if (!p->window() || !p->window()->isVisible()) {
            continue;
        }
        out.push_back(InstanceTarget{p->instanceId(), p->document().name, p->centerScreen()});
    }
    return out;
}

const LayoutDocument* LayoutInstanceManager::requireDoc(const QString& layoutId, QString* error)
{
    QString loadErr;
    if (!m_catalog.loadLayoutId(layoutId, &loadErr)) {
        if (error) {
            *error = loadErr;
        }
        return nullptr;
    }
    const LayoutDocument* doc = m_catalog.document(layoutId);
    if (!doc) {
        if (error) {
            *error = QStringLiteral("Layout not in catalog: %1").arg(layoutId);
        }
        return nullptr;
    }
    return doc;
}

void LayoutInstanceManager::replaceInstanceDocument(LayoutInstance* inst, LayoutDocument newDoc,
                                                    bool fireOnLoad)
{
    if (!inst) {
        return;
    }
    const QString instanceId = inst->instanceId();
    // Outgoing layout lifecycle + stop sticky loops for this instance.
    fireLifecycle(inst->document().onClose, instanceId);
    if (m_instanceTeardown) {
        m_instanceTeardown(instanceId);
    }
    inst->setDocument(std::move(newDoc));
    if (!m_globalDwellSequence.isEmpty() || m_globalGraceMs > 0) {
        inst->setGlobalDwellOverride(m_globalDwellSequence, m_globalGraceMs);
    }
    inst->setProgressVisuals(m_progressVisuals);
    inst->setTheme(m_theme);
    if (fireOnLoad) {
        fireLifecycle(inst->document().onLoad, instanceId);
    }
}

bool LayoutInstanceManager::applyDocument(LayoutInstance* inst, const QString& layoutId,
                                          QString* error)
{
    const LayoutDocument* doc = requireDoc(layoutId, error);
    if (!doc) {
        return false;
    }
    replaceInstanceDocument(inst, decorateCopy(*doc), /*fireOnLoad=*/true);
    return true;
}

QString LayoutInstanceManager::homeLayoutIdForMaster() const
{
    auto* master = masterInstance();
    if (!master) {
        return {};
    }
    const auto& sess = master->document().session;
    if (sess.isHome) {
        return master->layoutId();
    }
    if (!sess.expandLayoutId.isEmpty()) {
        return sess.expandLayoutId;
    }
    // Never return the non-home shell id — that would no-op expand.
    return {};
}

void LayoutInstanceManager::setFocused(const QString& instanceId, bool raiseWindow)
{
    auto it = std::find_if(m_instances.begin(), m_instances.end(),
                           [&](const std::unique_ptr<LayoutInstance>& p) {
                               return p && p->instanceId() == instanceId;
                           });
    if (it == m_instances.end()) {
        return;
    }

    const bool changed = (m_focusedId != instanceId);
    m_focusedId = instanceId;

    // Only reorder the hit-test stack when we also raise the HWND. Gaze-driven
    // focus (raiseWindow=false) must not desync hit order from visual z-order.
    if (raiseWindow) {
        auto node = std::move(*it);
        m_instances.erase(it);
        m_instances.push_back(std::move(node));
        m_instances.back()->raise();
    }

    if (changed) {
        emit instanceFocused(instanceId);
    }
}

bool LayoutInstanceManager::eraseSecondary(const QString& instanceId, QString* error)
{
    if (instanceId == m_masterId) {
        if (error) {
            *error = QStringLiteral("Cannot erase master instance");
        }
        return false;
    }
    if (m_instances.size() <= 1) {
        if (error) {
            *error = QStringLiteral("Cannot close the last layout instance");
        }
        return false;
    }

    auto it = std::find_if(m_instances.begin(), m_instances.end(),
                           [&](const std::unique_ptr<LayoutInstance>& p) {
                               return p && p->instanceId() == instanceId;
                           });
    if (it == m_instances.end()) {
        if (error) {
            *error = QStringLiteral("Unknown instance");
        }
        return false;
    }

    if (m_gazeInstanceId == instanceId) {
        m_gazeInstanceId.clear();
    }
    if (m_focusedId == instanceId) {
        m_focusedId = m_masterId;
    }

    // onClose while instance still valid
    if (*it) {
        fireLifecycle((*it)->document().onClose, instanceId);
    }
    if (m_instanceTeardown) {
        m_instanceTeardown(instanceId);
    }

    m_instances.erase(it);
    emit instanceClosed(instanceId);
    return true;
}

bool LayoutInstanceManager::applyLoadLayout(const QString& sourceInstanceId,
                                            const QString& layoutId, QString* error)
{
    const LayoutDocument* target = requireDoc(layoutId, error);
    if (!target) {
        return false;
    }

    const bool sourceIsMaster = sourceInstanceId == m_masterId;

    if (target->isMasterShell()) {
        if (sourceIsMaster) {
            return navigateMaster(layoutId, error);
        }
        if (!returnToMaster(sourceInstanceId, /*closeSecondary=*/true, error)) {
            return false;
        }
        if (!target->session.isHome) {
            return navigateMaster(layoutId, error);
        }
        return true;
    }

    if (sourceIsMaster) {
        return !openSecondary(layoutId, error).isEmpty();
    }

    return loadInto(sourceInstanceId, layoutId, error);
}

bool LayoutInstanceManager::loadInto(const QString& instanceId, const QString& layoutId,
                                     QString* error)
{
    auto* inst = instance(instanceId);
    if (!inst) {
        if (error) {
            *error = QStringLiteral("Unknown instance: %1").arg(instanceId);
        }
        return false;
    }
    const LayoutDocument* doc = requireDoc(layoutId, error);
    if (!doc) {
        return false;
    }

    if (instanceId == m_masterId) {
        if (!doc->isMasterShell() || doc->session.masterGroup != m_masterGroup) {
            if (error) {
                *error = QStringLiteral("Cannot load non-master layout into master instance");
            }
            return false;
        }
    } else if (doc->isMasterShell()) {
        if (error) {
            *error = QStringLiteral("Use navigateMaster/returnToMaster for master shells");
        }
        return false;
    }

    replaceInstanceDocument(inst, decorateCopy(*doc), /*fireOnLoad=*/true);
    setFocused(instanceId, true);
    emit sessionChanged();
    return true;
}

bool LayoutInstanceManager::navigateMaster(const QString& layoutId, QString* error)
{
    const LayoutDocument* doc = requireDoc(layoutId, error);
    if (!doc) {
        return false;
    }
    if (!doc->isMasterShell()) {
        if (error) {
            *error = QStringLiteral("navigateMaster requires masterShell role: %1").arg(layoutId);
        }
        return false;
    }

    if (auto* master = masterInstance()) {
        if (!doc->session.masterGroup.isEmpty() && !m_masterGroup.isEmpty()
            && doc->session.masterGroup != m_masterGroup) {
            if (error) {
                *error = QStringLiteral("Master group mismatch");
            }
            return false;
        }
        replaceInstanceDocument(master, decorateCopy(*doc), /*fireOnLoad=*/true);
        // Always show after navigation (e.g. dock → Main must leave hidden state).
        // Application::syncMasterChrome may hide again only for gaze-reveal docks.
        master->raise();
        setFocused(master->instanceId(), true);
        emit sessionChanged();
        // Second raise after chrome handlers: expand must win over a prior hide.
        if (!doc->session.isGazeRevealDock()) {
            master->raise();
        }
        GAZER_INFO << "navigateMaster →" << layoutId
                   << (doc->session.isGazeRevealDock() ? "(gaze-dock)" : "(show)");
        return true;
    }

    const QString id = makeInstanceId(layoutId);
    auto inst = std::make_unique<LayoutInstance>(id, decorateCopy(*doc));
    if (!m_globalDwellSequence.isEmpty() || m_globalGraceMs > 0) {
        inst->setGlobalDwellOverride(m_globalDwellSequence, m_globalGraceMs);
    }
    inst->applyPlacement(0);
    wireInstance(inst.get());
    const QVector<LayoutAction> onOpen = inst->document().onOpen;
    const QVector<LayoutAction> onLoad = inst->document().onLoad;
    m_masterId = id;
    m_masterGroup = doc->session.masterGroup;
    m_instances.push_back(std::move(inst));
    setFocused(id, true);
    fireLifecycle(onOpen, id);
    fireLifecycle(onLoad, id);
    GAZER_INFO << "Opened master" << id << layoutId << "group" << m_masterGroup;
    emit instanceOpened(id, layoutId);
    emit sessionChanged();
    return true;
}

QString LayoutInstanceManager::openSecondary(const QString& layoutId, QString* error)
{
    const LayoutDocument* doc = requireDoc(layoutId, error);
    if (!doc) {
        return {};
    }
    if (doc->isMasterShell()) {
        if (error) {
            *error = QStringLiteral(
                "openSecondary refuses masterShell; use navigateMaster or openInstance");
        }
        return {};
    }

    const QString id = makeInstanceId(layoutId);
    auto inst = std::make_unique<LayoutInstance>(id, decorateCopy(*doc));
    if (!m_globalDwellSequence.isEmpty() || m_globalGraceMs > 0) {
        inst->setGlobalDwellOverride(m_globalDwellSequence, m_globalGraceMs);
    }
    inst->setProgressVisuals(m_progressVisuals);
    const int offset = static_cast<int>(m_instances.size()) * 40;
    inst->applyPlacement(offset);
    wireInstance(inst.get());
    const QVector<LayoutAction> onOpen = inst->document().onOpen;
    const QVector<LayoutAction> onLoad = inst->document().onLoad;
    m_instances.push_back(std::move(inst));
    setFocused(id, true);
    fireLifecycle(onOpen, id);
    fireLifecycle(onLoad, id);
    GAZER_INFO << "Opened secondary" << id << layoutId;
    emit instanceOpened(id, layoutId);
    emit sessionChanged();
    return id;
}

bool LayoutInstanceManager::returnToMaster(const QString& fromInstanceId, bool closeSecondary,
                                           QString* error)
{
    auto* master = masterInstance();
    if (!master) {
        if (error) {
            *error = QStringLiteral("No master instance");
        }
        return false;
    }

    const QString homeId = homeLayoutIdForMaster();
    if (!homeId.isEmpty() && master->layoutId() != homeId) {
        if (!applyDocument(master, homeId, error)) {
            return false;
        }
    }

    if (closeSecondary && fromInstanceId != m_masterId) {
        QString err;
        if (!eraseSecondary(fromInstanceId, &err)) {
            if (error) {
                *error = err;
            }
            return false;
        }
    }

    setFocused(m_masterId, true);
    master->raise();
    emit sessionChanged();
    return true;
}

void LayoutInstanceManager::raiseMaster()
{
    auto* master = masterInstance();
    if (!master) {
        return;
    }
    const QString homeId = homeLayoutIdForMaster();
    if (!homeId.isEmpty() && master->layoutId() != homeId) {
        QString err;
        if (!navigateMaster(homeId, &err)) {
            GAZER_WARN << "raiseMaster navigate failed:" << err << "homeId" << homeId;
            // Still try to show whatever master we have.
            setFocused(master->instanceId(), true);
            master->raise();
            emit sessionChanged();
        }
        return; // navigateMaster already focused/raised/notified
    }
    setFocused(master->instanceId(), true);
    master->raise();
    emit sessionChanged();
}

QString LayoutInstanceManager::openInstance(const QString& layoutId, QString* error)
{
    const LayoutDocument* doc = requireDoc(layoutId, error);
    if (!doc) {
        return {};
    }
    if (doc->isMasterShell()) {
        if (!navigateMaster(layoutId, error)) {
            return {};
        }
        return m_masterId;
    }

    // Opening a board from Main: optionally collapse master to the dock chip first
    // so the secondary board ends up focused/on top (collapse does not raise master).
    if (m_autoCollapseMain) {
        if (auto* master = masterInstance()) {
            const auto& sess = master->document().session;
            if (sess.isHome && !sess.collapseLayoutId.isEmpty()
                && master->layoutId() != sess.collapseLayoutId) {
                QString collapseErr;
                if (!applyDocument(master, sess.collapseLayoutId, &collapseErr)) {
                    GAZER_WARN << "auto-collapse before openSecondary:" << collapseErr;
                } else {
                    GAZER_INFO << "Auto-collapsed master →" << sess.collapseLayoutId
                               << "before opening" << layoutId;
                }
            }
        }
    }

    return openSecondary(layoutId, error);
}

void LayoutInstanceManager::applyGlobalDwellOverride(const QVector<int>& dwellSequence,
                                                     int graceMs)
{
    m_globalDwellSequence = dwellSequence;
    m_globalGraceMs = graceMs;
    for (const auto& p : m_instances) {
        if (p) {
            p->setGlobalDwellOverride(dwellSequence, graceMs);
        }
    }
}

void LayoutInstanceManager::applyProgressVisuals(const ProgressVisuals& visuals)
{
    m_progressVisuals = visuals;
    for (const auto& p : m_instances) {
        if (p) {
            p->setProgressVisuals(visuals);
        }
    }
}

void LayoutInstanceManager::applyTheme(const ThemeColors& theme)
{
    m_theme = theme;
    for (const auto& p : m_instances) {
        if (p) {
            p->setTheme(theme);
        }
    }
}

void LayoutInstanceManager::refreshActiveIndicators(const ActiveStateResolver& resolver)
{
    if (!resolver) {
        return;
    }
    for (const auto& p : m_instances) {
        if (!p) {
            continue;
        }
        QSet<QString> activeIds;
        for (const LayoutItem& item : p->document().items) {
            if (item.activeState.isEmpty()) {
                continue;
            }
            if (resolver(item.activeState)) {
                activeIds.insert(item.id);
            }
        }
        p->setActiveItemIds(activeIds);
    }
}

void LayoutInstanceManager::setEdgeBubbleOverlay(EdgeBubbleOverlay* overlay)
{
    m_edgeBubbles = overlay;
    for (const auto& p : m_instances) {
        if (p) {
            p->setEdgeBubbleOverlay(overlay);
        }
    }
}

void LayoutInstanceManager::setDocumentDecorator(DocumentDecorator decorator)
{
    m_documentDecorator = std::move(decorator);
}

LayoutDocument LayoutInstanceManager::decorateCopy(const LayoutDocument& src) const
{
    LayoutDocument doc = src;
    if (m_documentDecorator) {
        m_documentDecorator(doc);
    }
    return doc;
}

bool LayoutInstanceManager::setInstanceDocument(const QString& instanceId, LayoutDocument doc,
                                                QString* error)
{
    auto* inst = instance(instanceId);
    if (!inst) {
        if (error) {
            *error = QStringLiteral("Unknown instance: %1").arg(instanceId);
        }
        return false;
    }
    replaceInstanceDocument(inst, std::move(doc), /*fireOnLoad=*/true);
    setFocused(instanceId, true);
    emit sessionChanged();
    return true;
}

int LayoutInstanceManager::closeOtherViews()
{
    QVector<QString> toClose;
    for (const auto& p : m_instances) {
        if (p && p->instanceId() != m_masterId) {
            toClose.push_back(p->instanceId());
        }
    }

    if (m_gazeInstanceId != m_masterId) {
        m_gazeInstanceId.clear();
    }

    int n = 0;
    for (const QString& id : toClose) {
        QString err;
        if (eraseSecondary(id, &err)) {
            ++n;
        }
    }

    if (auto* master = masterInstance()) {
        const QString homeId = homeLayoutIdForMaster();
        if (!homeId.isEmpty() && master->layoutId() != homeId
            && !master->document().session.isHome) {
            QString err;
            (void)applyDocument(master, homeId, &err);
        }
        setFocused(master->instanceId(), true);
        master->raise();
    }

    emit sessionChanged();
    GAZER_INFO << "closeOtherViews closed" << n;
    return n;
}

bool LayoutInstanceManager::closeInstance(const QString& instanceId, QString* error)
{
    if (m_instances.size() <= 1) {
        if (error) {
            *error = QStringLiteral("Cannot close the last layout instance");
        }
        return false;
    }

    if (instanceId == m_masterId) {
        if (error) {
            *error = QStringLiteral("Cannot close master while other boards are open");
        }
        setFocused(m_masterId, true);
        emit sessionChanged();
        return false;
    }

    if (!eraseSecondary(instanceId, error)) {
        return false;
    }

    if (m_focusedId == m_masterId || m_focusedId.isEmpty()) {
        if (auto* master = masterInstance()) {
            setFocused(master->instanceId(), true);
            master->raise();
        } else if (!m_instances.empty()) {
            setFocused(m_instances.back()->instanceId(), true);
        }
    }

    emit sessionChanged();
    GAZER_INFO << "Closed layout instance" << instanceId;
    return true;
}

void LayoutInstanceManager::focusInstance(const QString& instanceId)
{
    if (!instance(instanceId)) {
        return;
    }
    leaveActiveGaze();
    setFocused(instanceId, true);
    if (auto* inst = instance(instanceId)) {
        inst->leaveGaze();
    }
    emit sessionChanged();
}

LayoutInstance* LayoutInstanceManager::findInstanceAt(const QPointF& screenPoint) const
{
    // Topmost first: m_instances is ordered bottom→top; rbegin is visual/hit top.
    // Only the first containing board wins — overlapping boards underneath get nothing.
    for (auto it = m_instances.rbegin(); it != m_instances.rend(); ++it) {
        if (*it && (*it)->containsScreenPoint(screenPoint)) {
            return it->get();
        }
    }
    return nullptr;
}

void LayoutInstanceManager::reassertStackTopVisual()
{
    if (m_instances.empty() || !m_instances.back()) {
        return;
    }
    m_instances.back()->raise();
}

bool LayoutInstanceManager::onGaze(const GazePoint& point)
{
    // Invalid samples: keep last board dwell alive briefly (blinks). Instance
    // dwell machines also apply their own invalid grace.
    if (!point.valid) {
        if (!m_gazeInstanceId.isEmpty()) {
            if (auto* cur = instance(m_gazeInstanceId)) {
                // Feed invalid so DwellStateMachine grace can run; do not leave yet.
                GazePoint invalid = point;
                invalid.valid = false;
                cur->feedGaze(invalid, {});
            }
        }
        return !m_gazeInstanceId.isEmpty();
    }

    LayoutInstance* hit = findInstanceAt(point.toPointF());
    const QString hitId = hit ? hit->instanceId() : QString();

    if (hitId != m_gazeInstanceId) {
        if (auto* prev = instance(m_gazeInstanceId)) {
            prev->leaveGaze();
        }
        m_gazeInstanceId = hitId;
        if (hit) {
            // Focus id only — do not reorder stack (would desync from HWND z-order).
            setFocused(hitId, false);
        }
    }

    // Strict topmost: only the hit board receives dwell/hover. Others stay idle.
    if (hit) {
        hit->feedGaze(point, hit->hitTest(point.toPointF()));
        return true;
    }
    return false;
}

void LayoutInstanceManager::leaveActiveGaze()
{
    if (m_gazeInstanceId.isEmpty()) {
        return;
    }
    if (auto* prev = instance(m_gazeInstanceId)) {
        prev->leaveGaze();
    }
    m_gazeInstanceId.clear();
}

void LayoutInstanceManager::onWindowCloseRequested(const QString& instanceId)
{
    if (instanceId == m_masterId && m_instances.size() > 1) {
        auto* master = masterInstance();
        const QString collapseId =
            master ? master->document().session.collapseLayoutId : QString();
        if (!collapseId.isEmpty()) {
            QString err;
            if (!navigateMaster(collapseId, &err)) {
                GAZER_WARN << "collapse master:" << err;
            }
        } else {
            setFocused(m_masterId, true);
            emit sessionChanged();
        }
        return;
    }

    if (m_instances.size() > 1) {
        QString err;
        if (!closeInstance(instanceId, &err)) {
            GAZER_WARN << "closeInstance:" << err;
        }
        return;
    }

    if (auto* inst = instance(instanceId)) {
        inst->hide();
        GAZER_INFO << "Last layout board hidden (app stays in tray)";
    }
}

void LayoutInstanceManager::hideAll()
{
    for (const auto& p : m_instances) {
        if (p) {
            p->hide();
        }
    }
    m_gazeInstanceId.clear();
}

void LayoutInstanceManager::shutdown()
{
    m_gazeInstanceId.clear();
    m_focusedId.clear();
    m_masterId.clear();
    m_masterGroup.clear();
    m_instances.clear();
    emit sessionChanged();
}

} // namespace gazer
