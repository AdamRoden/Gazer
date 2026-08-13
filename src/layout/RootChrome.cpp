#include "layout/LayoutInstanceManager.h"

#include "utils/Log.h"

namespace gazer {

LayoutInstance* LayoutInstanceManager::homeInstance() const
{
    return instance(m_childInstanceBySlot.value(QStringLiteral("home")));
}

LayoutInstance* LayoutInstanceManager::quitInstance() const
{
    return instance(m_childInstanceBySlot.value(QStringLiteral("quit")));
}

void LayoutInstanceManager::applyChromeProps()
{
    const bool expanded = (m_rootChrome != RootChrome::Docked) || m_homeDismissing;
    m_rootProps.insert(QStringLiteral("expanded"), expanded);
    m_rootProps.insert(QStringLiteral("quitConfirm"), m_rootChrome == RootChrome::Quit);
    m_rootProps.insert(QStringLiteral("dwellSuspend"), m_dwellSuspended);
    for (const auto& p : m_instances) {
        if (p) {
            p->setPropertyContext(m_rootProps);
        }
    }
}

void LayoutInstanceManager::pushPropertyContext()
{
    applyChromeProps();
}

void LayoutInstanceManager::setRootProperty(const QString& key, const QVariant& value)
{
    if (key == QLatin1String("expanded")) {
        setRootChrome(value.toBool() ? RootChrome::Drawer : RootChrome::Docked);
        return;
    }
    if (key == QLatin1String("quitConfirm")) {
        setRootChrome(value.toBool() ? RootChrome::Quit : RootChrome::Drawer);
        return;
    }
    if (m_rootProps.value(key) == value) {
        return;
    }
    m_rootProps.insert(key, value);
    applyChromeProps();
    emit sessionChanged();
}

void LayoutInstanceManager::restackChrome()
{
    for (const auto& p : m_instances) {
        if (p && p->window() && p->window()->isVisible()
            && p->document().placement.aboveTaskbar) {
            p->window()->keepAboveTaskbar();
        }
    }
    // Master chips / sleep progress live on the edge overlay — always above the drawer.
    if (m_edgeBubbles && m_edgeBubbles->isVisible()) {
        m_edgeBubbles->raiseStack();
    }
}

void LayoutInstanceManager::setRootChrome(RootChrome next)
{
    auto* home = homeInstance();
    auto* quit = quitInstance();

    if (next == m_rootChrome) {
        if (next == RootChrome::Docked && !m_homeDismissing) {
            return;
        }
        if (next == RootChrome::Drawer && home && home->window() && home->window()->isVisible()
            && !home->isDismissing()) {
            restackChrome();
            return;
        }
        if (next == RootChrome::Quit && quit && quit->window() && quit->window()->isVisible()) {
            restackChrome();
            return;
        }
    }

    m_rootChrome = next;

    if (next == RootChrome::Drawer) {
        m_homeDismissing = false;
        if (quit) {
            quit->forceHide();
        }
        applyChromeProps();
        if (home) {
            if (home->isDismissing() || !home->window() || !home->window()->isVisible()) {
                home->playAppear();
            } else {
                home->raise();
            }
            home->resetAutoCloseClock(nowMs());
            setFocused(home->instanceId(), true);
        }
        restackChrome();
        emit sessionChanged();
        return;
    }

    if (next == RootChrome::Quit) {
        m_homeDismissing = false;
        if (home) {
            home->forceHide();
        }
        applyChromeProps();
        if (quit) {
            quit->raise();
            setFocused(quit->instanceId(), true);
        }
        restackChrome();
        emit sessionChanged();
        return;
    }

    if (quit) {
        quit->forceHide();
    }
    if (home && home->usesDrawerMotion() && home->window() && home->window()->isVisible()) {
        if (home->isDismissing()) {
            return;
        }
        m_homeDismissing = true;
        applyChromeProps();
        const QString homeId = home->instanceId();
        home->playDismiss([this, homeId]() {
            if (auto* h = instance(homeId)) {
                h->forceHide();
            }
            m_homeDismissing = false;
            applyChromeProps();
            if (auto* master = masterInstance()) {
                setFocused(master->instanceId(), false);
            }
            restackChrome();
            emit sessionChanged();
        });
        return;
    }
    if (home) {
        home->forceHide();
    }
    m_homeDismissing = false;
    applyChromeProps();
    if (auto* master = masterInstance()) {
        setFocused(master->instanceId(), false);
    }
    restackChrome();
    emit sessionChanged();
}

QString LayoutInstanceManager::homeLayoutIdForMaster() const
{
    auto* master = masterInstance();
    if (!master) {
        return {};
    }
    for (const LayoutChildRef& c : master->document().children) {
        if (c.id == QLatin1String("home")) {
            return c.layoutId;
        }
    }
    return {};
}

bool LayoutInstanceManager::isDeclaredChildLayout(const QString& layoutId) const
{
    return m_childInstanceByLayout.contains(layoutId);
}

bool LayoutInstanceManager::isChildInstance(const QString& instanceId) const
{
    for (auto it = m_childInstanceBySlot.cbegin(); it != m_childInstanceBySlot.cend(); ++it) {
        if (it.value() == instanceId) {
            return true;
        }
    }
    return false;
}

LayoutChildRef LayoutInstanceManager::childSpecForLayout(const QString& layoutId) const
{
    auto* root = masterInstance();
    if (!root) {
        return {};
    }
    for (const LayoutChildRef& c : root->document().children) {
        if (c.layoutId == layoutId || c.id == layoutId) {
            return c;
        }
    }
    return {};
}

void LayoutInstanceManager::syncChildVisibility()
{
    auto* home = homeInstance();
    auto* quit = quitInstance();
    const bool showHome = m_rootChrome == RootChrome::Drawer || m_homeDismissing;
    const bool showQuit = m_rootChrome == RootChrome::Quit;
    if (home) {
        if (showHome) {
            if (!home->isDismissing()) {
                home->raise();
            }
        } else if (!home->isDismissing()) {
            home->forceHide();
        }
    }
    if (quit) {
        if (showQuit) {
            quit->raise();
        } else {
            quit->forceHide();
        }
    }
}

QString LayoutInstanceManager::spawnChild(const LayoutChildRef& child, QString* error)
{
    if (m_childInstanceBySlot.contains(child.id)) {
        return m_childInstanceBySlot.value(child.id);
    }
    const LayoutDocument* doc = requireDoc(child.layoutId, error);
    if (!doc) {
        return {};
    }
    if (doc->isMaster()) {
        if (error) {
            *error = QStringLiteral("Cannot nest a master layout as a child: %1")
                         .arg(child.layoutId);
        }
        return {};
    }
    const QString id = makeInstanceId(child.layoutId);
    auto inst = std::make_unique<LayoutInstance>(id, decorateCopy(*doc));
    if (!m_globalDwellSequence.isEmpty() || m_globalGraceMs > 0 || m_globalScanGraceMs >= 0) {
        inst->setGlobalDwellOverride(m_globalDwellSequence, m_globalGraceMs, m_globalScanGraceMs);
    }
    inst->setProgressVisuals(m_progressVisuals);
    inst->applyPlacement(0);
    inst->hide();
    wireInstance(inst.get());
    const QVector<LayoutAction> onOpen = inst->document().onOpen;
    const QVector<LayoutAction> onLoad = inst->document().onLoad;
    m_instances.push_back(std::move(inst));
    m_childInstanceBySlot.insert(child.id, id);
    m_childInstanceByLayout.insert(child.layoutId, id);
    fireLifecycle(onOpen, id);
    fireLifecycle(onLoad, id);
    emit instanceOpened(id, child.layoutId);
    GAZER_INFO << "Spawned child" << child.id << id << child.layoutId;
    return id;
}

bool LayoutInstanceManager::showChildLayout(const QString& layoutId, QString* error)
{
    const LayoutChildRef spec = childSpecForLayout(layoutId);
    if (spec.layoutId.isEmpty() && spec.id.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Not a declared child layout: %1").arg(layoutId);
        }
        return false;
    }
    if (m_childInstanceBySlot.value(spec.id).isEmpty()) {
        if (spawnChild(spec, error).isEmpty()) {
            return false;
        }
    }
    if (spec.id == QLatin1String("quit")) {
        setRootChrome(RootChrome::Quit);
        return true;
    }
    if (spec.id == QLatin1String("home")) {
        setRootChrome(RootChrome::Drawer);
        return true;
    }
    if (error) {
        *error = QStringLiteral("Unknown root chrome slot: %1").arg(spec.id);
    }
    return false;
}

bool LayoutInstanceManager::expandHome(QString* error)
{
    const QString homeId = homeLayoutIdForMaster();
    if (homeId.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Root has no home child");
        }
        return false;
    }
    return showChildLayout(homeId, error);
}

void LayoutInstanceManager::collapseHome()
{
    setRootChrome(RootChrome::Docked);
}

void LayoutInstanceManager::raiseMaster()
{
    QString err;
    if (!expandHome(&err)) {
        GAZER_WARN << "raiseMaster expand failed:" << err;
    }
}

} // namespace gazer
