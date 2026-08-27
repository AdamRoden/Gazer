#include "layout/PageSession.h"

#include "layout/PageCatalog.h"
#include "layout/PageLoader.h"
#include "utils/Log.h"
#include "utils/ScreenGrab.h"

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QScreen>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QTransform>
#include <utility>

namespace gazer {

namespace {
constexpr auto kPreviewPrefix = "__editor_preview_";
} // namespace

QString PageSession::previewId(const QString& catalogId)
{
    if (catalogId.startsWith(QLatin1String(kPreviewPrefix))) {
        return catalogId;
    }
    return QLatin1String(kPreviewPrefix) + catalogId;
}

bool PageSession::isPreviewId(const QString& id)
{
    return id.startsWith(QLatin1String(kPreviewPrefix));
}

PageSession::PageSession(QObject* parent)
    : QObject(parent)
{
    m_props.insert(QStringLiteral("expanded"), false);
    m_props.insert(QStringLiteral("dwellSuspend"), false);
    m_autoCloseTimer.setInterval(200);
    connect(&m_autoCloseTimer, &QTimer::timeout, this, [this]() { tickAutoClose(); });
    m_drawerTimer.setInterval(16);
    connect(&m_drawerTimer, &QTimer::timeout, this, [this]() { tickDrawer(); });
    connect(&m_dwell, &DwellStateMachine::dwellProgress, this,
            [this](const QString& id, double p) {
                if (m_host) {
                    m_host->setHover(id, p, m_dwell.isScanGraceComplete());
                }
            });
    connect(&m_dwell, &DwellStateMachine::hoverChanged, this, [this](const QString& id) {
        m_hoverId = id;
        if (!m_host) {
            return;
        }
        if (id.isEmpty()) {
            m_host->setHover({}, 0.0, false);
        } else {
            m_host->setHover(id, m_dwell.progress(), m_dwell.isScanGraceComplete());
        }
    });
    connect(&m_dwell, &DwellStateMachine::itemActivated, this, [this](const QString& id) {
        activateTarget(id);
    });
    const auto onScreens = [this]() {
        if (hasRoot()) {
            rebuild();
        }
    };
    auto bindScreens = [this, onScreens]() {
        for (const QPointer<QScreen>& s : m_boundScreens) {
            if (s) {
                disconnect(s, nullptr, this, nullptr);
            }
        }
        m_boundScreens.clear();
        for (QScreen* s : QGuiApplication::screens()) {
            if (!s) {
                continue;
            }
            connect(s, &QScreen::geometryChanged, this, onScreens);
            connect(s, &QScreen::availableGeometryChanged, this, onScreens);
            connect(s, &QScreen::virtualGeometryChanged, this, onScreens);
            m_boundScreens.push_back(s);
        }
    };
    bindScreens();
    connect(qGuiApp, &QGuiApplication::screenAdded, this, [bindScreens, onScreens]() {
        bindScreens();
        onScreens();
    });
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, [bindScreens, onScreens]() {
        bindScreens();
        onScreens();
    });
    connect(qGuiApp, &QGuiApplication::primaryScreenChanged, this, [bindScreens, onScreens]() {
        bindScreens();
        onScreens();
    });
}

bool PageSession::openRoot(const QString& xmlPath, QString* error)
{
    PageDocument doc;
    if (!PageLoader::loadFromFile(xmlPath, doc, error)) {
        return false;
    }
    if (!doc.master && doc.id != QLatin1String("main")) {
        GAZER_WARN << "Root page is not marked master:" << doc.id;
    }
    m_root = std::move(doc);
    if (m_layoutsDir.isEmpty()) {
        m_layoutsDir = QFileInfo(xmlPath).absolutePath();
    }
    m_attached.clear();
    m_crumbs.clear();
    m_chrome = RootChrome::Docked;
    m_drawerPhase = DrawerPhase::Idle;
    m_drawerScale = 1.0;
    m_drawerTimer.stop();
    m_props.insert(QStringLiteral("expanded"), false);

    if (!m_host) {
        m_host = std::make_unique<PageHostWindow>();
        connect(m_host.get(), &PageHostWindow::targetClicked, this, [this](const QString& id) {
            activateTarget(id);
        });
        m_host->setShiftHeld(m_shiftHeld);
    }
    m_dwell.setEnabled(true);
    rebuild();
    m_host->showHost();
    GAZER_INFO << "Page root opened" << m_root.id << "from" << xmlPath;
    emit sessionChanged();
    return true;
}

void PageSession::setTheme(const ThemeColors& theme)
{
    if (m_host) {
        m_host->setTheme(theme);
    }
}

void PageSession::setProgressVisuals(const ProgressVisuals& visuals)
{
    if (m_host) {
        m_host->setProgressVisuals(visuals);
    }
}

