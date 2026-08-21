#include "layout/LayoutInstanceManager.h"

#include "layout/LayoutSchema.h"
#include "ui/OverlaySurface.h"
#include "utils/Log.h"

#include <QElapsedTimer>
#include <QSet>
#include <algorithm>

namespace gazer {

namespace {
QElapsedTimer g_autoCloseClock;
bool g_autoCloseClockStarted = false;

qint64 autoCloseNowMs()
{
    if (!g_autoCloseClockStarted) {
        g_autoCloseClock.start();
        g_autoCloseClockStarted = true;
    }
    return g_autoCloseClock.elapsed();
}
} // namespace

qint64 LayoutInstanceManager::nowMs() const
{
    return autoCloseNowMs();
}

LayoutInstanceManager::LayoutInstanceManager(LayoutManager& catalog, QObject* parent)
    : QObject(parent)
    , m_catalog(catalog)
{
}

QString LayoutInstanceManager::makeInstanceId(const QString& layoutId)
{
    return QStringLiteral("%1#%2").arg(layoutId).arg(m_nextSerial++);
}

void LayoutInstanceManager::applyInstanceChrome(LayoutInstance* inst)
{
    if (!inst) {
        return;
    }
    inst->setTheme(m_theme);
    inst->setProgressVisuals(m_progressVisuals);
    if (!m_globalDwellSequence.isEmpty() || m_globalGraceMs > 0 || m_globalScanGraceMs >= 0) {
        inst->setGlobalDwellOverride(m_globalDwellSequence, m_globalGraceMs, m_globalScanGraceMs);
    }
    inst->setPropertyContext(m_rootProps);
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
    applyInstanceChrome(inst);
    applyAutoClosePolicy(inst);
    connect(inst, &LayoutInstance::itemActivated, this, &LayoutInstanceManager::itemActivated);
    connect(inst, &LayoutInstance::windowCloseRequested, this,
            &LayoutInstanceManager::onWindowCloseRequested);
    connect(inst, &LayoutInstance::dwellEngagementEnded, this,
            &LayoutInstanceManager::dwellEngagementEnded);
    connect(inst, &LayoutInstance::dwellActivity, this,
            [this](const QString& instanceId) { noteDwellActivity(instanceId); });
}

std::unique_ptr<LayoutInstance> LayoutInstanceManager::makeWiredInstance(const QString& layoutId,
                                                                        const LayoutDocument& doc)
{
    auto inst = std::make_unique<LayoutInstance>(makeInstanceId(layoutId), decorateCopy(doc));
    inst->applyPlacement();
    wireInstance(inst.get());
    return inst;
}

bool LayoutInstanceManager::isRootChildLayout(const QString& layoutId) const
{
    return isDeclaredChildLayout(layoutId) || !childSpecForLayout(layoutId).layoutId.isEmpty();
}

void LayoutInstanceManager::applyAutoClosePolicy(LayoutInstance* inst)
{
    if (!inst) {
        return;
    }
    const auto& doc = inst->document();
    const bool on = doc.autoClose && m_autoCloseEnabled && inst->instanceId() != m_masterId
                    && !doc.isMaster();
    inst->setAutoCloseEnabled(on);
    if (on) {
        const int idle = doc.autoCloseIdleMs > 0 ? doc.autoCloseIdleMs : m_autoCloseIdleMs;
        const int fade = doc.autoCloseFadeMs > 0 ? doc.autoCloseFadeMs : m_autoCloseFadeMs;
        inst->setAutoCloseTiming(idle, fade);
    }
    inst->resetAutoCloseClock(autoCloseNowMs());
}

void LayoutInstanceManager::setAutoCloseDefaults(bool enabled, int idleMs, int fadeMs)
{
    m_autoCloseEnabled = enabled;
    m_autoCloseIdleMs = qMax(500, idleMs);
    m_autoCloseFadeMs = qMax(50, fadeMs);
    for (const auto& p : m_instances) {
        applyAutoClosePolicy(p.get());
    }
}

void LayoutInstanceManager::noteDwellActivity(const QString& instanceId)
{
    if (auto* inst = instance(instanceId)) {
        inst->resetAutoCloseClock(autoCloseNowMs());
    }
}

void LayoutInstanceManager::tickAutoClose(qint64 t)
{
    if (!m_autoCloseEnabled) {
        return;
    }
    QVector<QString> toClose;
    bool collapseMaster = false;
    for (const auto& p : m_instances) {
        if (!p || !p->autoCloseEnabled()) {
            continue;
        }
        // Hidden children (home/quit) must not keep firing collapse.
        if (!p->window() || !p->window()->isVisible()) {
            continue;
        }
        if (p->usesDrawerMotion()) {
            // Gaze on the drawer *or* the dock chips is still using the shell.
            if (m_gazeInstanceId == p->instanceId() || m_gazeInstanceId == m_masterId) {
                p->resetAutoCloseClock(t);
                continue;
            }
            if (p->isScaleAnimating()) {
                continue;
            }
            p->applyAutoCloseVisuals(t);
            if (p->autoCloseReadyToDismiss(t)) {
                collapseMaster = true;
            }
            continue;
        }
        // Dwelling on this board keeps it alive (also cancels mid-dismiss).
        if (m_gazeInstanceId == p->instanceId()) {
            p->resetAutoCloseClock(t);
            continue;
        }
        p->applyAutoCloseVisuals(t);
        if (!p->autoCloseFinished(t)) {
            continue;
        }
        if (p->instanceId() == m_masterId) {
            continue;
        } else if (isChildInstance(p->instanceId())) {
            collapseMaster = true;
        } else {
            toClose.push_back(p->instanceId());
        }
    }
    for (const QString& id : toClose) {
        QString err;
        if (closeInstance(id, &err)) {
            GAZER_INFO << "Auto-closed layout instance" << id;
        }
    }
    if (collapseMaster) {
        const auto* home = homeInstance();
        const bool already =
            m_homeDismissing || m_rootChrome == RootChrome::Docked
            || (home && home->isDismissing());
        if (!already) {
            collapseHome();
            GAZER_INFO << "Auto-collapsed home drawer";
        }
    }
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
    m_rootProps.insert(QStringLiteral("dwellSuspend"), suspended);
    for (const auto& p : m_instances) {
        if (p) {
            p->setDwellSuspended(suspended);
            p->setPropertyContext(m_rootProps);
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
    restackChrome();
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

int LayoutInstanceManager::visibleBoardCount() const
{
    int n = 0;
    for (const auto& p : m_instances) {
        if (!p || !p->window() || !p->window()->isVisible()) {
            continue;
        }
        if (p->instanceId() == m_masterId && !p->document().showsBoardWindow()) {
            continue;
        }
        ++n;
    }
    if (n == 0 && masterInstance()) {
        return 1; // dock root is still the live session
    }
    return n;
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
    applyInstanceChrome(inst);
    applyAutoClosePolicy(inst);
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
    if (isChildInstance(instanceId)) {
        if (error) {
            *error = QStringLiteral("Cannot erase a declared child instance");
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

    if (instanceId == m_editorPreviewId) {
        m_editorPreviewId.clear();
        for (const auto& p : m_instances) {
            if (p && p->instanceId() != instanceId) {
                p->setDwellSuspended(m_dwellSuspended);
            }
        }
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

    const bool sourceIsRoot = sourceInstanceId == m_masterId;
    const bool sourceIsChild = isChildInstance(sourceInstanceId);
    auto* root = masterInstance();
    const bool targetIsRoot = root && (layoutId == root->layoutId() || target->isMaster());

    if (targetIsRoot) {
        if (!sourceIsRoot && !sourceIsChild) {
            QString err;
            (void)eraseSecondary(sourceInstanceId, &err);
        }
        collapseHome();
        return true;
    }

    if (isRootChildLayout(layoutId)) {
        if (!sourceIsRoot && !sourceIsChild) {
            QString err;
            (void)eraseSecondary(sourceInstanceId, &err);
        }
        if (!showChildLayout(layoutId, error)) {
            return false;
        }
        emit sessionChanged();
        return true;
    }

    if (sourceIsRoot || sourceIsChild) {
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
        if (error) {
            *error = QStringLiteral("Cannot replace the root master document");
        }
        return false;
    }
    if (doc->isMaster()) {
        if (error) {
            *error = QStringLiteral("Cannot load master layout into a secondary instance");
        }
        return false;
    }

    replaceInstanceDocument(inst, decorateCopy(*doc), /*fireOnLoad=*/true);
    setFocused(instanceId, true);
    emit sessionChanged();
    return true;
}

bool LayoutInstanceManager::openMaster(const QString& layoutId, QString* error)
{
    const LayoutDocument* doc = requireDoc(layoutId, error);
    if (!doc) {
        return false;
    }
    if (auto* existing = masterInstance()) {
        if (existing->layoutId() == layoutId) {
            return true;
        }
        if (error) {
            *error = QStringLiteral("Root master already open as %1").arg(existing->layoutId());
        }
        return false;
    }

    auto inst = makeWiredInstance(layoutId, *doc);
    const QString id = inst->instanceId();
    const QVector<LayoutAction> onOpen = inst->document().onOpen;
    const QVector<LayoutAction> onLoad = inst->document().onLoad;
    const QVector<LayoutChildRef> children = inst->document().children;
    m_masterId = id;
    m_instances.insert(m_instances.begin(), std::move(inst));
    setFocused(id, true);
    fireLifecycle(onOpen, id);
    fireLifecycle(onLoad, id);
    GAZER_INFO << "Opened root master" << id << layoutId;

    for (const LayoutChildRef& child : children) {
        QString childErr;
        if (spawnChild(child, &childErr).isEmpty()) {
            GAZER_WARN << "Failed to spawn child" << child.layoutId << childErr;
        }
    }
    applyChromeProps();
    syncChildVisibility();
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
    if (doc->isMaster()) {
        if (error) {
            *error = QStringLiteral("openSecondary refuses master; use openMaster");
        }
        return {};
    }
    if (isRootChildLayout(layoutId)) {
        if (!showChildLayout(layoutId, error)) {
            return {};
        }
        emit sessionChanged();
        return m_childInstanceByLayout.value(layoutId);
    }

    auto inst = makeWiredInstance(layoutId, *doc);
    const QString id = inst->instanceId();
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

QString LayoutInstanceManager::openEditorPreview(const QVector<LayoutDocument>& family,
                                                 int currentIndex, QString* error)
{
    if (family.isEmpty() || currentIndex < 0 || currentIndex >= family.size()) {
        if (error) {
            *error = QStringLiteral("Nothing to preview");
        }
        return {};
    }

    QHash<QString, QString> map;
    for (const LayoutDocument& src : family) {
        if (!src.id.isEmpty()) {
            map.insert(src.id, LayoutSchema::editorPreviewId(src.id));
        }
    }
    auto remapActs = [&](QVector<LayoutAction>& acts) {
        for (LayoutAction& a : acts) {
            const auto it = map.constFind(a.layoutId);
            if (it != map.cend()) {
                a.layoutId = it.value();
            }
        }
    };
    LayoutDocument current;
    for (int i = 0; i < family.size(); ++i) {
        LayoutDocument d = family[i];
        d.id = map.value(d.id, LayoutSchema::editorPreviewId(d.id));
        remapActs(d.onOpen);
        remapActs(d.onLoad);
        remapActs(d.onClose);
        for (LayoutItem& item : d.items) {
            remapActs(item.actions);
            const auto it = map.constFind(item.action.layoutId);
            if (it != map.cend()) {
                item.action.layoutId = it.value();
            }
            const auto embed = map.constFind(item.embedLayoutId);
            if (embed != map.cend()) {
                item.embedLayoutId = embed.value();
            }
        }
        m_catalog.putDocument(d);
        if (i == currentIndex) {
            current = std::move(d);
        }
    }
    return openEditorPreview(std::move(current), error);
}

QString LayoutInstanceManager::openEditorPreview(LayoutDocument doc, QString* error)
{
    if (doc.master) {
        if (error) {
            *error = QStringLiteral("Cannot live-test the master root from the editor");
        }
        return {};
    }

    const QString previewLayoutId = doc.id.startsWith(QLatin1String("__editor_preview"))
                                        ? doc.id
                                        : QStringLiteral("__editor_preview");
    doc.master = false;
    doc.hideUntilGazeReveal = false;
    doc.children.clear();
    doc.id = previewLayoutId;
    if (!doc.name.contains(QLatin1String("(preview)"))) {
        doc.name = doc.name.isEmpty() ? QStringLiteral("Preview")
                                      : doc.name + QStringLiteral(" (preview)");
    }

    auto suspendOthers = [this](const QString& keepId) {
        for (const auto& p : m_instances) {
            if (p) {
                p->setDwellSuspended(p->instanceId() != keepId);
            }
        }
    };

    if (!m_editorPreviewId.isEmpty() && instance(m_editorPreviewId)) {
        if (!setInstanceDocument(m_editorPreviewId, std::move(doc), error)) {
            return {};
        }
        suspendOthers(m_editorPreviewId);
        return m_editorPreviewId;
    }
    m_editorPreviewId.clear();

    auto inst = makeWiredInstance(previewLayoutId, doc);
    const QString id = inst->instanceId();
    const QVector<LayoutAction> onOpen = inst->document().onOpen;
    const QVector<LayoutAction> onLoad = inst->document().onLoad;
    m_instances.push_back(std::move(inst));
    m_editorPreviewId = id;
    setFocused(id, true);
    suspendOthers(id);
    fireLifecycle(onOpen, id);
    fireLifecycle(onLoad, id);
    GAZER_INFO << "Opened editor preview" << id;
    emit instanceOpened(id, previewLayoutId);
    emit sessionChanged();
    return id;
}

QString LayoutInstanceManager::openInstance(const QString& layoutId, QString* error)
{
    const LayoutDocument* doc = requireDoc(layoutId, error);
    if (!doc) {
        return {};
    }
    if (doc->isMaster()) {
        if (!openMaster(layoutId, error)) {
            return {};
        }
        return m_masterId;
    }

    if (isRootChildLayout(layoutId)) {
        if (!showChildLayout(layoutId, error)) {
            return {};
        }
        emit sessionChanged();
        return m_childInstanceByLayout.value(layoutId);
    }

    if (m_autoCollapseMain && m_rootChrome == RootChrome::Drawer) {
        setRootChrome(RootChrome::Docked);
        GAZER_INFO << "Auto-collapsed home before opening" << layoutId;
    }

    return openSecondary(layoutId, error);
}

void LayoutInstanceManager::applyGlobalDwellOverride(const QVector<int>& dwellSequence,
                                                     int graceMs, int scanGraceMs)
{
    m_globalDwellSequence = dwellSequence;
    m_globalGraceMs = graceMs;
    m_globalScanGraceMs = scanGraceMs;
    for (const auto& p : m_instances) {
        if (p) {
            p->setGlobalDwellOverride(dwellSequence, graceMs, scanGraceMs);
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
    if (m_edgeBubbles) {
        disconnect(m_edgeBubbles, &OverlaySurface::stackChanged, this,
                   &LayoutInstanceManager::restackChrome);
    }
    m_edgeBubbles = overlay;
    if (m_edgeBubbles) {
        connect(m_edgeBubbles, &OverlaySurface::stackChanged, this,
                &LayoutInstanceManager::restackChrome);
    }
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

QVariant LayoutInstanceManager::rootProperty(const QString& key) const
{
    return m_rootProps.value(key);
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
        if (p && p->instanceId() != m_masterId && !isChildInstance(p->instanceId())) {
            toClose.push_back(p->instanceId());
        }
    }

    if (m_gazeInstanceId != m_masterId && !isChildInstance(m_gazeInstanceId)) {
        m_gazeInstanceId.clear();
    }

    int n = 0;
    for (const QString& id : toClose) {
        QString err;
        if (eraseSecondary(id, &err)) {
            ++n;
        }
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
            *error = QStringLiteral("Cannot close the root master");
        }
        setFocused(m_masterId, true);
        emit sessionChanged();
        return false;
    }
    if (isChildInstance(instanceId)) {
        auto* inst = instance(instanceId);
        const QString layoutId = inst ? inst->layoutId() : QString();
        const LayoutChildRef spec = childSpecForLayout(layoutId);
        if (spec.id == QLatin1String("home")) {
            setRootChrome(RootChrome::Docked);
        } else if (spec.id == QLatin1String("quit")) {
            setRootChrome(RootChrome::Drawer);
        } else if (inst) {
            inst->forceHide();
            emit sessionChanged();
        }
        return true;
    }

    if (!eraseSecondary(instanceId, error)) {
        return false;
    }

    if (m_focusedId == m_masterId || m_focusedId.isEmpty()) {
        if (m_rootChrome == RootChrome::Drawer && homeInstance()) {
            setFocused(homeInstance()->instanceId(), true);
        } else if (m_rootChrome == RootChrome::Quit && quitInstance()) {
            setFocused(quitInstance()->instanceId(), true);
        } else if (auto* master = masterInstance()) {
            setFocused(master->instanceId(), false);
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
    // Root dock chips (Sleep / Main) always beat the drawer and other boards.
    if (auto* master = masterInstance()) {
        if (master->containsVisibleUnboundedProgress(screenPoint)
            || master->containsScreenPoint(screenPoint)) {
            return master;
        }
    }
    // Visible unbounded progress chrome wins over other boards' windows
    // (even if those windows are higher in the HWND / stack z-order).
    for (auto it = m_instances.rbegin(); it != m_instances.rend(); ++it) {
        if (*it && (*it)->instanceId() == m_masterId) {
            continue;
        }
        if (*it && (*it)->containsVisibleUnboundedProgress(screenPoint)) {
            return it->get();
        }
    }
    for (auto it = m_instances.rbegin(); it != m_instances.rend(); ++it) {
        if (*it && (*it)->instanceId() == m_masterId) {
            continue;
        }
        if (*it && (*it)->containsScreenPoint(screenPoint)) {
            return it->get();
        }
    }
    return nullptr;
}

void LayoutInstanceManager::reassertStackTopVisual()
{
    restackChrome();
    if (m_instances.empty() || !m_instances.back()) {
        return;
    }
    LayoutInstance* top = m_instances.back().get();
    if (!top->window() || !top->window()->isVisible()) {
        return;
    }
    // Do not call raise() on drawer / above-taskbar — restackChrome already
    // used keepAboveTaskbar. Regular secondaries still need a visual raise.
    if (top->document().placement.aboveTaskbar || top->usesDrawerMotion()) {
        return;
    }
    top->window()->showAndRaise();
}

bool LayoutInstanceManager::onGaze(const GazePoint& point)
{
    tickAutoClose(autoCloseNowMs());

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

    // Topmost first (stack end = drawn on top). Only that board gets dwell.
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
            noteDwellActivity(hitId);
        }
    }

    // Strict topmost: only the hit board receives dwell/hover. Others stay idle.
    if (hit) {
        const QString itemId = hit->hitTest(point.toPointF());
        if (!itemId.isEmpty()) {
            noteDwellActivity(hit->instanceId());
        }
        hit->feedGaze(point, itemId);
        if (hit->instanceId() == m_masterId || hit->document().placement.aboveTaskbar) {
            restackChrome();
        }
        return true;
    }
    return false;
}

void LayoutInstanceManager::leaveActiveGaze()
{
    if (!m_gazeInstanceId.isEmpty()) {
        if (auto* prev = instance(m_gazeInstanceId)) {
            prev->leaveGaze();
        }
        m_gazeInstanceId.clear();
    }
    restackChrome();
}

void LayoutInstanceManager::onWindowCloseRequested(const QString& instanceId)
{
    if (instanceId == m_masterId) {
        return;
    }
    if (isChildInstance(instanceId)) {
        QString err;
        (void)closeInstance(instanceId, &err);
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
    m_childInstanceBySlot.clear();
    m_childInstanceByLayout.clear();
    m_rootProps.clear();
    m_rootChrome = RootChrome::Docked;
    m_homeDismissing = false;
    m_instances.clear();
    emit sessionChanged();
}

} // namespace gazer
