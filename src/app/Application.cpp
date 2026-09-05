#include "app/Application.h"

#include "app/ActionDispatcher.h"
#include "app/AppSettings.h"
#include "app/CommandRegistry.h"
#include "app/GazerServices.h"
#include "app/SettingsUi.h"
#include "assist/ComboMouse.h"
#include "assist/LookToScroll.h"
#include "assist/MouseAssistState.h"
#include "assist/MouseDwellMove.h"
#include "assist/ClipPlayer.h"
#include "assist/PhraseService.h"
#include "assist/ScriptHost.h"
#include "assist/SpeechEngine.h"
#include "assist/TtsService.h"
#include "core/ITracker.h"
#include "core/TrackerMouse.h"
#include "core/TrackerTobii.h"
#include "editor/LayoutEditorWindow.h"
#include "input/KeyStateManager.h"
#include "layout/PageCatalog.h"
#include "layout/PageEdit.h"
#include "layout/PageLoader.h"
#include "layout/PageSession.h"
#include "layout/PageTypes.h"
#include "ui/DwellSuspendOverlay.h"
#include "ui/Theme.h"
#include "ui/MagnifierOverlay.h"
#include "ui/PreviewWindow.h"
#include "ui/TrayIcon.h"
#include "utils/Log.h"
#include "utils/WinOverlay.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSize>
#include <QTimer>

#include <cstdlib>
#include <initializer_list>

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
    m_dwellSuspendOverlay = std::make_unique<DwellSuspendOverlay>();
    m_stackWatch = std::make_unique<OverlayStackWatch>();

    // Domain owns loops + lifecycle; shell only supplies the dispatcher.
    m_svc->bindActionDispatch(
        [this](const QVector<PageAction>& acts, const QString& pageId, const QString& targetId) {
            if (m_actions) {
                m_actions->dispatchPage(acts, pageId, targetId);
            }
        });

    m_gazeRouter.setPages(&m_svc->pages());
    m_gazeRouter.setAssistSession(&m_svc->assistSession());
    m_gazeRouter.setLookToScroll(&m_svc->lookToScroll());
    m_gazeRouter.setComboMouse(&m_svc->comboMouse());
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
    connect(m_tray.get(), &TrayIcon::layoutEditorRequested, this, [this]() { openPageEditor(); });
    connect(m_tray.get(), &TrayIcon::quitRequested, this, &Application::onQuitRequested);

    m_svc->commands().registerBuiltin(
        QStringLiteral("openPageEditor"),
        [this](const CommandRegistry::Invocation& inv, QString*) {
            openPageEditor(inv.pageId);
            return true;
        });

    m_svc->commands().registerBuiltin(QStringLiteral("quitApp"), [this](QString*) {
        QTimer::singleShot(0, this, &Application::onQuitRequested);
        return true;
    });
    m_svc->commands().registerBuiltin(QStringLiteral("openPreview"), [this](QString*) {
        if (m_preview) {
            m_preview->showAndRaise();
        }
        return true;
    });
    connect(&m_svc->pages(), &PageSession::dwellSuspendChanged, this,
            [this](bool) { syncDwellSuspendOverlay(); });
    connect(&m_svc->pages(), &PageSession::sessionChanged, this, &Application::updateTrayStatus);

    auto statusToTray = [this](const QString& msg) {
        if (m_tray) {
            m_tray->setStatus(msg);
        }
    };
    connect(m_svc.get(), &GazerServices::settingsChanged, this, [this]() {
        updateTrayStatus();
        if (m_preview) {
            m_preview->setTheme(m_svc->settings().resolvedTheme());
        }
        if (m_editor) {
            m_editor->setTheme(m_svc->settings().resolvedTheme());
        }
    });
    connect(m_actions.get(), &ActionDispatcher::statusMessage, this, statusToTray);
    connect(&m_svc->commands(), &CommandRegistry::statusMessage, this, statusToTray);
    connect(&m_svc->scripts(), &ScriptHost::statusMessage, this, statusToTray);
    connect(&m_svc->phrases(), &PhraseService::spoken, this,
            [statusToTray](const QString& t) { statusToTray(QStringLiteral("Said: %1").arg(t)); });

    const QString shippedMain =
        QDir(appDir).filePath(QStringLiteral("resources/layouts/main.xml"));
    QString mainXml = m_svc->catalog().pathFor(QStringLiteral("main"));
    if (mainXml.isEmpty()) {
        mainXml = shippedMain;
    }

    if (!QFileInfo::exists(mainXml)) {
        GAZER_ERROR << "Root page missing:" << mainXml;
        return false;
    }
    m_svc->pages().setLayoutsDirectory(QFileInfo(shippedMain).absolutePath());
    if (!m_svc->pages().openRoot(mainXml, &err)) {
        GAZER_ERROR << "Failed to open root page:" << err;
        return false;
    }
    m_svc->applySettings(false);
    if (!m_svc->settings().startDocked) {
        QString expandErr;
        if (const PageAction* show = PageEdit::firstShowLayers(m_svc->pages().root())) {
            if (!m_svc->pages().showLayers({*show}, QStringLiteral("main"), {}, &expandErr)) {
                GAZER_WARN << "Initial expand drawer failed:" << expandErr;
            }
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
        openPageEditor();
    }
    return true;
}

