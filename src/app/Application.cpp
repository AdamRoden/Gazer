#include "app/Application.h"

#include "app/AppSettings.h"
#include "core/TrackerMouse.h"
#include "core/TrackerTobii.h"
#include "utils/Log.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QSize>
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
    m_dockReveal = std::make_unique<DockRevealOverlay>();
    m_edgeBubbles = std::make_unique<EdgeBubbleOverlay>();
    m_svc->instances().setEdgeBubbleOverlay(m_edgeBubbles.get());

    m_gazeRouter.setInstances(&m_svc->instances());
    m_gazeRouter.setDockReveal(m_dockReveal.get());
    m_gazeRouter.setLookToScroll(&m_svc->lookToScroll());
    m_gazeRouter.setMouseDwellMove(&m_svc->mouseDwellMove());
    m_gazeRouter.setMagnifier(&m_svc->magnifier());

    connect(m_tray.get(), &TrayIcon::showPreviewRequested, m_preview.get(),
            &PreviewWindow::showAndRaise);
    connect(m_tray.get(), &TrayIcon::showLayoutRequested, this, [this]() {
        m_svc->instances().raiseMaster();
        m_dockRevealed = false;
        syncMasterChrome();
    });
    connect(m_tray.get(), &TrayIcon::quitRequested, this, &Application::onQuitRequested);

    connect(m_dockReveal.get(), &DockRevealOverlay::dockRevealRequested, this, [this]() {
        auto* master = m_svc->instances().masterInstance();
        if (!master || !master->document().session.isGazeRevealDock()) {
            GAZER_WARN << "Dock reveal fired but master is not a gaze-reveal dock";
            return;
        }
        m_dockRevealed = true;
        // Strip must turn off or it steals gaze from the Main ▶ chip (same corner).
        if (m_dockReveal) {
            m_dockReveal->setEnabledReveal(false);
        }
        master->raise();
        if (m_tray) {
            m_tray->setStatus(QStringLiteral("Dock revealed — dwell Main ▶"));
        }
        GAZER_INFO << "Collapsed dock revealed";
    });

    m_svc->commands().registerBuiltin(QStringLiteral("quitApp"), [this](QString*) {
        QTimer::singleShot(0, this, &Application::onQuitRequested);
        return true;
    });
    // Dock "Main ▶" — navigate to home shell and force-show.
    m_svc->commands().registerBuiltin(QStringLiteral("expandMaster"), [this](QString* error) {
        return expandMasterShell(error);
    });
    m_svc->commands().registerBuiltin(QStringLiteral("closeOtherViews"), [this](QString*) {
        const int n = m_svc->instances().closeOtherViews();
        m_dockRevealed = false;
        syncMasterChrome();
        if (m_tray) {
            m_tray->setStatus(QStringLiteral("Closed %1 other board(s)").arg(n));
        }
        return true;
    });

    auto statusToTray = [this](const QString& msg) {
        if (m_tray) {
            m_tray->setStatus(msg);
        }
    };
    connect(m_actions.get(), &ActionDispatcher::statusMessage, this, statusToTray);
    connect(&m_svc->commands(), &CommandRegistry::statusMessage, this, statusToTray);
    connect(&m_svc->scripts(), &ScriptHost::statusMessage, this, statusToTray);
    connect(&m_svc->phrases(), &PhraseService::spoken, this,
            [statusToTray](const QString& t) { statusToTray(QStringLiteral("Said: %1").arg(t)); });

    connect(&m_svc->instances(), &LayoutInstanceManager::itemActivated, this,
            &Application::onItemActivated);
    connect(&m_svc->instances(), &LayoutInstanceManager::sessionChanged, this, [this]() {
        // Entering a new master document: reset reveal so dock starts hidden again.
        auto* master = m_svc->instances().masterInstance();
        if (!master || !master->document().session.isGazeRevealDock()) {
            m_dockRevealed = false;
        } else if (!m_dockRevealed) {
            // Fresh dock mode — keep hidden.
        }
        syncMasterChrome();
    });

    if (m_svc->instances().openInstance(QStringLiteral("example_main"), &err).isEmpty()) {
        GAZER_ERROR << "Failed to open default layout:" << err;
        return false;
    }

    // Optional: start collapsed to the dock chip.
    if (m_svc->settings().startDocked) {
        QString collapseErr;
        if (!m_svc->instances().navigateMaster(QStringLiteral("example_main_collapsed"),
                                               &collapseErr)) {
            GAZER_WARN << "startDocked navigate failed:" << collapseErr;
        }
    }

    if (!startTracker()) {
        return false;
    }

    syncMasterChrome();
    GAZER_INFO << "Gazer running. Tracker:" << m_tracker->name()
               << "TTS:" << (m_svc->tts().isAvailable() ? "yes" : "no")
               << "settings:" << AppSettings::defaultFilePath();
    return true;
}