void PageSession::setGlobalDwell(const QVector<int>& sequence, int graceMs, int scanGraceMs)
{
    m_globalSequence = sequence.isEmpty() ? QVector<int>{800} : sequence;
    m_globalGraceMs = qMax(0, graceMs);
    m_globalScanGraceMs = qMax(0, scanGraceMs);
}

void PageSession::setDwellSuspended(bool on)
{
    if (m_dwellSuspended == on) {
        return;
    }
    m_dwellSuspended = on;
    m_props.insert(QStringLiteral("dwellSuspend"), on);
    if (on) {
        leaveGaze();
    } else {
        noteActivity();
    }
    rebuild();
    emit dwellSuspendChanged(on);
    emit sessionChanged();
}

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

PageFrame PageSession::frame() const
{
    PageFrame f;
    f.screen = QRectF(overlayScreenGeometry());
    f.desktop = QRectF(overlayDesktopGeometry());
    if (f.screen.isEmpty()) {
        f.screen = f.desktop;
    }
    if (f.desktop.isEmpty()) {
        f.desktop = f.screen;
    }
    return f;
}

QString PageSession::xmlPathFor(const QString& id) const
{
    if (m_catalog) {
        const QString live = m_catalog->pathFor(id);
        if (!live.isEmpty()) {
            return live;
        }
    }
    if (m_layoutsDir.isEmpty()) {
        return {};
    }
    return QDir(m_layoutsDir).filePath(id + QStringLiteral(".xml"));
}

QString PageSession::topPageId() const
{
    if (!m_attached.isEmpty()) {
        return m_attached.last().doc.id;
    }
    return m_root.id;
}

bool PageSession::hasPage(const QString& id) const
{
    if (m_root.id == id) {
        return true;
    }
    for (const AttachedPage& a : m_attached) {
        if (a.doc.id == id) {
            return true;
        }
    }
    return false;
}

void PageSession::registerMemoryPage(PageDocument doc)
{
    if (!doc.isValid()) {
        return;
    }
    m_memory.insert(doc.id, std::move(doc));
}

void PageSession::closePreviewPages()
{
    for (int i = m_attached.size() - 1; i >= 0; --i) {
        if (isPreviewId(m_attached[i].doc.id)) {
            m_attached.removeAt(i);
        }
    }
    for (auto it = m_memory.begin(); it != m_memory.end();) {
        if (isPreviewId(it.key())) {
            it = m_memory.erase(it);
        } else {
            ++it;
        }
    }
    leaveGaze();
    rebuild();
    if (!m_leaveGatePage.isEmpty() && !hasPage(m_leaveGatePage)) {
        clearLeaveGate();
    }
    emit sessionChanged();
}

bool PageSession::attachDocument(PageDocument doc, QString* error, bool decorate)
{
    if (!doc.isValid()) {
        if (error) {
            *error = QStringLiteral("Page has no id");
        }
        return false;
    }
    if (decorate && m_decorate) {
        m_decorate(doc);
    }
    const QString id = doc.id;
    for (int i = 0; i < m_attached.size(); ++i) {
        if (m_attached[i].doc.id == id) {
            m_attached[i].doc = std::move(doc);
            if (i != m_attached.size() - 1) {
                m_attached.move(i, m_attached.size() - 1);
            }
            leaveGaze();
            rebuild();
            raise();
            armLeaveGate(id);
            emit sessionChanged();
            return true;
        }
    }
    AttachedPage att;
    att.doc = std::move(doc);
    m_attached.push_back(std::move(att));
    leaveGaze();
    rebuild();
    raise();
    armLeaveGate(id);
    emit sessionChanged();
    return true;
}

bool PageSession::openPage(const QString& id, QString* error)
{
    if (id == m_root.id) {
        raise();
        return true;
    }
    if (isPreviewId(id)) {
        for (int i = m_attached.size() - 1; i >= 0; --i) {
            if (isPreviewId(m_attached[i].doc.id) && m_attached[i].doc.id != id) {
                m_attached.removeAt(i);
            }
        }
    }
    for (int i = 0; i < m_attached.size(); ++i) {
        if (m_attached[i].doc.id == id) {
            if (i != m_attached.size() - 1) {
                m_attached.move(i, m_attached.size() - 1);
                leaveGaze();
                rebuild();
            }
            raise();
            emit sessionChanged();
            return true;
        }
    }
    if (m_memory.contains(id)) {
        PageDocument doc = m_memory.value(id);
        return attachDocument(std::move(doc), error, true);
    }
    const QString path = xmlPathFor(id);
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        if (error) {
            *error = QStringLiteral("No XML page %1").arg(id);
        }
        return false;
    }
    PageDocument doc;
    if (!PageLoader::loadFromFile(path, doc, error)) {
        return false;
    }
    if (doc.id != id) {
        if (error) {
            *error = QStringLiteral("Page id '%1' does not match file '%2'").arg(doc.id, id);
        }
        return false;
    }
    if (m_decorate) {
        m_decorate(doc);
    }
    AttachedPage att;
    att.doc = std::move(doc);
    m_attached.push_back(std::move(att));
    if (m_autoCollapseMain && m_chrome == RootChrome::Drawer) {
        setRootChrome(RootChrome::Docked, false);
    }
    rebuild();
    raise();
    armLeaveGate(id);
    emit sessionChanged();
    GAZER_INFO << "Attached page" << id;
    return true;
}