void Application::updateTrayStatus()
{
    if (!m_tray || !m_tracker || !m_svc) {
        return;
    }
    const int n = m_svc->pages().openCount();
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
    // Gap at unpause targets (toggleDwellSuspend / resumeDwell): same rect gaze hits
    // (off-screen dwell union on-screen chip), clipped to the desktop.
    QVector<QRect> gaps;
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
    // Commit / follow color sliders before board dwell so leaving a slider can
    // activate minus/plus on the same sample.
    m_svc->settingsUi().onGaze(point);
    m_gazeRouter.dispatch(point);
    if (m_svc->isDwellSuspended()) {
        syncDwellSuspendOverlay();
    }
}

void Application::openPageEditor(const QString& pageId)
{
    if (!m_editor) {
        m_editor = std::make_unique<LayoutEditorWindow>();
        m_editor->setLayoutsDirectory(m_svc->catalog().directory());
        m_editor->setUserLayoutsDirectory(m_svc->catalog().userDirectory());
        m_editor->setTestHandler(
            [this](const PageDocument& doc, QString* error) { return testEditedLayout(doc, error); });
    }
    (void)m_svc->catalog().scan();
    QStringList ids;
    QStringList labels;
    for (const QString& id : m_svc->catalog().ids()) {
        ids.push_back(id);
        const QString name = m_svc->catalog().nameFor(id);
        labels.push_back(QStringLiteral("%1  (%2)").arg(name.isEmpty() ? id : name, id));
    }
    m_editor->setCatalog(ids, labels);
    m_editor->setCommandNames(m_svc->commands().names());
    m_editor->setTheme(m_svc->settings().resolvedTheme());
    if (!pageId.isEmpty()) {
        QString err;
        if (!m_editor->openLayoutId(pageId, &err)) {
            GAZER_WARN << "Page editor could not open" << pageId << err;
            if (m_tray) {
                m_tray->setStatus(err.isEmpty() ? QStringLiteral("Could not open %1").arg(pageId)
                                                : err);
            }
        }
    }
    m_editor->showAndRaise();
}

bool Application::testEditedLayout(const PageDocument& source, QString* error)
{
    if (!m_svc) {
        if (error) {
            *error = QStringLiteral("Gazer is not running");
        }
        return false;
    }
    if (source.master) {
        if (error) {
            *error = QStringLiteral("Master root pages cannot be live-tested");
        }
        return false;
    }
    PageDocument doc = source;
    const QString previewId = PageSession::previewId(doc.id);
    QHash<QString, QString> idMap;
    if (!doc.id.isEmpty()) {
        idMap.insert(doc.id, previewId);
    }
    doc.id = previewId;
    if (!doc.name.contains(QLatin1String("(preview)"))) {
        doc.name = doc.name.isEmpty() ? QStringLiteral("Preview")
                                      : doc.name + QStringLiteral(" (preview)");
    }
    PageEdit::remapPageActionTargets(doc, idMap);
    m_svc->pages().closePreviewPages();
    m_svc->pages().registerMemoryPage(doc);
    if (!m_svc->pages().attachDocument(std::move(doc), error, true)) {
        m_svc->pages().closePreviewPages();
        return false;
    }
    return true;
}

void Application::shutdownUi()
{
    if (m_svc) {
        m_svc->magnifier().setEnabledLens(false);
        m_svc->lookToScroll().setEnabled(false);
        m_svc->comboMouse().setEnabled(false);
        m_svc->mouseDwellMove().setArmed(false);
        m_svc->mouseAssist().releaseAllHolds();
        QString ignored;
        (void)m_svc->keyState().releaseAll(&ignored);
        m_svc->clipPlayer().stop();
        m_svc->speechEngine().stop();
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
