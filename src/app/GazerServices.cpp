#include "app/GazerServices.h"

#include "app/ActionChannel.h"
#include "app/ActiveStateResolver.h"
#include "app/CommandRegistry.h"
#include "app/ComposeUi.h"
#include "app/SettingsUi.h"
#include "assist/ActionLoopService.h"
#include "assist/AhkLauncher.h"
#include "assist/SidecarHost.h"
#include "assist/AssistCommands.h"
#include "assist/ComposeCommands.h"
#include "assist/AssistSession.h"
#include "assist/ComboMouse.h"
#include "assist/GazeMouseFollow.h"
#include "assist/GazeReticle.h"
#include "assist/HeadPoseMapper.h"
#include "assist/LookToMap.h"
#include "assist/LookToMaps.h"
#include "assist/MouseAssistState.h"
#include "assist/MouseDwellMove.h"
#include "assist/ClipPlayer.h"
#include "assist/ElevenClient.h"
#include "assist/SoundboardStore.h"
#include "assist/SpeechEngine.h"
#include "assist/SpeechHistory.h"
#include "assist/SpeechSecrets.h"
#include "assist/TtsService.h"
#include "input/InjectGate.h"
#include "input/InputService.h"
#include "input/InputTypes.h"
#include "input/KeyboardInjector.h"
#include "input/KeyStateManager.h"
#include "input/VirtualGamepad.h"
#include "layout/PageCatalog.h"
#include "layout/PageSession.h"
#include "layout/PageTypes.h"
#include "mapping/MappingEngine.h"
#include "ui/MagnifierOverlay.h"
#include "ui/ProgressVisuals.h"
#include "utils/Log.h"
#include "utils/WinOverlay.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QPoint>
#include <QStandardPaths>
#include <QtGlobal>
#include <utility>

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

GazerServices::~GazerServices()
{
    if (m_secrets) {
        m_secrets->forgetCache();
    }
}

