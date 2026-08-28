#include "layout/PageSession.h"

#include "layout/PageCatalog.h"
#include "layout/PageHit.h"
#include "layout/PageLoader.h"
#include "ui/PageHostWindow.h"
#include "ui/ProgressVisuals.h"
#include "ui/Theme.h"
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

PageSession::~PageSession() = default;

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

    ensureHost();
    m_dwell.setEnabled(true);
    rebuild();
    if (m_host) {
        m_host->showHost();
    }
    GAZER_INFO << "Page root opened" << m_root.id << "from" << xmlPath;
    emit sessionChanged();
    return true;
}

void PageSession::ensureHost()
{
    if (m_host) {
        return;
    }
    m_host = std::make_unique<PageHostWindow>();
    connect(m_host.get(), &PageHostWindow::targetClicked, this, [this](const QString& id) {
        activateTarget(id);
    });
    m_host->setShiftHeld(m_shiftHeld);
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
                leaveGaze();
                rebuild();
                raise();
                armLeaveGate(id);
            } else {
                rebuild();
                if (!m_hoverId.isEmpty() && !findTarget(m_hoverId)) {
                    leaveGaze();
                }
            }
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

void PageSession::ingest(const PageDocument& doc, const QSet<QString>& hiddenGrids, bool isMaster,
                         QVector<PageTarget>& targets, QVector<PageGridPaint>& gridPaints)
{
    QVector<PageGridPaint> g;
    QVector<PageTarget> piece =
        PageHit::collect(doc, frame(), hiddenGrids, m_props, m_dwellSuspended, &g);
    for (PageGridPaint& gp : g) {
        gp.pageId = doc.id;
        gp.master = isMaster;
        gridPaints.push_back(std::move(gp));
    }
    for (PageTarget& t : piece) {
        t.pageId = doc.id;
        t.master = isMaster;
        targets.push_back(std::move(t));
    }
}

void PageSession::rebuild()
{
    m_targets.clear();
    m_gridPaints.clear();
    for (const AttachedPage& a : m_attached) {
        ingest(a.doc, {}, false, m_targets, m_gridPaints);
    }
    ingest(m_root, hiddenRootGrids(), true, m_targets, m_gridPaints);

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
    if (m_host) {
        m_host->setActiveIds(std::move(ids));
        m_host->setLockedIds(std::move(locked));
    }
}

} // namespace gazer
