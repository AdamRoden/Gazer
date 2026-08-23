#include "app/GazerServices.h"

#include "app/ActiveStateResolver.h"
#include "app/SettingsUi.h"
#include "layout/PageHit.h"
#include "layout/PageSession.h"
#include "layout/PageTypes.h"
#include "assist/AssistCommands.h"
#include "input/InputTypes.h"
#include "utils/Log.h"

#include <QColor>
#include <QDir>
#include <QPoint>
#include <QStandardPaths>
#include <QtGlobal>
#include <functional>

namespace gazer {

namespace {

void applyMouseAmountLabel(QString& label, const QString& command, const QString& move,
                           const QString& scroll)
{
    if (command == QLatin1String("cycleMouseMoveAmount")) {
        label = move;
    } else if (command == QLatin1String("cycleMouseScrollAmount")) {
        label = scroll;
    }
}

void stampMousePage(PageDocument& doc, const QString& move, const QString& scroll)
{
    std::function<void(PageGrid&)> walk = [&](PageGrid& g) {
        for (PageCell& c : g.cells) {
            for (const PageAction& a : c.actions) {
                if (a.type == PageActionType::Command) {
                    applyMouseAmountLabel(c.label, a.command, move, scroll);
                }
            }
        }
        for (PageGrid& sub : g.subGrids) {
            walk(sub);
        }
    };
    for (PageGrid& g : doc.grids) {
        walk(g);
    }
}

} // namespace

GazerServices::GazerServices(QObject* parent)
    : QObject(parent)
{
}

GazerServices::~GazerServices() = default;

bool GazerServices::initialize(const QString& layoutsDir, const QString& mappingPath,
                               QString* error)
{
    Q_UNUSED(error);

    m_catalog = std::make_unique<PageCatalog>();
    m_pages = std::make_unique<PageSession>();
    m_input = std::make_unique<InputService>();
    m_mapping = std::make_unique<MappingEngine>(*m_input);
    m_tts = std::make_unique<TtsService>();
    m_phrases = std::make_unique<PhraseService>(*m_tts, *m_input, *m_mapping);
    m_commands = std::make_unique<CommandRegistry>(*m_mapping);
    m_lookToScroll = std::make_unique<LookToScroll>();
    m_magnifier = std::make_unique<MagnifierOverlay>();
    m_mouseDwellMove = std::make_unique<MouseDwellMove>();
    m_mouseAssist = std::make_unique<MouseAssistState>(*m_input);
    m_gazeReticle = std::make_unique<GazeReticle>();
    m_gazeMouseFollow = std::make_unique<GazeMouseFollow>();
    m_assistSession = std::make_unique<AssistSession>();
    m_actionLoops = std::make_unique<ActionLoopService>();
    m_scripts = std::make_unique<ScriptHost>(*m_phrases, *m_commands, *m_input, *m_pages);

    m_catalog->setDirectory(layoutsDir);
    const QString userLayouts =
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
            .filePath(QStringLiteral("layouts"));
    QDir().mkpath(userLayouts);
    m_catalog->setUserDirectory(userLayouts);
    m_pages->setCatalog(m_catalog.get());
    GAZER_INFO << "Pages available:" << m_catalog->scan();

    QString mapErr;
    if (!m_mapping->loadProfileFile(mappingPath, &mapErr)) {
        GAZER_WARN << "Mapping profile not loaded:" << mapErr;
    }

    QString setErr;
    if (!m_settings.loadFromFile(AppSettings::defaultFilePath(), &setErr)) {
        GAZER_INFO << "Using default settings (" << setErr << ")";
        m_settings = AppSettings::defaults();
    }

    m_settingsUi = std::make_unique<SettingsUi>(m_settings, *m_commands, *m_pages);
    m_settingsUi->setApplyFn([this](bool persist) { applySettings(persist); });
    m_settingsUi->setNotifyFn([this](const QString& msg) { notifyStatus(msg); });
    m_settingsUi->setMutateFn(
        [this](const std::function<void(AppSettings&)>& mutator, const QString& status) {
            mutateAndApply(mutator, status);
        });
    m_settingsUi->setResetFn([this]() { resetSettingsToDefaults(); });

    m_pages->setDecorate([this](PageDocument& doc) {
        if (doc.id == QLatin1String("lts_menu")) {
            doc.dwell.scanGrace = qMax(0, m_settings.scanGraceMs);
            doc.dwell.activation = QVector<int>{m_settings.ltsCenterDwellMs};
            const QString speed =
                QString::number(int(qRound(m_settings.ltsMaxNotchesPerSec)));
            for (PageGrid& g : doc.grids) {
                for (PageCell& c : g.cells) {
                    if (c.id == QLatin1String("hub")) {
                        c.label = speed;
                    }
                }
            }
            return;
        }
        m_settingsUi->decoratePage(doc);
        if (m_mouseAssist) {
            stampMousePage(doc, QStringLiteral("Step %1 px").arg(m_mouseAssist->moveAmountPx()),
                           QStringLiteral("Scroll ×%1").arg(m_mouseAssist->scrollNotches()));
        }
    });
    m_pages->setActiveResolver(
        [this](const QString& key) { return resolveActiveState(activeStateContext(), key); });
    m_pages->setLoopToggle([this](const PageTarget& t, const QString& pageId) {
        return m_actionLoops->toggle(pageId, sessionKey(t), t.actions, t.activeState);
    });
    m_pages->setLoopLatchClear([this]() { m_actionLoops->clearAllEngageLatches(); });
    m_pages->setLoopStopPage([this](const QString& pageId) { m_actionLoops->stopPage(pageId); });

    registerDomainCommands();
    m_settingsUi->registerCommands();

    connect(m_lookToScroll.get(), &LookToScroll::enabledChanged, this,
            [this](bool) { refreshActiveIndicators(); });
    connect(m_lookToScroll.get(), &LookToScroll::scrollSuspendedChanged, this,
            [this](bool) { refreshActiveIndicators(); });
    connect(m_lookToScroll.get(), &LookToScroll::maxNotchesPerSecChanged, this,
            [this](double n) {
                m_settings.ltsMaxNotchesPerSec = n;
                applySettings(true);
            });
    connect(m_mouseDwellMove.get(), &MouseDwellMove::armedChanged, this,
            [this](bool) { refreshActiveIndicators(); });
    connect(m_magnifier.get(), &MagnifierOverlay::enabledChanged, this,
            [this](bool) { refreshActiveIndicators(); });
    connect(m_mouseAssist.get(), &MouseAssistState::holdsChanged, this,
            [this]() { refreshActiveIndicators(); });
    connect(m_mouseAssist.get(), &MouseAssistState::amountsChanged, this,
            [this]() { refreshMouseAmountLabels(); });
    connect(m_gazeReticle.get(), &GazeReticle::enabledChanged, this,
            [this](bool) { refreshActiveIndicators(); });
    connect(m_gazeMouseFollow.get(), &GazeMouseFollow::enabledChanged, this,
            [this](bool) { refreshActiveIndicators(); });
    connect(m_actionLoops.get(), &ActionLoopService::loopsChanged, this,
            [this]() { refreshActiveIndicators(); });
    connect(this, &GazerServices::settingsChanged, this, [this]() { refreshActiveIndicators(); });
    connect(m_pages.get(), &PageSession::sessionChanged, this, [this]() { refreshActiveIndicators(); });

    applySettings(false);
    refreshActiveIndicators();
    return true;
}

void GazerServices::bindActionDispatch(ActionDispatchFn dispatch)
{
    m_pages->setDispatch(dispatch);
    m_actionLoops->setDispatchFn(
        [dispatch](const QVector<PageAction>& acts, const QString& pageId) {
            dispatch(acts, pageId, {});
        });
}

ActiveStateContext GazerServices::activeStateContext() const
{
    ActiveStateContext ctx;
    ctx.settings = &m_settings;
    ctx.lookToScroll = m_lookToScroll.get();
    ctx.mouseDwellMove = m_mouseDwellMove.get();
    ctx.magnifier = m_magnifier.get();
    ctx.gazeReticle = m_gazeReticle.get();
    ctx.gazeMouseFollow = m_gazeMouseFollow.get();
    ctx.mouseAssist = m_mouseAssist.get();
    ctx.actionLoops = m_actionLoops.get();
    ctx.settingsUi = m_settingsUi.get();
    ctx.dwellSuspended = isDwellSuspended();
    return ctx;
}

void GazerServices::setDwellSuspended(bool on)
{
    if (m_pages) {
        m_pages->setDwellSuspended(on);
    }
}

void GazerServices::toggleDwellSuspended()
{
    setDwellSuspended(!isDwellSuspended());
}

bool GazerServices::isDwellSuspended() const
{
    return m_pages && m_pages->isDwellSuspended();
}

void GazerServices::refreshActiveIndicators()
{
    if (m_pages) {
        m_pages->refreshActive();
    }
}

void GazerServices::refreshMouseAmountLabels()
{
    if (m_pages) {
        m_pages->refreshDecorated();
    }
}

void GazerServices::notifyStatus(const QString& msg)
{
    emit m_commands->statusMessage(msg);
}

void GazerServices::mutateAndApply(const std::function<void(AppSettings&)>& mutator,
                                   const QString& status)
{
    mutator(m_settings);
    m_settings.clamp();
    applySettings(true);
    if (!status.isEmpty()) {
        notifyStatus(status);
    }
}

void GazerServices::applySettings(bool persist)
{
    m_settings.clamp();

    if (m_pages) {
        m_pages->setAutoCollapseMain(m_settings.autoCollapseMain);
    }

    ProgressVisuals boardPv;
    boardPv.style.radial = m_settings.progressRadial;
    boardPv.style.fillBackground = m_settings.progressFill;
    boardPv.style.border = m_settings.progressBorder;
    boardPv.progressColor = m_settings.colorKey(QStringLiteral("progressColor"));
    boardPv.fillColor = m_settings.colorKey(QStringLiteral("progressFillColor"));
    boardPv.borderColor = m_settings.colorKey(QStringLiteral("progressBorderColor"));
    boardPv.flashUseForeground = m_settings.flashUseForeground;
    boardPv.flashForegroundOpacity = m_settings.flashForegroundOpacity;
    boardPv.flashColor = m_settings.colorKey(QStringLiteral("flashColor"));
    boardPv.flashMs = m_settings.flashMs;
    if (m_pages) {
        m_pages->setProgressVisuals(boardPv);
        m_pages->setTheme(m_settings.resolvedTheme());
        m_pages->setGlobalDwell(m_settings.dwellSequence, m_settings.dwellGraceMs,
                                m_settings.scanGraceMs);
    }

    m_mouseDwellMove->setDwellMs(m_settings.mouseMoveDwellMs);
    m_mouseDwellMove->setMagPickDwellMs(m_settings.magPickDwellMs);
    m_mouseDwellMove->setMagPickStyle(m_settings.magPickStyle);
    m_mouseDwellMove->setMousePickStyle(m_settings.mousePickStyle);
    m_mouseDwellMove->setSelectTimeoutMs(m_settings.mouseMoveSelectTimeoutMs);
    m_mouseDwellMove->setMagPickEnabled(m_settings.mouseMoveMagPick);
    m_mouseDwellMove->setMagPickCenterOnDwell(m_settings.mouseMoveMagPickCenterOnDwell);
    m_mouseDwellMove->setMagPickFullScreen(m_settings.mouseMoveMagPickFullScreen);
    m_mouseDwellMove->setForesightEnabled(m_settings.mouseMoveForesight);
    m_mouseDwellMove->setForesightDwellMs(m_settings.mouseMoveForesightDwellMs);
    m_mouseDwellMove->setForesightDoubleZoom(m_settings.mouseMoveForesightDoubleZoom);
    m_mouseDwellMove->setPickZoom(m_settings.pickZoom);
    m_mouseDwellMove->setPickWindowPx(m_settings.pickWindowPx);
    m_mouseDwellMove->setPickWindowRound(m_settings.pickWindowRound);
    ProgressVisuals mousePv = boardPv;
    mousePv.style.radial = m_settings.mouseProgressRadial;
    mousePv.style.fillBackground = m_settings.mouseProgressFill;
    mousePv.style.border = m_settings.mouseProgressBorder;
    m_mouseDwellMove->setProgressVisuals(mousePv);
    m_gazeReticle->setColor(boardPv.progressColor);
    m_magnifier->setAccent(boardPv.progressColor);
    m_lookToScroll->setAccent(boardPv.progressColor);

    m_lookToScroll->setDeadzonePx(m_settings.ltsDeadzonePx);
    m_lookToScroll->setFalloffPx(m_settings.ltsFalloffPx);
    m_lookToScroll->setMaxNotchesPerSec(m_settings.ltsMaxNotchesPerSec);
    m_lookToScroll->setAccelPerSec(m_settings.ltsAccelPerSec);
    m_lookToScroll->setCenterDwellMs(m_settings.ltsCenterDwellMs);
    m_lookToScroll->setIndicatorStyle(m_settings.ltsIndicatorStyle);

    m_magnifier->setZoom(m_settings.magZoom);
    m_magnifier->setLensSize(m_settings.magLensSize);
    m_magnifier->setFollowProfile(m_settings.magFollowProfile);
    m_gazeReticle->setFollowProfile(m_settings.magFollowProfile);
    m_gazeMouseFollow->setFollowProfile(m_settings.magFollowProfile);

    m_mapping->setSpeakAlsoType(m_settings.speakAlsoType);

    if (m_pages) {
        m_pages->refreshDecorated();
    }

    if (persist) {
        QString err;
        if (!m_settings.saveToFile(AppSettings::defaultFilePath(), &err)) {
            GAZER_WARN << "Failed to save settings:" << err;
        }
    }
    emit settingsChanged();
}

bool GazerServices::reloadSettings(QString* error)
{
    AppSettings loaded = AppSettings::defaults();
    if (!loaded.loadFromFile(AppSettings::defaultFilePath(), error)) {
        return false;
    }
    m_settings = loaded;
    applySettings(false);
    return true;
}

void GazerServices::resetSettingsToDefaults()
{
    m_settings = AppSettings::defaults();
    applySettings(true);
    notifyStatus(QStringLiteral("Settings reset to defaults"));
}

void GazerServices::registerDomainCommands()
{
    m_assistCmdCtx = std::make_unique<AssistCommandContext>();
    m_assistCmdCtx->commands = m_commands.get();
    m_assistCmdCtx->session = m_assistSession.get();
    m_assistCmdCtx->pages = m_pages.get();
    m_assistCmdCtx->lookToScroll = m_lookToScroll.get();
    m_assistCmdCtx->mouseDwellMove = m_mouseDwellMove.get();
    m_assistCmdCtx->magnifier = m_magnifier.get();
    m_assistCmdCtx->gazeReticle = m_gazeReticle.get();
    m_assistCmdCtx->gazeMouseFollow = m_gazeMouseFollow.get();
    m_assistCmdCtx->mouseAssist = m_mouseAssist.get();
    m_assistCmdCtx->actionLoops = m_actionLoops.get();
    m_assistCmdCtx->settings = &m_settings;
    m_assistCmdCtx->applySettings = [this](bool persist) { applySettings(persist); };
    m_assistCmdCtx->refreshActiveIndicators = [this]() { refreshActiveIndicators(); };
    m_assistCmdCtx->notifyStatus = [this](const QString& msg) { notifyStatus(msg); };
    m_assistCmdCtx->setDwellSuspended = [this](bool on) { setDwellSuspended(on); };
    m_assistCmdCtx->isDwellSuspended = [this]() { return isDwellSuspended(); };
    registerAssistCommands(*m_assistCmdCtx);

    m_commands->registerBuiltin(QStringLiteral("mouseMoveToGaze"), [this](QString* error) {
        if (!m_lastGaze.valid) {
            if (error) {
                *error = QStringLiteral("No valid gaze sample");
            }
            return false;
        }
        InputOutput o;
        o.type = InputOutput::Type::MouseMoveTo;
        o.dx = qRound(m_lastGaze.x);
        o.dy = qRound(m_lastGaze.y);
        return m_input->execute(o, error);
    });
    m_commands->registerBuiltin(QStringLiteral("mouseLeftClick"), [this](QString* error) {
        InputOutput o;
        o.type = InputOutput::Type::MouseClick;
        o.button = QStringLiteral("left");
        return m_input->execute(o, error);
    });
    // Shared sticky policy: stop layout actionLoops + assist sticky (gaze click loop).
    m_commands->registerBuiltin(QStringLiteral("stopAllActionLoops"), [this](QString*) {
        if (m_actionLoops) {
            m_actionLoops->stopAll();
        }
        if (m_mouseDwellMove && m_mouseDwellMove->isClickLoop()) {
            m_mouseDwellMove->setArmed(false);
        }
        refreshActiveIndicators();
        notifyStatus(QStringLiteral("All action loops stopped"));
        return true;
    });
}

} // namespace gazer