bool GazerServices::initialize(const QString& layoutsDir, const QString& mappingPath,
                               QString* error, bool includeUserLayouts)
{
    Q_UNUSED(error);

    m_catalog = std::make_unique<PageCatalog>();
    m_pages = std::make_unique<PageSession>();
    m_keyState = std::make_unique<KeyStateManager>();
    m_keyState->setInjector([](const QString& key, bool down, QString* error) {
        return down ? KeyboardInjector::keyDown(key, error) : KeyboardInjector::keyUp(key, error);
    });
    m_input = std::make_unique<InputService>(*m_keyState);
    m_mapping = std::make_unique<MappingEngine>(*m_input);
    m_tts = std::make_unique<TtsService>();
    m_clips = std::make_unique<ClipPlayer>();
    m_secrets = std::make_unique<SpeechSecrets>();
    m_eleven = std::make_unique<ElevenClient>(this);
    m_speech = std::make_unique<SpeechEngine>(*m_tts, m_settings, *m_secrets, *m_eleven, *m_clips);
    m_commands = std::make_unique<CommandRegistry>(*m_mapping);
    m_board = std::make_unique<SoundboardStore>();
    {
        QString boardErr;
        if (!m_board->load(&boardErr) && !boardErr.isEmpty()) {
            GAZER_WARN << "Soundboard:" << boardErr;
        }
    }
    m_history = std::make_unique<SpeechHistory>();
    {
        QString histErr;
        if (!m_history->load(&histErr) && !histErr.isEmpty()) {
            GAZER_WARN << "Speech history:" << histErr;
        }
    }
    m_compose = std::make_unique<ComposeUi>(*m_pages, *m_speech, m_settings, *m_secrets, *m_eleven,
                                           *m_tts, *m_board, *m_history);
    m_lookToMaps = std::make_unique<LookToMaps>();
    m_lookToMaps->setInput(m_input.get());
    m_comboMouse = std::make_unique<ComboMouse>();
    m_magnifier = std::make_unique<MagnifierOverlay>();
    m_mouseDwellMove = std::make_unique<MouseDwellMove>();
    m_mouseAssist = std::make_unique<MouseAssistState>(*m_input);
    m_gazeReticle = std::make_unique<GazeReticle>();
    m_gazeMouseFollow = std::make_unique<GazeMouseFollow>();
    m_headPose = std::make_unique<HeadPoseMapper>();
    m_headPose->setInput(m_input.get());
    m_assistSession = std::make_unique<AssistSession>();
    m_actionLoops = std::make_unique<ActionLoopService>();
    m_ahk = std::make_unique<AhkLauncher>();
    m_sidecars = std::make_unique<SidecarHost>();
    m_sidecars->setAhk(m_ahk.get());
    m_sidecars->setActionPipe(ActionChannel::pipeName());

    m_catalog->setDirectory(layoutsDir);
    if (includeUserLayouts) {
        const QString userLayouts =
            QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
                .filePath(QStringLiteral("layouts"));
        QDir().mkpath(userLayouts);
        m_catalog->setUserDirectory(userLayouts);
    }
    m_pages->setCatalog(m_catalog.get());
    GAZER_INFO << "Pages available:" << m_catalog->scan();

    QString mapErr;
    if (!m_mapping->loadProfileFile(mappingPath, &mapErr)) {
        GAZER_WARN << "Mapping profile not loaded:" << mapErr;
    }

    QString setErr;
    const QString settingsPath = AppSettings::defaultFilePath();
    if (!m_settings.loadFromFile(settingsPath, &setErr)) {
        GAZER_INFO << "Using default settings (" << setErr << ")";
        m_settings = AppSettings::defaults();
        if (!QFile::exists(settingsPath)) {
            QString saveErr;
            if (!m_settings.saveToFile(settingsPath, &saveErr)) {
                GAZER_WARN << "Failed to write default settings:" << saveErr;
            }
        }
    }
    m_settings.elevenApiKeySet = m_secrets->hasKey();

    m_settingsUi = std::make_unique<SettingsUi>(m_settings, *m_commands, *m_pages, *m_secrets,
                                               *m_eleven);
    m_settingsUi->setApplyFn([this](bool persist) { applySettings(persist); });
    m_settingsUi->setNotifyFn([this](const QString& msg) { notifyStatus(msg); });
    m_settingsUi->setMutateFn(
        [this](const std::function<void(AppSettings&)>& mutator, const QString& status) {
            mutateAndApply(mutator, status);
        });
    m_settingsUi->setResetFn([this]() { resetSettingsToDefaults(); });
    m_settingsUi->setMouseDwellMove(m_mouseDwellMove.get());
    m_settingsUi->setHeadPoseMapper(m_headPose.get());
    m_settingsUi->setLookToMaps(m_lookToMaps.get());
    m_headPose->setRunCommand(
        [this](const QString& name, QString* error) { return m_commands->run(name, error); });
    m_headPose->setNotifyFn([this](const QString& msg) { notifyStatus(msg); });
    m_lookToMaps->setNotifyFn([this](const QString& msg) { notifyStatus(msg); });
    connect(m_mouseDwellMove.get(), &MouseDwellMove::movedTo, this,
            [this](QPoint pos) { m_settingsUi->onColorAimMoved(pos); });
    connect(m_mouseDwellMove.get(), &MouseDwellMove::armedChanged, this, [this](bool armed) {
        if (!armed) {
            m_settingsUi->cancelEyedropper();
        }
    });

    m_pages->setDecorate([this](PageDocument& doc) {
        m_settingsUi->decoratePage(doc);
        if (m_compose) {
            m_compose->decoratePage(doc);
        }
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
    registerComposeCommands(*m_commands, *m_compose);
    m_compose->setApplyFn([this]() { applySettings(true); });
    m_compose->setNotifyFn([this](const QString& msg) { notifyStatus(msg); });
    connect(m_speech.get(), &SpeechEngine::historyReady, this,
            [this](const QString& phrase, const QString& backend, const QString& modelId,
                   const QString& voiceId, const QString& mpegPath) {
                if (m_compose) {
                    m_compose->onHistoryReady(phrase, backend, modelId, voiceId, mpegPath);
                }
            });
    connect(m_speech.get(), &SpeechEngine::statusChanged, this, [this]() {
        if (m_pages) {
            m_pages->refreshDecorated();
            m_pages->refreshActive();
        }
    });
    connect(m_speech.get(), &SpeechEngine::notify, this,
            [this](const QString& msg) { notifyStatus(msg); });
    connect(m_eleven.get(), &ElevenClient::keyValidated, this,
            [this](bool ok, const QString& err) {
                (void)m_settingsUi->onSpeechKeyValidated(ok, err);
                if (ok && m_compose) {
                    m_compose->onCatalogReady(true, {});
                }
            });
    connect(m_eleven.get(), &ElevenClient::catalogReady, this,
            [this](bool ok, const QString& err) {
                if (m_compose) {
                    m_compose->onCatalogReady(ok, err);
                }
                if (!ok && !err.isEmpty()) {
                    notifyStatus(err);
                }
            });

    connect(m_lookToMaps.get(), &LookToMaps::enabledChanged, this,
            [this](LookToDest, bool) { refreshActiveIndicators(); });
    connect(m_comboMouse.get(), &ComboMouse::enabledChanged, this,
            [this](bool) { refreshActiveIndicators(); });
    connect(m_lookToMaps.get(), &LookToMaps::outputSuspendedChanged, this,
            [this](LookToDest, bool) { refreshActiveIndicators(); });
    connect(m_lookToMaps.get(), &LookToMaps::maxSpeedChanged, this,
            [this](LookToDest dest, double n) {
                m_settings.lookToMap(dest).maxSpeed = n;
                applySettings(true);
            });
    connect(m_lookToMaps.get(), &LookToMaps::axisModeChanged, this,
            [this](LookToDest dest, LtsScrollMode mode) {
                if (m_settings.lookToMap(dest).axisMode == mode) {
                    return;
                }
                m_settings.lookToMap(dest).axisMode = mode;
                applySettings(true);
            });
    connect(m_mouseDwellMove.get(), &MouseDwellMove::armedChanged, this,
            [this](bool) { refreshActiveIndicators(); });
    connect(m_magnifier.get(), &MagnifierOverlay::enabledChanged, this,
            [this](bool) { refreshActiveIndicators(); });
    connect(m_mouseAssist.get(), &MouseAssistState::holdsChanged, this,
            [this]() { refreshActiveIndicators(); });
    connect(m_keyState.get(), &KeyStateManager::stateChanged, this, [this]() {
        if (m_pages) {
            m_pages->setShiftHeld(m_keyState->isHeld(QStringLiteral("shift")));
        }
        refreshActiveIndicators();
    });
    connect(m_mouseAssist.get(), &MouseAssistState::amountsChanged, this,
            [this]() { refreshMouseAmountLabels(); });
    connect(m_gazeReticle.get(), &GazeReticle::enabledChanged, this,
            [this](bool) { refreshActiveIndicators(); });
    connect(m_gazeMouseFollow.get(), &GazeMouseFollow::enabledChanged, this,
            [this](bool) { refreshActiveIndicators(); });
    connect(m_actionLoops.get(), &ActionLoopService::loopsChanged, this,
            [this]() { refreshActiveIndicators(); });
    connect(this, &GazerServices::settingsChanged, this, [this]() { refreshActiveIndicators(); });
    connect(m_pages.get(), &PageSession::sessionChanged, this, [this]() {
        refreshActiveIndicators();
        if (m_compose) {
            m_compose->onSessionChanged();
        }
    });

    applySettings(false);
    refreshActiveIndicators();
    return true;
}

void GazerServices::bindActionDispatch(ActionDispatchFn dispatch)
{
    auto gated = wrapPageAimGate(m_pages.get(), m_mouseDwellMove.get(), std::move(dispatch));
    m_pages->setDispatch(gated);
    m_actionLoops->setDispatchFn(
        [gated](const QVector<PageAction>& acts, const QString& pageId) {
            gated(acts, pageId, {});
        });
}

ActiveStateContext GazerServices::activeStateContext() const
{
    ActiveStateContext ctx;
    ctx.settings = &m_settings;
    ctx.lookToMaps = m_lookToMaps.get();
    ctx.comboMouse = m_comboMouse.get();
    ctx.mouseDwellMove = m_mouseDwellMove.get();
    ctx.magnifier = m_magnifier.get();
    ctx.gazeReticle = m_gazeReticle.get();
    ctx.gazeMouseFollow = m_gazeMouseFollow.get();
    ctx.mouseAssist = m_mouseAssist.get();
    ctx.keyState = m_keyState.get();
    ctx.actionLoops = m_actionLoops.get();
    ctx.settingsUi = m_settingsUi.get();
    ctx.speechEngine = m_speech.get();
    ctx.composeUi = m_compose.get();
    ctx.dwellSuspended = isDwellSuspended();
    ctx.trackerLostMouse = m_trackerLostMouse;
    return ctx;
}

void GazerServices::setTrackerLostMouse(bool on)
{
    if (m_trackerLostMouse == on) {
        return;
    }
    m_trackerLostMouse = on;
    if (m_pages) {
        m_pages->setProp(QStringLiteral("trackerLostMouse"), on);
    }
    refreshActiveIndicators();
}

void GazerServices::setDwellSuspended(bool on)
{
    if (m_pages) {
        m_pages->setDwellSuspended(on);
    }
    if (m_comboMouse) {
        m_comboMouse->setPaused(on);
    }
    if (m_mouseDwellMove) {
        m_mouseDwellMove->setPaused(on);
    }
    if (on && m_mouseAssist) {
        m_mouseAssist->releaseAllHolds();
    }
    if (on && m_keyState) {
        QString ignored;
        (void)m_keyState->releaseAll(&ignored);
    }
}

bool GazerServices::isDwellSuspended() const
{
    return m_pages && m_pages->isDwellSuspended();
}

void GazerServices::stopAssistOutput()
{
    if (m_actionLoops) {
        m_actionLoops->stopAll();
    }
    QString ignored;
    if (m_keyState) {
        (void)m_keyState->releaseAll(&ignored);
    }
    if (m_mouseAssist) {
        m_mouseAssist->releaseAllHolds();
    }
    if (m_lookToMaps) {
        m_lookToMaps->disableAll();
    }
    if (m_comboMouse) {
        m_comboMouse->setEnabled(false);
    }
    if (m_mouseDwellMove) {
        m_mouseDwellMove->setArmed(false);
    }
    if (m_magnifier) {
        m_magnifier->setEnabledLens(false);
    }
    if (m_gazeMouseFollow) {
        m_gazeMouseFollow->setEnabled(false);
    }
    if (m_input) {
        (void)m_input->gamepad().resetNeutral(&ignored);
    }
}

void GazerServices::panicReset()
{
    stopAssistOutput();
    if (m_pages) {
        m_pages->closeAttached();
    }
    setDwellSuspended(false);
}

void GazerServices::setInjectPaused(bool on)
{
    if (on) {
        QString ignored;
        if (m_input) {
            (void)m_input->gamepad().resetNeutral(&ignored);
        }
        if (m_keyState) {
            (void)m_keyState->releaseAll(&ignored);
        }
        if (m_mouseAssist) {
            m_mouseAssist->releaseAllHolds();
        }
    }
    InjectGate::setPaused(on);
    if (m_lookToMaps) {
        m_lookToMaps->setOutputPaused(on);
    }
    if (m_headPose) {
        m_headPose->setPaused(on);
    }
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
    setScreenCaptureMode(m_settings.screenCapture);
    applyScreenCapturePolicy();
    if (m_secrets) {
        m_settings.elevenApiKeySet = m_secrets->hasKey();
    }
    if (m_tts) {
        QString voiceErr;
        (void)m_tts->setVoiceToken(m_settings.sapiVoiceToken, &voiceErr);
    }

    if (m_pages) {
        m_pages->setAutoCollapseMain(m_settings.autoCollapseMain);
        m_pages->setLayoutAutoClose(m_settings.layoutAutoClose, m_settings.layoutAutoCloseIdleMs,
                                    m_settings.layoutAutoCloseFadeMs);
    }

    ProgressVisuals boardPv;
    boardPv.style = m_settings.progress;
    boardPv.progressColor = m_settings.colorKey(QStringLiteral("progressColor"));
    boardPv.fillColor = m_settings.colorKey(QStringLiteral("progressFillColor"));
    boardPv.flashCustom = m_settings.flashCustom;
    boardPv.flashForegroundOpacity = m_settings.flashForegroundOpacity;
    boardPv.flashColor = m_settings.colorKey(QStringLiteral("flashColor"));
    boardPv.flashMs = m_settings.flashMs;
    boardPv.hoverBorder = m_settings.resolvedHoverBorder();
    boardPv.hoverBorderWidth = double(m_settings.hoverBorderWeight);
    if (m_pages) {
        m_pages->setThemeChromeVisibility(m_settings.hoverCustom, m_settings.flashCustom);
        m_pages->setProgressVisuals(boardPv);
        m_pages->setTheme(m_settings.resolvedTheme());
        m_pages->setDwellTiming(m_settings.dwellSequence, m_settings.rapidDwellSequence,
                               m_settings.dwellGraceMs, m_settings.scanGraceMs);
    }

    m_mouseDwellMove->setDwellMs(m_settings.mouseMoveDwellMs);
    m_mouseDwellMove->setGateGraceMs(m_settings.dwellGraceMs);
    m_mouseDwellMove->setMagPickDwellMs(m_settings.magPickDwellMs);
    m_mouseDwellMove->setMagPickStyle(m_settings.magPickStyle);
    m_mouseDwellMove->setMousePickStyle(m_settings.mousePickStyle);
    m_mouseDwellMove->setSelectTimeoutMs(m_settings.mouseMoveSelectTimeoutMs);
    m_mouseDwellMove->setMagPickEnabled(m_settings.mouseMoveMagPick);
    m_mouseDwellMove->setMagPickCenterOnDwell(m_settings.mouseMoveMagPickCenterOnDwell);
    m_mouseDwellMove->setMagPickFullScreen(m_settings.mouseMoveMagPickFullScreen);
    m_mouseDwellMove->setForesightEnabled(m_settings.mouseMoveForesight);
    m_mouseDwellMove->setForesightDwellMs(m_settings.mouseMoveForesightDwellMs);
    m_mouseDwellMove->setForesightHoldMs(m_settings.mouseMoveForesightHoldMs);
    m_mouseDwellMove->setForesightSecondZoom(m_settings.mouseMoveForesightSecondZoom);
    m_mouseDwellMove->setPickZoom(m_settings.pickZoom);
    m_mouseDwellMove->setPickWindowPx(m_settings.pickWindowPx);
    m_mouseDwellMove->setPickWindowRound(m_settings.pickWindowRound);
    ProgressVisuals mousePv = boardPv;
    mousePv.style = m_settings.mouseProgress;
    m_mouseDwellMove->setProgressVisuals(mousePv);
    m_gazeReticle->setColor(boardPv.progressColor);
    m_magnifier->setAccent(boardPv.progressColor);
    m_lookToMaps->setAccent(boardPv.progressColor);
    for (int i = 0; i < kLookToDestCount; ++i) {
        const auto dest = LookToDest(i);
        m_lookToMaps->applyConfig(dest, m_settings.lookToMap(dest));
    }

    m_comboMouse->setAccent(boardPv.progressColor);
    const ThemeColors theme = m_settings.resolvedTheme();
    const QColor comboInner = m_settings.colorKey(QStringLiteral("comboInnerColor"));
    const QColor comboOuter = m_settings.colorKey(QStringLiteral("comboOuterColor"));
    m_lookToMaps->setTheme(theme);
    m_lookToMaps->setPieRadii(m_settings.comboInnerRadiusPx, m_settings.comboSharedRadiusPx,
                             m_settings.comboOuterRadiusPx);
    m_lookToMaps->setAnnulusColors(comboInner, comboOuter);
    m_lookToMaps->setScanGraceMs(m_settings.scanGraceMs);
    m_lookToMaps->setDwellGraceMs(m_settings.dwellGraceMs);
    m_lookToMaps->setDwellSequence(m_settings.dwellSequence);
    m_comboMouse->setTheme(theme);
    m_comboMouse->setRadii(m_settings.comboInnerRadiusPx, m_settings.comboSharedRadiusPx,
                          m_settings.comboOuterRadiusPx);
    m_comboMouse->setAnnulusColors(comboInner, comboOuter);
    m_comboMouse->setScanGraceMs(m_settings.scanGraceMs);
    m_comboMouse->setDwellGraceMs(m_settings.dwellGraceMs);
    m_comboMouse->setDwellSequence(m_settings.dwellSequence);

    m_magnifier->setZoom(m_settings.magZoom);
    m_magnifier->setLensSize(m_settings.magLensSize);
    m_magnifier->setFollowProfile(m_settings.magFollowProfile);
    m_gazeReticle->setFollowProfile(m_settings.magFollowProfile);
    m_gazeMouseFollow->setFollowProfile(m_settings.magFollowProfile);

    if (m_headPose) {
        m_headPose->setEnabled(m_settings.headPoseEnabled);
        m_headPose->setOrigin(m_settings.headPoseOrigin, m_settings.headPoseOriginSet);
        m_headPose->setMaps(m_settings.headPoseMaps);
    }

    if (m_pages) {
        m_pages->refreshDecorated();
    }
    if (m_compose) {
        m_compose->syncFromSettings();
    }

    if (persist) {
        QString err;
        if (!m_settings.saveToFile(AppSettings::defaultFilePath(), &err)) {
            GAZER_WARN << "Failed to save settings:" << err;
        }
    }
    emit settingsChanged();
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
    m_assistCmdCtx->lookToMaps = m_lookToMaps.get();
    m_assistCmdCtx->comboMouse = m_comboMouse.get();
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

    auto cycleMod = [this](const QString& key) {
        return [this, key](QString* error) { return m_keyState->cycle(key, error); };
    };
    m_commands->registerBuiltin(QStringLiteral("leftCtrl"), cycleMod(QStringLiteral("Control")));
    m_commands->registerBuiltin(QStringLiteral("rightCtrl"), cycleMod(QStringLiteral("RControl")));
    m_commands->registerBuiltin(QStringLiteral("leftAlt"), cycleMod(QStringLiteral("Alt")));
    m_commands->registerBuiltin(QStringLiteral("rightAlt"), cycleMod(QStringLiteral("RAlt")));
    m_commands->registerBuiltin(QStringLiteral("leftWin"), cycleMod(QStringLiteral("LWin")));
    m_commands->registerBuiltin(QStringLiteral("rightWin"), cycleMod(QStringLiteral("RWin")));
    m_commands->registerBuiltin(QStringLiteral("leftShift"), cycleMod(QStringLiteral("Shift")));
    m_commands->registerBuiltin(QStringLiteral("rightShift"), cycleMod(QStringLiteral("RShift")));
    m_commands->registerBuiltin(QStringLiteral("releaseModifiers"),
                                 [this](QString* error) { return m_keyState->releaseAll(error); });

    auto clickAtCursor = [this](const QString& button) {
        return [this, button](QString* error) {
            InputOutput o;
            o.type = InputOutput::Type::MouseClick;
            o.button = button;
            return m_input->execute(o, error);
        };
    };
    m_commands->registerBuiltin(QStringLiteral("mouseLeftClick"),
                                clickAtCursor(QStringLiteral("left")));
    m_commands->registerBuiltin(QStringLiteral("mouseRightClick"),
                                clickAtCursor(QStringLiteral("right")));
    m_commands->registerBuiltin(QStringLiteral("mouseMiddleClick"),
                                clickAtCursor(QStringLiteral("middle")));
    // Shared sticky policy: stop layout actionLoops + assist sticky (gaze click loop).
    m_commands->registerBuiltin(QStringLiteral("stopAllActionLoops"), [this](QString*) {
        if (m_actionLoops) {
            m_actionLoops->stopAll();
        }
        if (m_mouseDwellMove && m_mouseDwellMove->isClickLoop()) {
            m_mouseDwellMove->setArmed(false);
        }
        if (m_mouseAssist) {
            m_mouseAssist->releaseAllHolds();
        }
        if (m_keyState) {
            QString ignored;
            (void)m_keyState->releaseAll(&ignored);
        }
        refreshActiveIndicators();
        notifyStatus(QStringLiteral("All action loops stopped"));
        return true;
    });
}

} // namespace gazer
