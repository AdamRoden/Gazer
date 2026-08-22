#include "layout/PageSession.h"

#include "layout/DwellRegionSpace.h"
#include "layout/PageLoader.h"
#include "utils/Log.h"

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>
#include <QTransform>
#include <utility>

namespace gazer {

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
                    m_host->setHover(id, p);
                }
            });
    connect(&m_dwell, &DwellStateMachine::hoverChanged, this, [this](const QString& id) {
        m_hoverId = id;
        if (id.isEmpty() && m_host) {
            m_host->setHover({}, 0.0);
        }
    });
    connect(&m_dwell, &DwellStateMachine::itemActivated, this, [this](const QString& id) {
        activateTarget(id);
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
    m_hiddenZones.clear();
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

bool PageSession::applyPageAction(PageVerb verb, PageTargetKind kind, const QString& id,
                                  QString* error)
{
    if (!hasRoot()) {
        if (error) {
            *error = QStringLiteral("No root page");
        }
        return false;
    }

    if (kind == PageTargetKind::Page) {
        if (id.compare(QLatin1String("main_drawer"), Qt::CaseInsensitive) == 0) {
            return applyPageAction(verb, PageTargetKind::Grid, QStringLiteral("drawer"), error);
        }
        if (id.compare(QLatin1String("main_quit_confirm"), Qt::CaseInsensitive) == 0) {
            return applyPageAction(verb, PageTargetKind::Grid, QStringLiteral("quit"), error);
        }
        if (id.compare(QLatin1String("all"), Qt::CaseInsensitive) == 0) {
            if (verb == PageVerb::Close || verb == PageVerb::Toggle) {
                closeAttached();
                return true;
            }
        }
        if (id == m_root.id) {
            if (verb == PageVerb::Close) {
                if (error) {
                    *error = QStringLiteral("Cannot close the root page");
                }
                return false;
            }
            closeAttached();
            setRootChrome(RootChrome::Drawer);
            return true;
        }
        const bool attached = hasPage(id);
        if (verb == PageVerb::Close || (verb == PageVerb::Toggle && attached)) {
            if (attached) {
                closePage(id);
                return true;
            }
            if (error) {
                *error = QStringLiteral("Page not open: %1").arg(id);
            }
            return false;
        }
        if (verb == PageVerb::Open || verb == PageVerb::Toggle) {
            return openPage(id, error);
        }
        if (error) {
            *error = QStringLiteral("No handler for Page %1").arg(id);
        }
        return false;
    }

    if (kind == PageTargetKind::Grid) {
        const bool all = id.compare(QLatin1String("all"), Qt::CaseInsensitive) == 0;
        if (all) {
            if (verb == PageVerb::Close || verb == PageVerb::Toggle) {
                setRootChrome(RootChrome::Docked);
                return true;
            }
            if (error) {
                *error = QStringLiteral("Open Grid all is not valid");
            }
            return false;
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

    const bool all = id.compare(QLatin1String("all"), Qt::CaseInsensitive) == 0;
    QStringList ids;
    if (all) {
        for (const PageZone& z : m_root.zones) {
            if (!z.id.isEmpty()) {
                ids.push_back(z.id);
            }
        }
    } else {
        ids.push_back(id);
    }
    for (const QString& one : ids) {
        if (one.isEmpty()) {
            continue;
        }
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

PageFrame PageSession::frame() const
{
    PageFrame f;
    f.screen = QRectF(DwellRegionSpace::virtualDesktop());
    if (QScreen* s = QGuiApplication::primaryScreen()) {
        f.desktop = QRectF(s->availableGeometry());
    } else {
        f.desktop = f.screen;
    }
    if (f.screen.isEmpty()) {
        f.screen = f.desktop;
    }
    return f;
}

QString PageSession::xmlPathFor(const QString& id) const
{
    if (m_layoutsDir.isEmpty()) {
        return {};
    }
    return QDir(m_layoutsDir).filePath(id + QStringLiteral(".xml"));
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

bool PageSession::attachDocument(PageDocument doc, QString* error)
{
    if (!doc.isValid()) {
        if (error) {
            *error = QStringLiteral("Page has no id");
        }
        return false;
    }
    for (int i = 0; i < m_attached.size(); ++i) {
        if (m_attached[i].doc.id == doc.id) {
            m_attached[i].doc = std::move(doc);
            if (i != m_attached.size() - 1) {
                m_attached.move(i, m_attached.size() - 1);
            }
            leaveGaze();
            rebuild();
            raise();
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
    emit sessionChanged();
    return true;
}

bool PageSession::openPage(const QString& id, QString* error)
{
    if (id == m_root.id) {
        raise();
        return true;
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
    emit sessionChanged();
    GAZER_INFO << "Attached page" << id;
    return true;
}

void PageSession::closePage(const QString& id)
{
    for (int i = 0; i < m_attached.size(); ++i) {
        if (m_attached[i].doc.id == id) {
            m_attached.removeAt(i);
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
    m_attached.clear();
    rebuild();
    emit sessionChanged();
    return n;
}

void PageSession::ingest(const PageDocument& doc, const QSet<QString>& hiddenGrids,
                         const QSet<QString>& hiddenZones, QVector<PageTarget>& rest,
                         QVector<PageTarget>& shellLayer, QVector<PageGridPaint>& restGrids,
                         QVector<PageGridPaint>& shellGrids)
{
    QVector<PageGridPaint> g;
    QVector<PageTarget> piece = PageHit::collect(doc, frame(), hiddenGrids, hiddenZones, m_props,
                                                 m_dwellSuspended, &g);
    for (PageGridPaint& gp : g) {
        gp.pageId = doc.id;
        (gp.shell ? shellGrids : restGrids).push_back(std::move(gp));
    }
    for (PageTarget& t : piece) {
        t.pageId = doc.id;
        if (!doc.id.isEmpty() && !t.id.startsWith(doc.id + QLatin1Char('/'))) {
            t.id = doc.id + QLatin1Char('/') + t.id;
        }
        (t.shell ? shellLayer : rest).push_back(std::move(t));
    }
}

void PageSession::rebuild()
{
    QVector<PageTarget> rest;
    QVector<PageTarget> shellLayer;
    QVector<PageGridPaint> restGrids;
    QVector<PageGridPaint> shellGrids;
    ingest(m_root, hiddenRootGrids(), m_hiddenZones, rest, shellLayer, restGrids, shellGrids);
    for (const AttachedPage& a : m_attached) {
        ingest(a.doc, {}, {}, rest, shellLayer, restGrids, shellGrids);
    }
    rest.append(shellLayer);
    restGrids.append(shellGrids);
    m_targets = std::move(rest);
    m_gridPaints = std::move(restGrids);
    if (m_host) {
        m_host->coverVirtualDesktop();
        m_host->setGridPaints(m_gridPaints);
        m_host->setTargets(m_targets);
        m_host->setDrawerScale(m_drawerScale);
    }
    refreshActive();
    if (autoCloseIdleMs() >= 0) {
        if (!m_idleClockRunning) {
            noteActivity();
        }
        if (!m_autoCloseTimer.isActive()) {
            m_autoCloseTimer.start();
        }
    } else {
        m_autoCloseTimer.stop();
        m_idleClockRunning = false;
    }
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

void PageSession::refreshActive()
{
    if (!m_host) {
        return;
    }
    QSet<QString> ids;
    if (m_active) {
        for (const PageTarget& t : m_targets) {
            if (!t.activeState.isEmpty() && m_active(t.activeState)) {
                ids.insert(t.id);
            }
        }
    }
    m_host->setActiveIds(std::move(ids));
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

void PageSession::tickAutoClose()
{
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
        if (t.id == id) {
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
    const PageTarget* hit = PageHit::at(m_targets, point.toPointF(), m_drawerScale, m_hoverId,
                                        m_gridPaints);
    const QString id = hit ? hit->id : QString();
    if (hit) {
        noteActivity();
    }
    if (id != m_hoverId) {
        applyDwellFor(hit);
    }
    m_dwell.onGazeSample(point, id);
    return hit != nullptr;
}

void PageSession::leaveGaze()
{
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
    const QTransform xf = PageHit::drawerTransform(m_targets, m_drawerScale, m_gridPaints);
    for (const PageTarget& t : m_targets) {
        if (!t.interactive || !t.dwellExempt) {
            continue;
        }
        QRectF r = t.geom.contentOnScreen();
        if (r.isEmpty()) {
            r = t.geom.progressZone;
        }
        r = PageHit::mapDrawer(t, r, xf, m_drawerScale);
        if (!r.isEmpty()) {
            gaps.push_back(r.toRect().adjusted(-16, -16, 16, 16));
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
        m_host->flash(targetId);
    }
    emit targetActivated(pageId, targetId);
    if (m_dispatch) {
        m_dispatch(actions, pageId, targetId);
    }
}

} // namespace gazer
