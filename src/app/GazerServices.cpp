#include "app/GazerServices.h"

#include "assist/AssistCommands.h"
#include "input/InputTypes.h"
#include "ui/Theme.h"
#include "utils/Log.h"

#include <QColor>
#include <QPoint>
#include <QtGlobal>
#include <functional>

namespace gazer {

namespace {

LayoutItem makeItem(const QString& id, const QString& label, int row, int col,
                    LayoutAction::Type type, const QString& payload,
                    const QColor& bg = QColor(48, 54, 72), int colSpan = 1,
                    bool interactive = true, const QString& caption = {},
                    const QString& settingKey = {})
{
    LayoutItem it;
    it.id = id;
    it.label = label;
    it.caption = caption;
    it.settingKey = settingKey;
    it.interactive = interactive;
    it.row = row;
    it.col = col;
    it.colSpan = colSpan;
    it.style.background = bg;
    it.style.foreground = QColor(244, 246, 252);
    if (interactive && type != LayoutAction::Type::Unknown) {
        it.action.type = type;
        if (type == LayoutAction::Type::Command) {
            it.action.name = payload;
        } else if (type == LayoutAction::Type::LoadLayout
                   || type == LayoutAction::Type::OpenLayout) {
            it.action.layoutId = payload;
        } else if (type == LayoutAction::Type::TypeText) {
            it.action.text = payload;
        }
    }
    return it;
}

LayoutItem makeLabel(const QString& id, const QString& label, int row, int col, int colSpan = 1,
                     const QString& caption = {}, const QString& settingKey = {})
{
    return makeItem(id, label, row, col, LayoutAction::Type::Unknown, {}, QColor(0, 0, 0, 0),
                    colSpan, /*interactive=*/false, caption, settingKey);
}

} // namespace

GazerServices::GazerServices(QObject* parent)
    : QObject(parent)
{
}

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

    m_instances->setDocumentDecorator(
        [this](LayoutDocument& doc) { decorateSettingsDocument(doc); });
    m_instances->setInstanceTeardownHook(
        [this](const QString& instanceId) { m_actionLoops->stopInstance(instanceId); });
    connect(m_instances.get(), &LayoutInstanceManager::dwellEngagementEnded, this,
            [this](const QString& instanceId, const QString& itemId) {
                m_actionLoops->clearEngageLatch(instanceId, itemId);
            });

    registerDomainCommands();
    registerSettingsCommands();