void Application::syncMasterChrome()
{
    auto* master = m_svc ? m_svc->instances().masterInstance() : nullptr;
    const bool gazeDock = master && master->document().session.isGazeRevealDock();
    const int boardCount = m_svc ? m_svc->instances().count() : 0;
    const bool masterIsHome = master && master->document().session.isHome;

    // Reveal strip only while dock is collapsed *and* still hidden. Once the chip
    // is shown, the strip must not steal gaze from Main ▶.
    if (m_dockReveal) {
        m_dockReveal->setEnabledReveal(gazeDock && !m_dockRevealed);
    }

    if (master && gazeDock) {
        if (m_dockRevealed) {
            master->raise();
            // Keep secondary boards above the dock chip for both HWND and hit-test.
            if (boardCount > 1) {
                m_svc->instances().reassertStackTopVisual();
            }
        } else {
            master->hide();
            GAZER_INFO << "Dock hidden — look bottom-left strip to reveal";
        }
    } else if (master) {
        m_dockRevealed = false;
        if (m_dockReveal) {
            m_dockReveal->setEnabledReveal(false);
        }

        // Home Main (or sole board / quit shell alone): bring master to front.
        // Collapsed dock with other boards open: show chip but do NOT steal z-order
        // from Keyboard/Mouse/etc. — only the topmost board receives gaze in overlaps.
        if (masterIsHome || boardCount <= 1) {
            master->raise();
            GAZER_INFO << "Master shown layout" << master->layoutId();
        } else {
            if (master->window() && !master->window()->isVisible()) {
                master->raise();
            }
            m_svc->instances().reassertStackTopVisual();
            GAZER_INFO << "Master shell" << master->layoutId()
                       << "visible under stack top (boards:" << boardCount << ")";
        }
    }

    if (m_tray && m_tracker) {
        m_tray->setStatus(QStringLiteral("%1 boards · %2")
                              .arg(m_svc->instances().count())
                              .arg(m_tracker->name()));
    }
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
    syncMasterChrome();
}

void Application::wireTracker()
{
    if (!m_tracker) {
        return;
    }
    connect(m_tracker.get(), &ITracker::gazeUpdated, this, &Application::onGaze);
    connect(m_tracker.get(), &ITracker::gazeUpdated, m_preview.get(),
            &PreviewWindow::onGazeUpdated);
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
    GAZER_INFO << "Activate:" << sourceId << itemCopy.id << itemCopy.label
               << "action" << static_cast<int>(itemCopy.action.type)
               << itemCopy.action.name << itemCopy.action.layoutId;

    // expandMaster must run immediately (dock→Main races hide chrome if deferred).
    // All other actions go through ActionDispatcher on the next event-loop tick.
    const bool immediate =
        itemCopy.action.type == LayoutAction::Type::Command
        && itemCopy.action.name == QStringLiteral("expandMaster");

    auto run = [this, itemCopy, sourceId]() {
        if (m_actions) {
            m_actions->dispatch(itemCopy, sourceId);
        }
        if (auto* master = m_svc->instances().masterInstance()) {
            if (!master->document().session.isGazeRevealDock()) {
                m_dockRevealed = false;
                master->raise();
            }
        }
        syncMasterChrome();
    };

    if (immediate) {
        run();
    } else {
        QTimer::singleShot(0, this, run);
    }
}

bool Application::expandMasterShell(QString* error)
{
    m_dockRevealed = false;
    if (m_dockReveal) {
        m_dockReveal->setEnabledReveal(false);
    }

    auto* master = m_svc->instances().masterInstance();
    if (!master) {
        if (error) {
            *error = QStringLiteral("No master instance");
        }
        return false;
    }

    QString homeId = master->document().session.expandLayoutId;
    if (homeId.isEmpty()) {
        // Already on a home shell, or missing expandLayoutId — prefer isHome id.
        if (master->document().session.isHome) {
            homeId = master->layoutId();
        } else {
            homeId = QStringLiteral("example_main");
        }
    }

    GAZER_INFO << "expandMasterShell from" << master->layoutId() << "→" << homeId;

    if (master->layoutId() != homeId) {
        QString err;
        if (!m_svc->instances().navigateMaster(homeId, &err)) {
            GAZER_ERROR << "navigateMaster failed:" << err;
            if (error) {
                *error = err;
            }
            if (m_tray) {
                m_tray->setStatus(QStringLiteral("Expand failed: %1").arg(err));
            }
            return false;
        }
    }

    m_dockRevealed = false;
    if (m_dockReveal) {
        m_dockReveal->setEnabledReveal(false);
    }
    if (auto* m = m_svc->instances().masterInstance()) {
        m->raise();
        if (m->window()) {
            m->window()->showAndRaise();
        }
    }
    syncMasterChrome();

    if (auto* m = m_svc->instances().masterInstance()) {
        GAZER_INFO << "expandMasterShell done layout=" << m->layoutId()
                   << "visible=" << (m->window() && m->window()->isVisible())
                   << "size=" << (m->window() ? m->window()->size() : QSize());
    }
    if (m_tray) {
        m_tray->setStatus(QStringLiteral("Main restored"));
    }
    return true;
}

void Application::shutdownUi()
{
    if (m_edgeBubbles) {
        m_edgeBubbles->clearAll();
        m_edgeBubbles->hide();
    }
    if (m_dockReveal) {
        m_dockReveal->setEnabledReveal(false);
        m_dockReveal->hide();
    }
    if (m_svc) {
        m_svc->magnifier().setEnabledLens(false);
        m_svc->lookToScroll().setEnabled(false);
        m_svc->mouseDwellMove().setArmed(false);
        m_svc->mouseAssist().releaseAllHolds();
        m_svc->tts().stop();
        m_svc->instances().hideAll();
        m_svc->instances().shutdown();
    }
    if (m_preview) {
        m_preview->hide();
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
