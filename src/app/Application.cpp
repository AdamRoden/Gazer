#include "app/Application.h"

#include "app/AppSettings.h"
#include "app/SettingsUi.h"
#include "editor/LayoutEditorWindow.h"
#include "layout/LayoutInstance.h"
#include "layout/PageSession.h"
#include "ui/OverlaySurface.h"
#include "core/TrackerMouse.h"
#include "core/TrackerTobii.h"
#include "utils/Log.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSize>
#include <QStandardPaths>
#include <QTimer>

#include <cstdlib>

namespace gazer {

Application::Application(QObject* parent)
    : QObject(parent)
{
}

Application::~Application()
{
    if (m_tracker) {
        m_tracker->stop();
    }
}

bool Application::initialize()
{
    m_svc = std::make_unique<GazerServices>();
    const QString appDir = QCoreApplication::applicationDirPath();
    QString err;
    if (!m_svc->initialize(QDir(appDir).filePath(QStringLiteral("resources/layouts")),
                           QDir(appDir).filePath(QStringLiteral("resources/mappings/default.json")),
                           &err)) {
        GAZER_ERROR << "Services init failed:" << err;
        return false;
    }

    m_actions = std::make_unique<ActionDispatcher>(*m_svc);
    m_preview = std::make_unique<PreviewWindow>();
    m_tray = std::make_unique<TrayIcon>();
    m_edgeBubbles = std::make_unique<EdgeBubbleOverlay>();
    m_dwellSuspendOverlay = std::make_unique<DwellSuspendOverlay>();
    m_svc->instances().setEdgeBubbleOverlay(m_edgeBubbles.get());
    auto restackChrome = [this]() {
        if (m_restacking) {
            return;
        }
        m_restacking = true;
        m_svc->instances().restackChrome();
        if (m_dwellSuspendOverlay && m_dwellSuspendOverlay->isVisible()) {
            m_dwellSuspendOverlay->raiseStack();
        }
        m_restacking = false;
    };
    connect(m_edgeBubbles.get(), &OverlaySurface::stackChanged, this, restackChrome);
    connect(m_dwellSuspendOverlay.get(), &OverlaySurface::stackChanged, this, restackChrome);

    // Domain owns loops + lifecycle; shell only supplies the dispatcher.
    m_svc->bindActionDispatch([this](const QVector<LayoutAction>& acts, const QString& sourceId) {
        if (m_actions) {
            m_actions->dispatchAll(acts, sourceId);
        }
    });

    m_gazeRouter.setInstances(&m_svc->instances());
    m_gazeRouter.setPages(&m_svc->pages());
    m_gazeRouter.setAssistSession(&m_svc->assistSession());
    m_gazeRouter.setLookToScroll(&m_svc->lookToScroll());
    m_gazeRouter.setMouseDwellMove(&m_svc->mouseDwellMove());
    m_gazeRouter.setMagnifier(&m_svc->magnifier());
    m_gazeRouter.setGazeReticle(&m_svc->gazeReticle());
    m_gazeRouter.setGazeMouseFollow(&m_svc->gazeMouseFollow());

    m_preview->setTheme(m_svc->settings().resolvedTheme());

    connect(m_tray.get(), &TrayIcon::showPreviewRequested, m_preview.get(),
            &PreviewWindow::showAndRaise);
    connect(m_tray.get(), &TrayIcon::showLayoutRequested, this, [this]() {
        m_svc->pages().raise();
        updateTrayStatus();
    });
    connect(m_tray.get(), &TrayIcon::layoutEditorRequested, this, [this]() { openLayoutEditor(); });
    connect(m_tray.get(), &TrayIcon::quitRequested, this, &Application::onQuitRequested);

    m_svc->commands().registerBuiltin(
        QStringLiteral("openLayoutEditor"),
        [this](const CommandRegistry::Invocation& inv, QString*) {
            QString id = inv.layoutId;
            if (id.isEmpty() && !inv.sourceInstanceId.isEmpty()) {
                if (const auto* inst = m_svc->instances().instance(inv.sourceInstanceId)) {
                    id = inst->document().id;
                }
            }
            openLayoutEditor(id);
            return true;
        });

    m_svc->commands().registerBuiltin(QStringLiteral("quitApp"), [this](QString*) {
        QTimer::singleShot(0, this, &Application::onQuitRequested);
        return true;
    });
    // Dock "Main ▶" — show the home child of the persistent root.
    m_svc->commands().registerBuiltin(QStringLiteral("expandMaster"), [this](QString* error) {
        return expandMasterShell(error);
    });
    m_svc->commands().registerBuiltin(QStringLiteral("collapseMaster"), [this](QString*) {
        m_svc->pages().applyPageAction(PageVerb::Close, PageTargetKind::Grid,
                                       QStringLiteral("all"));
        updateTrayStatus();
        return true;
    });
    m_svc->commands().registerBuiltin(QStringLiteral("closeOtherViews"), [this](QString*) {
        const int pages = m_svc->pages().closeAttached();
        const int json = m_svc->instances().closeOtherViews();
        updateTrayStatus();
        if (m_tray) {
            m_tray->setStatus(
                QStringLiteral("Closed %1 other board(s)").arg(pages + json));
        }
        return true;
    });
    m_svc->commands().registerBuiltin(QStringLiteral("openPreview"), [this](QString*) {
        if (m_preview) {
            m_preview->showAndRaise();
        }
        return true;
    });
    m_svc->commands().registerBuiltin(QStringLiteral("theme.dark"), [this](QString*) {
        m_svc->settings().themeMode = ThemeMode::Dark;
        m_svc->applySettings(true);
        if (m_preview) {
            m_preview->setTheme(m_svc->settings().resolvedTheme());
        }
        return true;
    });
    m_svc->commands().registerBuiltin(QStringLiteral("theme.light"), [this](QString*) {
        m_svc->settings().themeMode = ThemeMode::Light;
        m_svc->applySettings(true);
        if (m_preview) {
            m_preview->setTheme(m_svc->settings().resolvedTheme());
        }
        return true;
    });
    m_svc->commands().registerBuiltin(QStringLiteral("theme.custom"), [this](QString*) {
        m_svc->settings().applyCustomPalette(false);
        m_svc->applySettings(true);
        if (m_preview) {
            m_preview->setTheme(m_svc->settings().resolvedTheme());
        }
        return true;
    });

    connect(&m_svc->instances(), &LayoutInstanceManager::sessionChanged, this,
            &Application::syncDwellSuspendOverlay);
    connect(&m_svc->pages(), &PageSession::dwellSuspendChanged, this,
            [this](bool) { syncDwellSuspendOverlay(); });
    connect(&m_svc->pages(), &PageSession::sessionChanged, this, &Application::updateTrayStatus);

    auto statusToTray = [this](const QString& msg) {
        if (m_tray) {
            m_tray->setStatus(msg);
        }
    };
    connect(m_svc.get(), &GazerServices::settingsChanged, this, [this]() { updateTrayStatus(); });
    connect(m_actions.get(), &ActionDispatcher::statusMessage, this, statusToTray);
    connect(&m_svc->commands(), &CommandRegistry::statusMessage, this, statusToTray);
    connect(&m_svc->scripts(), &ScriptHost::statusMessage, this, statusToTray);
    connect(&m_svc->phrases(), &PhraseService::spoken, this,
            [statusToTray](const QString& t) { statusToTray(QStringLiteral("Said: %1").arg(t)); });

    connect(&m_svc->instances(), &LayoutInstanceManager::itemActivated, this,
            &Application::onItemActivated);
    connect(&m_svc->instances(), &LayoutInstanceManager::sessionChanged, this,
            &Application::updateTrayStatus);

    const QString mainXml =
        QDir(appDir).filePath(QStringLiteral("resources/layouts/main.xml"));
    m_svc->pages().setDispatch([this](const QVector<PageAction>& acts, const QString& pageId,
                                      const QString& targetId) {
        if (m_actions) {
            m_actions->dispatchPage(acts, pageId, targetId);
        }
    });

    if (!QFileInfo::exists(mainXml)) {
        GAZER_ERROR << "Root page missing:" << mainXml;
        return false;
    }
    m_svc->pages().setLayoutsDirectory(QFileInfo(mainXml).absolutePath());
    if (!m_svc->pages().openRoot(mainXml, &err)) {
        GAZER_ERROR << "Failed to open root page:" << err;
        return false;
    }
    m_svc->applySettings(false);
    if (!m_svc->settings().startDocked) {
        QString expandErr;
        if (!m_svc->pages().applyPageAction(PageVerb::Open, PageTargetKind::Grid,
                                            QStringLiteral("drawer"), &expandErr)) {
            GAZER_WARN << "Initial expand drawer failed:" << expandErr;
        }
    }

    if (!startTracker()) {
        return false;
    }

    updateTrayStatus();
    GAZER_INFO << "Gazer running. Tracker:" << m_tracker->name()
               << "TTS:" << (m_svc->tts().isAvailable() ? "yes" : "no")
               << "settings:" << AppSettings::defaultFilePath();

    if (QCoreApplication::arguments().contains(QStringLiteral("--editor"))) {
        openLayoutEditor();
    }
    return true;
}

void Application::updateTrayStatus()
{
    if (!m_tray || !m_tracker || !m_svc) {
        return;
    }
    const int n = m_svc->pages().openCount() + m_svc->instances().visibleBoardCount();
    m_tray->setStatus(QStringLiteral("%1 boards · %2").arg(n).arg(m_tracker->name()));
}

bool Application::startTracker()
{
    const int pref = m_svc ? m_svc->settings().trackerPref : 0;
    if (pref == 1) {
        GAZER_INFO << "Tracker preference: mouse only";
        fallbackToMouse();
        return m_tracker && m_tracker->isRunning();
    }

    {
        auto tobii = std::make_unique<TrackerTobii>();
        if (tobii->start()) {
            m_tracker = std::move(tobii);
            wireTracker();
            if (auto* t = qobject_cast<TrackerTobii*>(m_tracker.get())) {
                connect(t, &TrackerTobii::streamFailed, this, &Application::onTobiiStreamFailed);
            }
            return true;
        }
    }

    GAZER_INFO << "Tobii unavailable — falling back to mouse cursor";
    fallbackToMouse();
    return m_tracker && m_tracker->isRunning();
}

void Application::fallbackToMouse()
{
    if (m_tracker) {
        m_tracker->stop();
        m_tracker.reset();
    }
    auto mouse = std::make_unique<TrackerMouse>();
    if (!mouse->start()) {
        GAZER_ERROR << "Mouse tracker failed to start";
        return;
    }
    m_tracker = std::move(mouse);
    wireTracker();
}

void Application::onTobiiStreamFailed(const QString& reason)
{
    GAZER_WARN << "Tobii stream failed:" << reason << "— switching to mouse";
    fallbackToMouse();
    updateTrayStatus();
}

void Application::syncDwellSuspendOverlay()
{
    if (!m_dwellSuspendOverlay || !m_svc) {
        return;
    }
    const bool susp = m_svc->isDwellSuspended();
    m_dwellSuspendOverlay->setSuspended(susp);
    if (!susp) {
        return;
    }
    // Gap at dwell-exempt unpause targets (toggleDwellSuspend / resumeDwell),
    // including unbounded edge affordances (coerced on-screen band).
    QVector<QRect> gaps;
    for (LayoutInstance* inst : m_svc->instances().instances()) {
        if (!inst) {
            continue;
        }
        for (const LayoutItem& item : inst->document().items) {
            bool isUnpause = false;
            for (const LayoutAction& a : item.effectiveActions()) {
                if (a.type == LayoutAction::Type::Command
                    && (a.name == QLatin1String("toggleDwellSuspend")
                        || a.name == QLatin1String("resumeDwell"))) {
                    isUnpause = true;
                    break;
                }
            }
            if (!isUnpause) {
                continue;
            }
            const QRect gap = inst->unpauseGapScreenRect(item);
            if (!gap.isEmpty()) {
                // Expand slightly so the border break is obvious.
                gaps.push_back(gap.adjusted(-16, -16, 16, 16));
            }
        }
    }
    for (const QRect& gap : m_svc->pages().unpauseGapRects()) {
        gaps.push_back(gap);
    }
    m_dwellSuspendOverlay->setGapRects(gaps);
    m_dwellSuspendOverlay->refreshGeometry();
}

void Application::wireTracker()
{
    if (!m_tracker) {
        return;
    }
    // Avoid stacking multiple connections on tracker restart.
    disconnect(m_tracker.get(), nullptr, this, nullptr);
    if (m_preview) {
        disconnect(m_tracker.get(), nullptr, m_preview.get(), nullptr);
    }
    connect(m_tracker.get(), &ITracker::gazeUpdated, this, &Application::onGaze);
    connect(m_tracker.get(), &ITracker::gazeUpdated, m_preview.get(),
            &PreviewWindow::onGazeUpdated);
    connect(m_tracker.get(), &ITracker::headPoseUpdated, m_preview.get(),
            &PreviewWindow::onHeadPoseUpdated);
    connect(m_tracker.get(), &ITracker::trackingLost, m_preview.get(), [this]() {
        if (m_tracker) {
            m_preview->setTrackerName(QStringLiteral("%1 (lost)").arg(m_tracker->name()));
        }
    });
    connect(m_tracker.get(), &ITracker::trackingRestored, m_preview.get(), [this]() {
        if (m_tracker) {
            m_preview->setTrackerName(m_tracker->name());
        }
    });

    m_preview->setTrackerName(m_tracker->name());
    m_tray->setStatus(QStringLiteral("%1 — running").arg(m_tracker->name()));
    GAZER_INFO << "Using tracker:" << m_tracker->name();
}

void Application::onGaze(const gazer::GazePoint& point)
{
    if (point.valid) {
        m_svc->setLastGaze(point);
    }
    // Commit / follow color sliders before board dwell so leaving a slider can
    // activate minus/plus on the same sample.
    m_svc->settingsUi().onGaze(point);
    m_gazeRouter.dispatch(point);
}

void Application::onItemActivated(const QString& instanceId, const QString& itemId)
{
    auto* inst = m_svc->instances().instance(instanceId);
    if (!inst) {
        return;
    }
    const LayoutItem* itemPtr = inst->document().findItem(itemId);
    if (!itemPtr) {
        GAZER_WARN << "Activated unknown item:" << itemId << "on" << instanceId;
        return;
    }

    const LayoutItem itemCopy = *itemPtr;
    const QString sourceId = instanceId;
    const auto acts = itemCopy.effectiveActions();
    GAZER_INFO << "Activate:" << sourceId << itemCopy.id << itemCopy.label
               << "actions" << acts.size() << (itemCopy.actionLoop ? "loop" : "");

    auto run = [this, itemCopy, sourceId]() {
        if (m_actions) {
            m_actions->dispatchItem(itemCopy, sourceId);
        }
        if (m_svc->mouseDwellMove().isArmed()) {
            bool armsMove = false;
            for (const LayoutAction& a : itemCopy.effectiveActions()) {
                if (a.type != LayoutAction::Type::Command) {
                    continue;
                }
                if (a.name == QLatin1String("mouseDwellMove")
                    || a.name == QLatin1String("mouseDwellClickLoop")
                    || a.name == QLatin1String("mouseMoveAndLeftClick")
                    || a.name == QLatin1String("mouseMoveAndRightClick")
                    || a.name == QLatin1String("mouseMoveAndMiddleClick")) {
                    armsMove = true;
                    break;
                }
            }
            if (armsMove) {
                if (auto* src = m_svc->instances().instance(sourceId)) {
                    const QRect r = src->itemScreenRect(itemCopy.id);
                    if (!r.isEmpty()) {
                        m_svc->mouseDwellMove().gateUntilGazeLeaves(r);
                    }
                }
            }
        }
        m_svc->instances().restackChrome();
        updateTrayStatus();
    };

    QTimer::singleShot(0, this, run);
}

bool Application::expandMasterShell(QString* error)
{
    if (!m_svc->pages().applyPageAction(PageVerb::Open, PageTargetKind::Grid,
                                        QStringLiteral("drawer"), error)) {
        if (m_tray && error) {
            m_tray->setStatus(QStringLiteral("Expand failed: %1").arg(*error));
        }
        return false;
    }

    updateTrayStatus();
    if (m_tray) {
        m_tray->setStatus(QStringLiteral("Main restored"));
    }
    return true;
}

void Application::openLayoutEditor(const QString& layoutId)
{
    if (!m_editor) {
        m_editor = std::make_unique<LayoutEditorWindow>();
        m_editor->setLayoutsDirectory(m_svc->catalog().layoutsDirectory());
        const QString userLayouts =
            QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                .filePath(QStringLiteral("layouts"));
        m_editor->setUserLayoutsDirectory(userLayouts);
        m_editor->setTestHandler(
            [this](const QVector<LayoutDocument>& family, int current, QString* error) {
                return testEditedLayout(family, current, error);
            });
    }
    QStringList ids = m_svc->catalog().layoutIds();
    QStringList labels;
    for (const QString& id : ids) {
        const LayoutDocument* d = m_svc->catalog().document(id);
        const QString name = d && !d->name.isEmpty() ? d->name : id;
        labels.push_back(QStringLiteral("%1  (%2)").arg(name, id));
    }
    m_editor->setCatalog(ids, labels);
    m_editor->setCommandNames(m_svc->commands().names());
    m_editor->setTheme(m_svc->settings().resolvedTheme());
    if (!layoutId.isEmpty()) {
        QString err;
        if (!m_editor->openLayoutId(layoutId, &err)) {
            GAZER_WARN << "Layout editor could not open" << layoutId << err;
            if (m_tray) {
                m_tray->setStatus(err.isEmpty() ? QStringLiteral("Could not open %1").arg(layoutId)
                                                : err);
            }
        }
    }
    m_editor->showAndRaise();
}

bool Application::testEditedLayout(const QVector<LayoutDocument>& family, int currentIndex,
                                   QString* error)
{
    if (!m_svc) {
        if (error) {
            *error = QStringLiteral("Gazer is not running");
        }
        return false;
    }
    return !m_svc->instances().openEditorPreview(family, currentIndex, error).isEmpty();
}

void Application::shutdownUi()
{
    if (m_edgeBubbles) {
        m_edgeBubbles->clearAll();
        m_edgeBubbles->hide();
    }
    if (m_svc) {
        m_svc->magnifier().setEnabledLens(false);
        m_svc->lookToScroll().setEnabled(false);
        m_svc->mouseDwellMove().setArmed(false);
        m_svc->mouseAssist().releaseAllHolds();
        m_svc->tts().stop();
        m_svc->instances().hideAll();
        m_svc->instances().shutdown();
        m_svc->pages().hideHost();
    }
    if (m_preview) {
        m_preview->hide();
    }
    if (m_editor) {
        m_editor->hide();
    }
    if (m_tray) {
        m_tray->hideIcon();
    }
}

void Application::onQuitRequested()
{
    shutdownUi();
    if (m_tracker) {
        m_tracker->stop();
        m_tracker.reset();
    }
    QApplication::quit();
    QTimer::singleShot(500, []() { std::exit(0); });
}

} // namespace gazer