    // Keep accent indicators in sync with toggles / holds.
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

bool GazerServices::resolveActiveState(const QString& key) const
{
    if (key == QLatin1String("dwellSuspend") || key == QLatin1String("dwell.suspended")) {
        return m_instances && m_instances->isDwellSuspended();
    }
    if (key == QLatin1String("lookToScroll")) {
        return m_lookToScroll && m_lookToScroll->isEnabled();
    }
    if (key == QLatin1String("lookToScroll.suspended")) {
        return m_lookToScroll && m_lookToScroll->isScrollSuspended();
    }
    if (key == QLatin1String("mouseDwellMove")) {
        return m_mouseDwellMove && m_mouseDwellMove->isArmed()
               && !m_mouseDwellMove->isClickLoop();
    }
    if (key == QLatin1String("loop.gazeClick") || key == QLatin1String("mouseDwellClickLoop")) {
        return m_mouseDwellMove && m_mouseDwellMove->isClickLoop();
    }
    if (key == QLatin1String("mouseMoveMagPick")) {
        return m_settings.mouseMoveMagPick;
    }
    if (key == QLatin1String("mouseMoveMagPickCenter")) {
        return m_settings.mouseMoveMagPickCenterOnDwell;
    }
    if (key == QLatin1String("magnifier")) {
        return m_magnifier && m_magnifier->isEnabledLens();
    }
    if (key == QLatin1String("gazeReticle")) {
        return m_gazeReticle && m_gazeReticle->isEnabled();
    }
    if (key == QLatin1String("gazeMouseFollow")) {
        return m_gazeMouseFollow && m_gazeMouseFollow->isEnabled();
    }
    if (m_actionLoops && m_actionLoops->isActiveState(key)) {
        return true;
    }
    if (key == QLatin1String("mouse.leftHold")) {
        return m_mouseAssist && m_mouseAssist->isLeftHeld();
    }
    if (key == QLatin1String("mouse.rightHold")) {
        return m_mouseAssist && m_mouseAssist->isRightHeld();
    }
    if (key == QLatin1String("mouse.middleHold")) {
        return m_mouseAssist && m_mouseAssist->isMiddleHeld();
    }
    if (key == QLatin1String("mouse.anyHold")) {
        return m_mouseAssist && m_mouseAssist->anyButtonHeld();
    }
    if (key == QLatin1String("lts.placeCursorFirst")) {
        return m_settings.ltsPlaceCursorFirst;
    }
    if (key == QLatin1String("setting.progressRadial")) {
        return m_settings.progressRadial;
    }
    if (key == QLatin1String("setting.progressFill")) {
        return m_settings.progressFill;
    }
    if (key == QLatin1String("setting.progressBorder")) {
        return m_settings.progressBorder;
    }
    if (key == QLatin1String("setting.mouseProgressRadial")) {
        return m_settings.mouseProgressRadial;
    }
    if (key == QLatin1String("setting.mouseProgressFill")) {
        return m_settings.mouseProgressFill;
    }
    if (key == QLatin1String("setting.mouseProgressBorder")) {
        return m_settings.mouseProgressBorder;
    }
    if (key == QLatin1String("setting.flashOnComplete")) {
        return m_settings.flashOnComplete;
    }
    if (key == QLatin1String("setting.autoCollapseMain")) {
        return m_settings.autoCollapseMain;
    }
    if (key == QLatin1String("setting.startDocked")) {
        return m_settings.startDocked;
    }
    if (key == QLatin1String("setting.speakAlsoType")) {
        return m_settings.speakAlsoType;
    }
    if (key == QLatin1String("setting.magFollow.0")) {
        return m_settings.magFollowProfile == 0;
    }
    if (key == QLatin1String("setting.magFollow.1")) {
        return m_settings.magFollowProfile == 1;
    }
    if (key == QLatin1String("setting.magFollow.2")) {
        return m_settings.magFollowProfile == 2;
    }
    if (key == QLatin1String("setting.tracker.0")) {
        return m_settings.trackerPref == 0;
    }
    if (key == QLatin1String("setting.tracker.1")) {
        return m_settings.trackerPref == 1;
    }
    if (key == QLatin1String("setting.theme.dark")) {
        return m_settings.themeMode == ThemeMode::Dark;
    }
    if (key == QLatin1String("setting.theme.light")) {
        return m_settings.themeMode == ThemeMode::Light;
    }
    if (key == QLatin1String("setting.theme.custom")) {
        return m_settings.themeMode == ThemeMode::Custom;
    }
    return false;
}

void GazerServices::refreshActiveIndicators()
{
    if (!m_instances) {
        return;
    }
    m_instances->refreshActiveIndicators(
        [this](const QString& key) { return resolveActiveState(key); });
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

void GazerServices::decorateSettingsDocument(LayoutDocument& doc) const
{
    if (!doc.id.startsWith(QLatin1String("example_settings"))) {
        return;
    }
    doc.uiStyle = LayoutUiStyle::Fluent;
    for (LayoutItem& item : doc.items) {
        // Live value text on non-interactive value cells (settingKey + role value/label).
        if (!item.settingKey.isEmpty() && !item.interactive) {
            item.label = m_settings.displayValue(item.settingKey);
        }
        // Description labels may use settingKey only for help lookup via id pattern.
        if (!item.interactive && item.caption.isEmpty() && !item.settingKey.isEmpty()
            && item.id.contains(QLatin1String("desc"))) {
            item.label = AppSettings::settingDescription(item.settingKey);
        }
    }
}

void GazerServices::refreshOpenSettingsBoards()
{
    for (LayoutInstance* inst : m_instances->instances()) {
        if (!inst) {
            continue;
        }
        const QString lid = inst->layoutId();
        if (!lid.startsWith(QLatin1String("example_settings"))) {
            continue;
        }
        if (m_numpadActive && inst->instanceId() == m_numpadInstanceId) {
            continue; // don't clobber the live numpad
        }
        const LayoutDocument* src = m_catalog->document(lid);
        if (!src) {
            continue;
        }
        LayoutDocument copy = *src;
        decorateSettingsDocument(copy);
        inst->setDocument(copy);
        inst->setGlobalDwellOverride(m_settings.dwellSequence, m_settings.dwellGraceMs);
    }
}

void GazerServices::applySettings(bool persist)
{
    m_settings.clamp();

    m_instances->setAutoCollapseMain(m_settings.autoCollapseMain);
    m_instances->applyGlobalDwellOverride(m_settings.dwellSequence, m_settings.dwellGraceMs);

    ProgressVisuals boardPv;
    boardPv.radial = m_settings.progressRadial;
    boardPv.fillBackground = m_settings.progressFill;
    boardPv.border = m_settings.progressBorder;
    boardPv.progressColor = AppSettings::parseColor(m_settings.progressColor);
    boardPv.fillColor = AppSettings::parseColor(m_settings.progressFillColor, QColor(0, 180, 220, 70));
    boardPv.borderColor = AppSettings::parseColor(m_settings.progressBorderColor);
    boardPv.flashOnComplete = m_settings.flashOnComplete;
    boardPv.flashBorderColor = AppSettings::parseColor(m_settings.flashBorderColor, Qt::white);
    boardPv.flashFillColor =
        AppSettings::parseColor(m_settings.flashFillColor, QColor(0, 220, 255, 120));
    boardPv.flashMs = m_settings.flashMs;
    m_instances->applyProgressVisuals(boardPv);
    m_instances->applyTheme(m_settings.resolvedTheme());

    m_mouseDwellMove->setDwellMs(m_settings.mouseMoveDwellMs);
    m_mouseDwellMove->setMagPickEnabled(m_settings.mouseMoveMagPick);
    m_mouseDwellMove->setMagPickCenterOnDwell(m_settings.mouseMoveMagPickCenterOnDwell);
    m_mouseDwellMove->setMagPickZoom(m_settings.magZoom);
    ProgressVisuals mousePv;
    mousePv.radial = m_settings.mouseProgressRadial;
    mousePv.fillBackground = m_settings.mouseProgressFill;
    mousePv.border = m_settings.mouseProgressBorder;
    mousePv.progressColor =
        AppSettings::parseColor(m_settings.mouseProgressColor, QColor(255, 200, 40));
    mousePv.fillColor =
        AppSettings::parseColor(m_settings.mouseProgressFillColor, QColor(255, 200, 40, 64));
    mousePv.borderColor =
        AppSettings::parseColor(m_settings.mouseProgressBorderColor, QColor(255, 200, 40));
    m_mouseDwellMove->setProgressVisuals(mousePv);

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

    refreshOpenSettingsBoards();

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

LayoutDocument GazerServices::buildNumpadDocument() const
{
    // Layout:
    //  title (label)
    //  description (label)
    //  input box (label / display)
    //  7 8 9 ⌫
    //  4 5 6 reset
    //  1 2 3 clear
    //  0 .   −
    //  Save  Cancel
    LayoutDocument doc;
    doc.schemaVersion = 1;
    doc.id = QStringLiteral("settings_numpad_live");
    doc.name = AppSettings::settingTitle(m_numpadKey);
    doc.description = QStringLiteral("Enter a value, then Save");
    doc.uiStyle = LayoutUiStyle::Fluent;
    doc.grid.columns = 4;
    doc.grid.rows = 8;
    doc.grid.gapPx = 10;
    doc.grid.marginPx = 20;
    doc.dwell.enabled = true;
    doc.dwell.msSequence = m_settings.dwellSequence;
    doc.dwell.ms = m_settings.dwellSequence.isEmpty() ? 650 : m_settings.dwellSequence.first();
    doc.placement.specified = true;
    doc.placement.anchor = LayoutWindowPlacement::Anchor::Center;
    doc.placement.widthPx = 520;
    doc.placement.heightPx = 720;

    doc.items.push_back(makeLabel(QStringLiteral("title"), AppSettings::settingTitle(m_numpadKey),
                                  0, 0, 4,
                                  QStringLiteral("Saved: %1")
                                      .arg(m_settings.displayValue(m_numpadKey))));
    doc.items.push_back(makeLabel(QStringLiteral("desc"),
                                  AppSettings::settingDescription(m_numpadKey), 1, 0, 4));
    doc.items.push_back(makeLabel(
        QStringLiteral("display"),
        m_numpadBuffer.isEmpty() ? QStringLiteral("0") : m_numpadBuffer, 2, 0, 4));

    auto key = [&](const QString& id, const QString& label, int row, int col, const QString& cmd,
                   const QColor& bg = QColor(55, 62, 82)) {
        doc.items.push_back(makeItem(id, label, row, col, LayoutAction::Type::Command, cmd, bg));
    };

    key(QStringLiteral("d7"), QStringLiteral("7"), 3, 0, QStringLiteral("settings.numpad.digit.7"));
    key(QStringLiteral("d8"), QStringLiteral("8"), 3, 1, QStringLiteral("settings.numpad.digit.8"));
    key(QStringLiteral("d9"), QStringLiteral("9"), 3, 2, QStringLiteral("settings.numpad.digit.9"));
    key(QStringLiteral("back"), QStringLiteral("⌫"), 3, 3,
        QStringLiteral("settings.numpad.backspace"), QColor(90, 60, 70));

    key(QStringLiteral("d4"), QStringLiteral("4"), 4, 0, QStringLiteral("settings.numpad.digit.4"));
    key(QStringLiteral("d5"), QStringLiteral("5"), 4, 1, QStringLiteral("settings.numpad.digit.5"));
    key(QStringLiteral("d6"), QStringLiteral("6"), 4, 2, QStringLiteral("settings.numpad.digit.6"));
    key(QStringLiteral("reset"), QStringLiteral("Reset"), 4, 3,
        QStringLiteral("settings.numpad.reset"), QColor(80, 70, 50));

    key(QStringLiteral("d1"), QStringLiteral("1"), 5, 0, QStringLiteral("settings.numpad.digit.1"));
    key(QStringLiteral("d2"), QStringLiteral("2"), 5, 1, QStringLiteral("settings.numpad.digit.2"));
    key(QStringLiteral("d3"), QStringLiteral("3"), 5, 2, QStringLiteral("settings.numpad.digit.3"));
    key(QStringLiteral("clear"), QStringLiteral("Clear"), 5, 3,
        QStringLiteral("settings.numpad.clear"), QColor(100, 70, 50));

    // Row: minus · zero · period · comma
    key(QStringLiteral("minus"), QStringLiteral("−"), 6, 0,
        QStringLiteral("settings.numpad.minus"), QColor(70, 78, 100));
    key(QStringLiteral("d0"), QStringLiteral("0"), 6, 1, QStringLiteral("settings.numpad.digit.0"));
    key(QStringLiteral("period"), QStringLiteral("."), 6, 2,
        QStringLiteral("settings.numpad.period"), QColor(70, 78, 100));
    key(QStringLiteral("comma"), QStringLiteral(","), 6, 3,
        QStringLiteral("settings.numpad.comma"), QColor(70, 78, 100));

    doc.items.push_back(makeItem(QStringLiteral("save"), QStringLiteral("Save"), 7, 0,
                                 LayoutAction::Type::Command, QStringLiteral("settings.numpad.save"),
                                 QColor(0, 120, 140), 2));
    doc.items.push_back(makeItem(QStringLiteral("cancel"), QStringLiteral("Cancel"), 7, 2,
                                 LayoutAction::Type::Command,
                                 QStringLiteral("settings.numpad.cancel"), QColor(90, 50, 55), 2));
    return doc;
}

bool GazerServices::openNumericEditor(const QString& settingKey, QString* error)
{
    if (!AppSettings::isNumericKey(settingKey)) {
        if (error) {
            *error = QStringLiteral("Not a numeric setting: %1").arg(settingKey);
        }
        return false;
    }
    auto* focused = m_instances->focusedInstance();
    if (!focused) {
        if (error) {
            *error = QStringLiteral("No focused board for numeric editor");
        }
        return false;
    }

    m_numpadActive = true;
    m_numpadInstanceId = focused->instanceId();
    m_numpadReturnLayoutId = focused->layoutId();
    m_numpadKey = settingKey;
    m_numpadBuffer = m_settings.numericBufferSeed(settingKey);

    if (!m_instances->setInstanceDocument(m_numpadInstanceId, buildNumpadDocument(), error)) {
        m_numpadActive = false;
        return false;
    }
    notifyStatus(QStringLiteral("Edit %1").arg(AppSettings::settingTitle(settingKey)));
    return true;
}

void GazerServices::refreshNumpadDisplay()
{
    if (!m_numpadActive) {
        return;
    }
    auto* inst = m_instances->instance(m_numpadInstanceId);
    if (!inst) {
        return;
    }
    const QString shown = m_numpadBuffer.isEmpty() ? QStringLiteral("0") : m_numpadBuffer;
    inst->setItemText(QStringLiteral("display"), shown, {});
}

void GazerServices::numpadAppend(const QString& ch)
{
    if (!m_numpadActive) {
        return;
    }
    // Dwell sequence allows multiple commas; single decimals allow one period.
    const bool sequenceMode = (m_numpadKey == QLatin1String("dwellMs")
                               || m_numpadKey == QLatin1String("dwellSequence"));
    if (ch == QLatin1String(".") && !sequenceMode && m_numpadBuffer.contains(QLatin1Char('.'))) {
        return;
    }
    if (ch == QLatin1String(",") && !sequenceMode) {
        return; // comma only valid for dwell sequences
    }
    if (m_numpadBuffer == QLatin1String("0") && ch != QLatin1String(".")
        && ch != QLatin1String(",")) {
        m_numpadBuffer = ch;
    } else {
        if (m_numpadBuffer.size() >= 48) {
            return;
        }
        m_numpadBuffer += ch;
    }
    refreshNumpadDisplay();
}

void GazerServices::numpadBackspace()
{
    if (!m_numpadActive || m_numpadBuffer.isEmpty()) {
        return;
    }
    m_numpadBuffer.chop(1);
    refreshNumpadDisplay();
}

void GazerServices::numpadClear()
{
    if (!m_numpadActive) {
        return;
    }
    m_numpadBuffer.clear();
    refreshNumpadDisplay();
}

void GazerServices::numpadReset()
{
    if (!m_numpadActive) {
        return;
    }
    m_numpadBuffer = m_settings.numericBufferSeed(m_numpadKey);
    refreshNumpadDisplay();
}

void GazerServices::numpadMinus()
{
    if (!m_numpadActive) {
        return;
    }
    if (m_numpadBuffer.startsWith(QLatin1Char('-'))) {
        m_numpadBuffer.remove(0, 1);
    } else {
        m_numpadBuffer.prepend(QLatin1Char('-'));
    }
    refreshNumpadDisplay();
}

bool GazerServices::numpadSave(QString* error)
{
    if (!m_numpadActive) {
        if (error) {
            *error = QStringLiteral("Numeric editor is not open");
        }
        return false;
    }
    QString err;
    if (!m_settings.applyNumericBuffer(m_numpadKey, m_numpadBuffer.isEmpty() ? QStringLiteral("0")
                                                                              : m_numpadBuffer,
                                       &err)) {
        notifyStatus(err);
        if (error) {
            *error = err;
        }
        return false;
    }
    const QString savedTitle = AppSettings::settingTitle(m_numpadKey);
    const QString savedValue = m_settings.displayValue(m_numpadKey);
    applySettings(true);

    const QString returnId = m_numpadReturnLayoutId;
    const QString instId = m_numpadInstanceId;
    m_numpadActive = false;
    m_numpadInstanceId.clear();
    m_numpadKey.clear();
    m_numpadBuffer.clear();

    QString loadErr;
    if (!m_instances->loadInto(instId, returnId, &loadErr)) {
        const LayoutDocument* src = m_catalog->document(returnId);
        if (src) {
            LayoutDocument copy = *src;
            decorateSettingsDocument(copy);
            m_instances->setInstanceDocument(instId, copy, &loadErr);
        }
    }
    notifyStatus(QStringLiteral("Saved %1 = %2").arg(savedTitle, savedValue));
    return true;
}

bool GazerServices::openColorPicker(const QString& colorKey, QString* error)
{
    if (!AppSettings::isColorKey(colorKey)) {
        if (error) {
            *error = QStringLiteral("Not a color setting");
        }
        return false;
    }
    auto* focused = m_instances->focusedInstance();
    if (!focused) {
        if (error) {
            *error = QStringLiteral("No focused board");
        }
        return false;
    }
    m_colorPickerActive = true;
    m_colorPickerInstanceId = focused->instanceId();
    m_colorPickerReturnLayoutId = focused->layoutId();
    m_colorPickerKey = colorKey;

    LayoutDocument doc;
    doc.id = QStringLiteral("settings_color_live");
    doc.name = AppSettings::settingTitle(colorKey);
    doc.description = AppSettings::settingDescription(colorKey);
    doc.uiStyle = LayoutUiStyle::Fluent;
    doc.grid.columns = 5;
    doc.grid.rows = 3;
    doc.grid.gapPx = 12;
    doc.grid.marginPx = 56;
    doc.dwell.ms = m_settings.dwellSequence.isEmpty() ? 650 : m_settings.dwellSequence.first();
    doc.placement.specified = true;
    doc.placement.anchor = LayoutWindowPlacement::Anchor::Center;
    doc.placement.widthPx = 720;
    doc.placement.heightPx = 420;

    doc.items.push_back(makeLabel(QStringLiteral("title"), AppSettings::settingTitle(colorKey), 0,
                                  0, 5, QStringLiteral("Current %1")
                                            .arg(m_settings.displayValue(colorKey))));

    // Voice-aligned sample palette (first 15 + cancel).
    doc.grid.rows = 5;
    doc.grid.columns = 5;
    doc.items.clear();
    doc.items.push_back(makeLabel(QStringLiteral("title"), AppSettings::settingTitle(colorKey), 0,
                                  0, 5, m_settings.displayValue(colorKey)));
    const QVector<QString> palette = voiceColorPalette();
    const int nSwatches = qMin(15, palette.size());
    for (int i = 0; i < nSwatches; ++i) {
        const int row = 1 + i / 5;
        const int col = i % 5;
        QString hex = palette[i];
        if (hex.startsWith(QLatin1Char('#'))) {
            hex = hex.mid(1);
        }
        doc.items.push_back(makeItem(
            QStringLiteral("c%1").arg(i), QStringLiteral("■ %1").arg(hex), row, col,
            LayoutAction::Type::Command,
            QStringLiteral("settings.color.%1.%2").arg(colorKey, hex),
            QColor(QStringLiteral("#%1").arg(hex))));
    }
    doc.items.push_back(makeItem(QStringLiteral("cancel"), QStringLiteral("Cancel"), 3, 0,
                                 LayoutAction::Type::Command,
                                 QStringLiteral("settings.color.cancel"), QColor(90, 50, 55), 5));

    if (!m_instances->setInstanceDocument(m_colorPickerInstanceId, doc, error)) {
        m_colorPickerActive = false;
        return false;
    }
    notifyStatus(QStringLiteral("Pick color for %1").arg(AppSettings::settingTitle(colorKey)));
    return true;
}

void GazerServices::closeColorPicker()
{
    if (!m_colorPickerActive) {
        return;
    }
    const QString instId = m_colorPickerInstanceId;
    const QString returnId = m_colorPickerReturnLayoutId;
    m_colorPickerActive = false;
    m_colorPickerInstanceId.clear();
    m_colorPickerKey.clear();
    QString err;
    m_instances->loadInto(instId, returnId, &err);
}

bool GazerServices::numpadCancel(QString* error)
{
    if (!m_numpadActive) {
        if (error) {
            *error = QStringLiteral("Numeric editor is not open");
        }
        return false;
    }
    const QString returnId = m_numpadReturnLayoutId;
    const QString instId = m_numpadInstanceId;
    m_numpadActive = false;
    m_numpadInstanceId.clear();
    m_numpadKey.clear();
    m_numpadBuffer.clear();

    QString loadErr;
    if (!m_instances->loadInto(instId, returnId, &loadErr)) {
        if (error) {
            *error = loadErr;
        }
        notifyStatus(QStringLiteral("Cancelled"));
        return false;
    }
    notifyStatus(QStringLiteral("Edit cancelled"));
    return true;
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
    // Ensure click works even if mapping profile failed to load.
    m_commands->registerBuiltin(QStringLiteral("mouseLeftClick"), [this](QString* error) {
        InputOutput o;
        o.type = InputOutput::Type::MouseClick;
        o.button = QStringLiteral("left");
        return m_input->execute(o, error);
    });
    m_commands->registerBuiltin(QStringLiteral("stopAllActionLoops"), [this](QString*) {
        if (m_actionLoops) {
            m_actionLoops->stopAll();
        }
        refreshActiveIndicators();
        notifyStatus(QStringLiteral("All action loops stopped"));
        return true;
    });
}

void GazerServices::registerSettingsCommands()
{
    auto editCmd = [this](const QString& key) {
        return [this, key](QString* error) {
            return openNumericEditor(key, error);
        };
    };

    for (const char* key :
         {"dwellMs", "dwellGraceMs", "mouseMoveDwellMs", "magZoom", "magLensSize",
          "ltsDeadzonePx", "ltsFalloffPx", "ltsMaxNotchesPerSec"}) {
        m_commands->registerBuiltin(QStringLiteral("settings.edit.%1").arg(QLatin1String(key)),
                                    editCmd(QLatin1String(key)));
    }

    m_commands->registerBuiltin(QStringLiteral("settings.numpad.noop"),
                                [](QString*) { return true; });
    for (int d = 0; d <= 9; ++d) {
        m_commands->registerBuiltin(
            QStringLiteral("settings.numpad.digit.%1").arg(d), [this, d](QString*) {
                numpadAppend(QString::number(d));
                return true;
            });
    }
    m_commands->registerBuiltin(QStringLiteral("settings.numpad.period"), [this](QString*) {
        numpadAppend(QStringLiteral("."));
        return true;
    });
    m_commands->registerBuiltin(QStringLiteral("settings.numpad.comma"), [this](QString*) {
        numpadAppend(QStringLiteral(","));
        return true;
    });
    m_commands->registerBuiltin(QStringLiteral("settings.numpad.backspace"), [this](QString*) {
        numpadBackspace();
        return true;
    });
    m_commands->registerBuiltin(QStringLiteral("settings.numpad.clear"), [this](QString*) {
        numpadClear();
        return true;
    });
    m_commands->registerBuiltin(QStringLiteral("settings.numpad.reset"), [this](QString*) {
        numpadReset();
        return true;
    });
    m_commands->registerBuiltin(QStringLiteral("settings.numpad.minus"), [this](QString*) {
        numpadMinus();
        return true;
    });

    auto nudge = [this](const QString& key, int dir) {
        return [this, key, dir](QString*) {
            if (key == QLatin1String("dwellMs") || key == QLatin1String("dwellSequence")) {
                if (m_settings.dwellSequence.isEmpty()) {
                    m_settings.dwellSequence = {700};
                }
                m_settings.dwellSequence[0] =
                    qBound(50, m_settings.dwellSequence[0] + dir * 50, 10000);
            } else if (key == QLatin1String("dwellGraceMs")) {
                m_settings.dwellGraceMs =
                    qBound(0, m_settings.dwellGraceMs + dir * 20, 800);
            } else if (key == QLatin1String("mouseMoveDwellMs")) {
                m_settings.mouseMoveDwellMs =
                    qBound(200, m_settings.mouseMoveDwellMs + dir * 50, 2500);
            } else if (key == QLatin1String("magZoom")) {
                m_settings.magZoom = qBound(1.25, m_settings.magZoom + dir * 0.25, 6.0);
            } else if (key == QLatin1String("magLensSize")) {
                m_settings.magLensSize =
                    qBound(160, m_settings.magLensSize + dir * 20, 900);
            } else if (key == QLatin1String("ltsDeadzonePx")) {
                m_settings.ltsDeadzonePx =
                    qBound(30, m_settings.ltsDeadzonePx + dir * 10, 400);
            } else if (key == QLatin1String("ltsFalloffPx")) {
                m_settings.ltsFalloffPx =
                    qBound(80, m_settings.ltsFalloffPx + dir * 20, 800);
            } else if (key == QLatin1String("ltsMaxNotchesPerSec")) {
                m_settings.ltsMaxNotchesPerSec =
                    qBound(0.5, m_settings.ltsMaxNotchesPerSec + dir * 0.5, 24.0);
            } else if (key == QLatin1String("ltsAccelPerSec")) {
                m_settings.ltsAccelPerSec =
                    qBound(0.0, m_settings.ltsAccelPerSec + dir * 0.05, 2.0);
            } else if (key == QLatin1String("ltsCenterDwellMs")) {
                m_settings.ltsCenterDwellMs =
                    qBound(200, m_settings.ltsCenterDwellMs + dir * 50, 2500);
            } else if (key == QLatin1String("flashMs")) {
                m_settings.flashMs = qBound(40, m_settings.flashMs + dir * 20, 1000);
            }
            applySettings(true);
            notifyStatus(QStringLiteral("%1 = %2")
                             .arg(AppSettings::settingTitle(key), m_settings.displayValue(key)));
            return true;
        };
    };

    for (const char* key :
         {"dwellMs", "dwellGraceMs", "mouseMoveDwellMs", "magZoom", "magLensSize",
          "ltsDeadzonePx", "ltsFalloffPx", "ltsMaxNotchesPerSec", "ltsAccelPerSec",
          "ltsCenterDwellMs", "flashMs"}) {
        m_commands->registerBuiltin(QStringLiteral("settings.nudge.%1.dec").arg(QLatin1String(key)),
                                    nudge(QLatin1String(key), -1));
        m_commands->registerBuiltin(QStringLiteral("settings.nudge.%1.inc").arg(QLatin1String(key)),
                                    nudge(QLatin1String(key), +1));
    }

    auto toggleBool = [this](bool AppSettings::*member, const QString& label) {
        return [this, member, label](QString*) {
            m_settings.*member = !(m_settings.*member);
            applySettings(true);
            notifyStatus(QStringLiteral("%1: %2")
                             .arg(label, (m_settings.*member) ? QStringLiteral("ON")
                                                              : QStringLiteral("OFF")));
            return true;
        };
    };
    m_commands->registerBuiltin(QStringLiteral("settings.progress.radial.toggle"),
                                toggleBool(&AppSettings::progressRadial, QStringLiteral("Radial")));
    m_commands->registerBuiltin(QStringLiteral("settings.progress.fill.toggle"),
                                toggleBool(&AppSettings::progressFill, QStringLiteral("Fill")));
    m_commands->registerBuiltin(QStringLiteral("settings.progress.border.toggle"),
                                toggleBool(&AppSettings::progressBorder, QStringLiteral("Border")));
    m_commands->registerBuiltin(
        QStringLiteral("settings.mouseProgress.radial.toggle"),
        toggleBool(&AppSettings::mouseProgressRadial, QStringLiteral("Mouse radial")));
    m_commands->registerBuiltin(
        QStringLiteral("settings.mouseProgress.fill.toggle"),
        toggleBool(&AppSettings::mouseProgressFill, QStringLiteral("Mouse fill")));
    m_commands->registerBuiltin(
        QStringLiteral("settings.mouseProgress.border.toggle"),
        toggleBool(&AppSettings::mouseProgressBorder, QStringLiteral("Mouse border")));
    m_commands->registerBuiltin(
        QStringLiteral("settings.flash.toggle"),
        toggleBool(&AppSettings::flashOnComplete, QStringLiteral("Completion flash")));

    const char* colorKeys[] = {"progressColor",          "progressFillColor",
                               "progressBorderColor",    "mouseProgressColor",
                               "mouseProgressFillColor", "mouseProgressBorderColor",
                               "flashBorderColor",       "flashFillColor"};
    const char* palette[] = {"00DCFF", "FFFFFF", "FFC828", "00FF88", "FF4488",
                             "8866FF", "FF8800", "33AADD", "AABBCC", "222222"};
    for (const char* ck : colorKeys) {
        m_commands->registerBuiltin(
            QStringLiteral("settings.edit.color.%1").arg(QLatin1String(ck)),
            [this, ck](QString* error) { return openColorPicker(QLatin1String(ck), error); });
        for (const char* hex : palette) {
            m_commands->registerBuiltin(
                QStringLiteral("settings.color.%1.%2").arg(QLatin1String(ck), QLatin1String(hex)),
                [this, ck, hex](QString*) {
                    m_settings.setColorKey(QLatin1String(ck),
                                           QColor(QStringLiteral("#%1").arg(QLatin1String(hex))));
                    applySettings(true);
                    if (m_colorPickerActive) {
                        closeColorPicker();
                    }
                    notifyStatus(QStringLiteral("%1 = #%2")
                                     .arg(AppSettings::settingTitle(QLatin1String(ck)),
                                          QLatin1String(hex)));
                    return true;
                });
        }
    }
    m_commands->registerBuiltin(QStringLiteral("settings.color.cancel"), [this](QString*) {
        closeColorPicker();
        notifyStatus(QStringLiteral("Color pick cancelled"));
        return true;
    });
    m_commands->registerBuiltin(QStringLiteral("settings.numpad.save"),
                                [this](QString* e) { return numpadSave(e); });
    m_commands->registerBuiltin(QStringLiteral("settings.numpad.cancel"),
                                [this](QString* e) { return numpadCancel(e); });

    // Presets still available
    m_commands->registerBuiltin(QStringLiteral("settings.dwell.slow"), [this](QString*) {
        mutateAndApply([](AppSettings& s) { s.setDwellPreset(0); },
                       QStringLiteral("Dwell: Slow (~1000 ms)"));
        return true;
    });
    m_commands->registerBuiltin(QStringLiteral("settings.dwell.normal"), [this](QString*) {
        mutateAndApply([](AppSettings& s) { s.setDwellPreset(1); },
                       QStringLiteral("Dwell: Normal (~700 ms)"));
        return true;
    });
    m_commands->registerBuiltin(QStringLiteral("settings.dwell.fast"), [this](QString*) {
        mutateAndApply([](AppSettings& s) { s.setDwellPreset(2); },
                       QStringLiteral("Dwell: Fast (~450 ms)"));
        return true;
    });

    m_commands->registerBuiltin(QStringLiteral("settings.mag.follow.sticky"), [this](QString*) {
        mutateAndApply([](AppSettings& s) { s.setMagFollowProfile(0); },
                       QStringLiteral("Mag follow: Sticky"));
        return true;
    });
    m_commands->registerBuiltin(QStringLiteral("settings.mag.follow.balanced"), [this](QString*) {
        mutateAndApply([](AppSettings& s) { s.setMagFollowProfile(1); },
                       QStringLiteral("Mag follow: Balanced"));
        return true;
    });
    m_commands->registerBuiltin(QStringLiteral("settings.mag.follow.snappy"), [this](QString*) {
        mutateAndApply([](AppSettings& s) { s.setMagFollowProfile(2); },
                       QStringLiteral("Mag follow: Snappy"));
        return true;
    });

    m_commands->registerBuiltin(QStringLiteral("settings.lts.placeCursor.toggle"), [this](QString*) {
        m_settings.ltsPlaceCursorFirst = !m_settings.ltsPlaceCursorFirst;
        applySettings(true);
        notifyStatus(m_settings.ltsPlaceCursorFirst
                         ? QStringLiteral("LTS: place cursor first ON")
                         : QStringLiteral("LTS: place cursor first OFF"));
        return true;
    });
    m_commands->registerBuiltin(QStringLiteral("settings.session.autoCollapse.toggle"),
                                [this](QString*) {
                                    m_settings.autoCollapseMain = !m_settings.autoCollapseMain;
                                    applySettings(true);
                                    notifyStatus(m_settings.autoCollapseMain
                                                     ? QStringLiteral("Auto-collapse Main: ON")
                                                     : QStringLiteral("Auto-collapse Main: OFF"));
                                    return true;
                                });
    m_commands->registerBuiltin(QStringLiteral("settings.session.startDocked.toggle"),
                                [this](QString*) {
                                    m_settings.startDocked = !m_settings.startDocked;
                                    applySettings(true);
                                    notifyStatus(m_settings.startDocked
                                                     ? QStringLiteral("Start docked: ON")
                                                     : QStringLiteral("Start docked: OFF"));
                                    return true;
                                });
    m_commands->registerBuiltin(QStringLiteral("settings.tracker.auto"), [this](QString*) {
        mutateAndApply([](AppSettings& s) { s.trackerPref = 0; },
                       QStringLiteral("Tracker: Auto Tobii (restart to apply)"));
        return true;
    });
    m_commands->registerBuiltin(QStringLiteral("settings.tracker.mouse"), [this](QString*) {
        mutateAndApply([](AppSettings& s) { s.trackerPref = 1; },
                       QStringLiteral("Tracker: Mouse only (restart to apply)"));
        return true;
    });
    m_commands->registerBuiltin(QStringLiteral("settings.speech.alsoType.toggle"), [this](QString*) {
        m_settings.speakAlsoType = !m_settings.speakAlsoType;
        applySettings(true);
        notifyStatus(m_settings.speakAlsoType ? QStringLiteral("Speak also types: ON")
                                              : QStringLiteral("Speak also types: OFF"));
        return true;
    });
    m_commands->registerBuiltin(QStringLiteral("settings.reset"), [this](QString*) {
        resetSettingsToDefaults();
        return true;
    });
}

} // namespace gazer