void PageSession::closePage(const QString& id)
{
    if (m_leaveGatePage == id) {
        clearLeaveGate();
    }
    for (int i = 0; i < m_attached.size(); ++i) {
        if (m_attached[i].doc.id == id) {
            m_attached.removeAt(i);
            if (m_loopStopPage) {
                m_loopStopPage(id);
            }
            rebuild();
            emit sessionChanged();
            return;
        }
    }
}

int PageSession::closeAttached()
{
    const int n = m_attached.size();
    if (n == 0) {
        return 0;
    }
    clearLeaveGate();
    if (m_loopStopPage) {
        for (const AttachedPage& a : m_attached) {
            m_loopStopPage(a.doc.id);
        }
    }
    m_attached.clear();
    rebuild();
    emit sessionChanged();
    return n;
}

void PageSession::ingest(const PageDocument& doc, const QSet<QString>& hiddenGrids,
                         QVector<PageTarget>& rest, QVector<PageTarget>& shellLayer,
                         QVector<PageGridPaint>& restGrids, QVector<PageGridPaint>& shellGrids)
{
    QVector<PageGridPaint> g;
    QVector<PageTarget> piece =
        PageHit::collect(doc, frame(), hiddenGrids, m_props, m_dwellSuspended, &g);
    for (PageGridPaint& gp : g) {
        gp.pageId = doc.id;
        (gp.shell ? shellGrids : restGrids).push_back(std::move(gp));
    }
    for (PageTarget& t : piece) {
        t.pageId = doc.id;
        (t.shell ? shellLayer : rest).push_back(std::move(t));
    }
}

void PageSession::rebuild()
{
    QVector<PageTarget> rest;
    QVector<PageTarget> shellLayer;
    QVector<PageGridPaint> restGrids;
    QVector<PageGridPaint> shellGrids;
    ingest(m_root, hiddenRootGrids(), rest, shellLayer, restGrids, shellGrids);
    for (const AttachedPage& a : m_attached) {
        ingest(a.doc, {}, rest, shellLayer, restGrids, shellGrids);
    }
    rest.append(shellLayer);
    restGrids.append(shellGrids);
    m_targets = std::move(rest);
    m_gridPaints = std::move(restGrids);
    if (m_host) {
        const PageFrame fr = frame();
        QRectF reserved = PageHit::reservedBounds(m_root, fr, hiddenRootGrids());
        for (const AttachedPage& a : m_attached) {
            const QRectF piece = PageHit::reservedBounds(a.doc, fr);
            if (piece.isEmpty()) {
                continue;
            }
            reserved = reserved.isEmpty() ? piece : reserved.united(piece);
        }
        m_host->commit(m_targets, m_gridPaints, m_drawerScale, reserved);
    }
    refreshActive();
    syncAutoClose();
}

void PageSession::refreshDecorated()
{
    if (!m_decorate) {
        return;
    }
    for (AttachedPage& a : m_attached) {
        m_decorate(a.doc);
    }
    rebuild();
}

void PageSession::setShiftHeld(bool on)
{
    if (m_shiftHeld == on) {
        return;
    }
    m_shiftHeld = on;
    if (m_host) {
        m_host->setShiftHeld(on);
    }
}

void PageSession::refreshActive()
{
    if (!m_host) {
        return;
    }
    QSet<QString> ids;
    QSet<QString> locked;
    if (m_active) {
        for (const PageTarget& t : m_targets) {
            if (t.activeState.isEmpty()) {
                continue;
            }
            if (m_active(t.activeState)) {
                ids.insert(sessionKey(t));
            }
            QString lockKey = t.activeState;
            if (lockKey.startsWith(QLatin1String("mod.")) && !lockKey.endsWith(QLatin1String(".locked"))
                && !lockKey.endsWith(QLatin1String(".down"))) {
                lockKey += QStringLiteral(".locked");
            }
            if (lockKey.endsWith(QLatin1String(".locked")) && m_active(lockKey)) {
                locked.insert(sessionKey(t));
            }
        }
    }
    m_host->setActiveIds(std::move(ids));
    m_host->setLockedIds(std::move(locked));
}

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

void PageSession::playDrawerAppear()
{
    m_drawerPhase = DrawerPhase::Appear;
    m_drawerScale = kDrawerMinScale;
    m_drawerClock.restart();
    m_drawerTimer.start();
    rebuild();
    raise();
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
        if (PageHit::shapeContains(z, g.chrome, false, pos)) {
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
