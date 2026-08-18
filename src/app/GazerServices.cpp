#include "app/GazerServices.h"

#include "app/ActiveStateResolver.h"
#include "app/SettingsUi.h"
#include "assist/AssistCommands.h"
#include "input/InputTypes.h"
#include "utils/Log.h"

#include <QColor>
#include <QPoint>
#include <QtGlobal>

namespace gazer {

GazerServices::GazerServices(QObject* parent)
    : QObject(parent)
{
}

GazerServices::~GazerServices() = default;

bool GazerServices::initialize(const QString& layoutsDir, const QString& mappingPath,
                               QString* error)
{
    Q_UNUSED(error);

    m_catalog = std::make_unique<LayoutManager>();
    m_instances = std::make_unique<LayoutInstanceManager>(*m_catalog);
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
    m_scripts = std::make_unique<ScriptHost>(*m_phrases, *m_commands, *m_input, *m_instances);

    m_catalog->setLayoutsDirectory(layoutsDir);
    GAZER_INFO << "Layouts available:" << m_catalog->scanDirectory();

    QString mapErr;
    if (!m_mapping->loadProfileFile(mappingPath, &mapErr)) {
        GAZER_WARN << "Mapping profile not loaded:" << mapErr;
    }

    QString setErr;
    if (!m_settings.loadFromFile(AppSettings::defaultFilePath(), &setErr)) {
        GAZER_INFO << "Using default settings (" << setErr << ")";
        m_settings = AppSettings::defaults();
    }

    m_settingsUi =
        std::make_unique<SettingsUi>(m_settings, *m_instances, *m_catalog, *m_commands);
    m_settingsUi->setApplyFn([this](bool persist) { applySettings(persist); });
    m_settingsUi->setNotifyFn([this](const QString& msg) { notifyStatus(msg); });
    m_settingsUi->setMutateFn(
        [this](const std::function<void(AppSettings&)>& mutator, const QString& status) {
            mutateAndApply(mutator, status);
        });
    m_settingsUi->setResetFn([this]() { resetSettingsToDefaults(); });

    m_instances->setDocumentDecorator([this](LayoutDocument& doc) {
        m_settingsUi->decorateDocument(doc);
        decorateMouseAmountLabels(doc);
    });
    m_instances->setInstanceTeardownHook(
        [this](const QString& instanceId) { m_actionLoops->stopInstance(instanceId); });
    connect(m_instances.get(), &LayoutInstanceManager::dwellEngagementEnded, this,
            [this](const QString& instanceId, const QString& itemId) {
                m_actionLoops->clearEngageLatch(instanceId, itemId);
            });

    registerDomainCommands();
    m_settingsUi->registerCommands();

    connect(m_lookToScroll.get(), &LookToScroll::enabledChanged, this,
            [this](bool) { refreshActiveIndicators(); });
    connect(m_lookToScroll.get(), &LookToScroll::scrollSuspendedChanged, this,
            [this](bool) { refreshActiveIndicators(); });
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
    connect(m_instances.get(), &LayoutInstanceManager::sessionChanged, this,
            [this]() { refreshActiveIndicators(); });

    applySettings(false);
    refreshActiveIndicators();
    return true;
}

void GazerServices::bindActionDispatch(ActionDispatchFn dispatch)
{
    m_actionLoops->setDispatchFn(dispatch);
    m_instances->setLifecycleRunner(std::move(dispatch));
}

ActiveStateContext GazerServices::activeStateContext() const
{
    ActiveStateContext ctx;
    ctx.settings = &m_settings;
    ctx.instances = m_instances.get();
    ctx.lookToScroll = m_lookToScroll.get();
    ctx.mouseDwellMove = m_mouseDwellMove.get();
    ctx.magnifier = m_magnifier.get();
    ctx.gazeReticle = m_gazeReticle.get();
    ctx.gazeMouseFollow = m_gazeMouseFollow.get();
    ctx.mouseAssist = m_mouseAssist.get();
    ctx.actionLoops = m_actionLoops.get();
    ctx.settingsUi = m_settingsUi.get();
    return ctx;
}

void GazerServices::refreshActiveIndicators()
{
    if (!m_instances) {
        return;
    }
    m_instances->refreshActiveIndicators(
        [this](const QString& key) { return resolveActiveState(activeStateContext(), key); });
}

void GazerServices::decorateMouseAmountLabels(LayoutDocument& doc) const
{
    if (!m_mouseAssist) {
        return;
    }
    const QString move = QStringLiteral("Step %1 px").arg(m_mouseAssist->moveAmountPx());
    const QString scroll = QStringLiteral("Scroll ×%1").arg(m_mouseAssist->scrollNotches());
    for (LayoutItem& item : doc.items) {
        const QString name = item.action.name;
        if (name == QLatin1String("cycleMouseMoveAmount")) {
            item.label = move;
        } else if (name == QLatin1String("cycleMouseScrollAmount")) {
            item.label = scroll;
        }
    }
}

void GazerServices::refreshMouseAmountLabels()
{
    if (!m_instances || !m_mouseAssist) {
        return;
    }
    for (LayoutInstance* inst : m_instances->instances()) {
        if (!inst) {
            continue;
        }
        bool has = false;
        for (const LayoutItem& item : inst->document().items) {
            if (item.action.name == QLatin1String("cycleMouseMoveAmount")
                || item.action.name == QLatin1String("cycleMouseScrollAmount")) {
                has = true;
                break;
            }
        }
        if (!has) {
            continue;
        }
        inst->mutateItems([this](LayoutItem& item) {
            if (item.action.name == QLatin1String("cycleMouseMoveAmount")) {
                item.label = QStringLiteral("Step %1 px").arg(m_mouseAssist->moveAmountPx());
            } else if (item.action.name == QLatin1String("cycleMouseScrollAmount")) {
                item.label = QStringLiteral("Scroll ×%1").arg(m_mouseAssist->scrollNotches());
            }
        });
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

    m_instances->setAutoCollapseMain(m_settings.autoCollapseMain);
    m_instances->setAutoCloseDefaults(m_settings.layoutAutoClose, m_settings.layoutAutoCloseIdleMs,
                                      m_settings.layoutAutoCloseFadeMs);
    m_instances->applyGlobalDwellOverride(m_settings.dwellSequence, m_settings.dwellGraceMs,
                                          m_settings.scanGraceMs);

    ProgressVisuals boardPv;
    boardPv.radial = m_settings.progressRadial;
    boardPv.fillBackground = m_settings.progressFill;
    boardPv.border = m_settings.progressBorder;
    boardPv.progressColor = m_settings.colorKey(QStringLiteral("progressColor"));
    boardPv.fillColor = m_settings.colorKey(QStringLiteral("progressFillColor"));
    boardPv.borderColor = m_settings.colorKey(QStringLiteral("progressBorderColor"));
    boardPv.flashUseForeground = m_settings.flashUseForeground;
    boardPv.flashForegroundOpacity = m_settings.flashForegroundOpacity;
    boardPv.flashColor = m_settings.colorKey(QStringLiteral("flashColor"));
    boardPv.flashMs = m_settings.flashMs;
    m_instances->applyProgressVisuals(boardPv);
    m_instances->applyTheme(m_settings.resolvedTheme());

    m_mouseDwellMove->setDwellMs(m_settings.mouseMoveDwellMs);
    m_mouseDwellMove->setMagPickDwellMs(m_settings.magPickDwellMs);
    m_mouseDwellMove->setMagPickStyle(m_settings.magPickStyle);
    m_mouseDwellMove->setMousePickStyle(m_settings.mousePickStyle);
    m_mouseDwellMove->setSelectTimeoutMs(m_settings.mouseMoveSelectTimeoutMs);
    m_mouseDwellMove->setMagPickEnabled(m_settings.mouseMoveMagPick);
    m_mouseDwellMove->setMagPickCenterOnDwell(m_settings.mouseMoveMagPickCenterOnDwell);
    m_mouseDwellMove->setMagPickZoom(m_settings.magZoom);
    ProgressVisuals mousePv = boardPv;
    mousePv.radial = m_settings.mouseProgressRadial;
    mousePv.fillBackground = m_settings.mouseProgressFill;
    mousePv.border = m_settings.mouseProgressBorder;
    m_mouseDwellMove->setProgressVisuals(mousePv);
    m_gazeReticle->setColor(boardPv.progressColor);
    m_magnifier->setAccent(boardPv.progressColor);
    m_lookToScroll->setAccent(boardPv.progressColor);

    m_lookToScroll->setDeadzonePx(m_settings.ltsDeadzonePx);
    m_lookToScroll->setFalloffPx(m_settings.ltsFalloffPx);
    m_lookToScroll->setMaxNotchesPerSec(m_settings.ltsMaxNotchesPerSec);
    m_lookToScroll->setAccelPerSec(m_settings.ltsAccelPerSec);
    m_lookToScroll->setCenterDwellMs(m_settings.ltsCenterDwellMs);

    m_magnifier->setZoom(m_settings.magZoom);
    m_magnifier->setLensSize(m_settings.magLensSize);
    m_magnifier->setFollowProfile(m_settings.magFollowProfile);
    m_gazeReticle->setFollowProfile(m_settings.magFollowProfile);
    m_gazeMouseFollow->setFollowProfile(m_settings.magFollowProfile);

    m_mapping->setSpeakAlsoType(m_settings.speakAlsoType);

    if (m_settingsUi) {
        m_settingsUi->refreshOpenBoards();
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
    m_assistCmdCtx->instances = m_instances.get();
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
