#include "app/SettingsUi.h"

#include "app/CommandRegistry.h"
#include "layout/LayoutInstance.h"
#include "layout/LayoutInstanceManager.h"
#include "layout/LayoutManager.h"
#include "ui/Theme.h"

#include <QColor>
#include <QtGlobal>

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

SettingsUi::SettingsUi(AppSettings& settings, LayoutInstanceManager& instances,
                       LayoutManager& catalog, CommandRegistry& commands)
    : m_settings(settings)
    , m_instances(instances)
    , m_catalog(catalog)
    , m_commands(commands)
{
}

void SettingsUi::notifyStatus(const QString& msg)
{
    if (m_notify) {
        m_notify(msg);
    }
}

void SettingsUi::apply(bool persist)
{
    if (m_apply) {
        m_apply(persist);
    }
}

void SettingsUi::decorateDocument(LayoutDocument& doc) const
{
    if (!doc.id.startsWith(QLatin1String("main_settings"))) {
        return;
    }
    doc.uiStyle = LayoutUiStyle::Fluent;
    for (LayoutItem& item : doc.items) {
        if (!item.settingKey.isEmpty() && !item.interactive) {
            item.label = m_settings.displayValue(item.settingKey);
        }
        if (!item.interactive && item.caption.isEmpty() && !item.settingKey.isEmpty()
            && item.id.contains(QLatin1String("desc"))) {
            item.label = AppSettings::settingDescription(item.settingKey);
        }
    }
}

void SettingsUi::refreshOpenBoards()
{
    for (LayoutInstance* inst : m_instances.instances()) {
        if (!inst) {
            continue;
        }
        const QString lid = inst->layoutId();
        if (!lid.startsWith(QLatin1String("main_settings"))) {
            continue;
        }
        if (m_numpadActive && inst->instanceId() == m_numpadInstanceId) {
            continue;
        }
        const LayoutDocument* src = m_catalog.document(lid);
        if (!src) {
            continue;
        }
        LayoutDocument copy = *src;
        decorateDocument(copy);
        inst->setDocument(copy);
        inst->setGlobalDwellOverride(m_settings.dwellSequence, m_settings.dwellGraceMs,
                                     m_settings.scanGraceMs);
    }
}

LayoutDocument SettingsUi::buildNumpadDocument() const
{
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
    doc.placement.width = DimSpec::pixels(520);
    doc.placement.height = DimSpec::pixels(720);

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

bool SettingsUi::openNumericEditor(const QString& settingKey, QString* error)
{
    if (!AppSettings::isNumericKey(settingKey)) {
        if (error) {
            *error = QStringLiteral("Not a numeric setting: %1").arg(settingKey);
        }
        return false;
    }
    auto* focused = m_instances.focusedInstance();
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

    if (!m_instances.setInstanceDocument(m_numpadInstanceId, buildNumpadDocument(), error)) {
        m_numpadActive = false;
        return false;
    }
    notifyStatus(QStringLiteral("Edit %1").arg(AppSettings::settingTitle(settingKey)));
    return true;
}

void SettingsUi::refreshNumpadDisplay()
{
    if (!m_numpadActive) {
        return;
    }
    auto* inst = m_instances.instance(m_numpadInstanceId);
    if (!inst) {
        return;
    }
    const QString shown = m_numpadBuffer.isEmpty() ? QStringLiteral("0") : m_numpadBuffer;
    inst->setItemText(QStringLiteral("display"), shown, {});
}

void SettingsUi::numpadAppend(const QString& ch)
{
    if (!m_numpadActive) {
        return;
    }
    const bool sequenceMode = (m_numpadKey == QLatin1String("dwellMs")
                               || m_numpadKey == QLatin1String("dwellSequence"));
    if (ch == QLatin1String(".") && !sequenceMode && m_numpadBuffer.contains(QLatin1Char('.'))) {
        return;
    }
    if (ch == QLatin1String(",") && !sequenceMode) {
        return;
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

void SettingsUi::numpadBackspace()
{
    if (!m_numpadActive || m_numpadBuffer.isEmpty()) {
        return;
    }
    m_numpadBuffer.chop(1);
    refreshNumpadDisplay();
}

void SettingsUi::numpadClear()
{
    if (!m_numpadActive) {
        return;
    }
    m_numpadBuffer.clear();
    refreshNumpadDisplay();
}

void SettingsUi::numpadReset()
{
    if (!m_numpadActive) {
        return;
    }
    m_numpadBuffer = m_settings.numericBufferSeed(m_numpadKey);
    refreshNumpadDisplay();
}

void SettingsUi::numpadMinus()
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

bool SettingsUi::numpadSave(QString* error)
{
    if (!m_numpadActive) {
        if (error) {
            *error = QStringLiteral("Numeric editor is not open");
        }
        return false;
    }
    QString err;
    if (!m_settings.applyNumericBuffer(m_numpadKey,
                                       m_numpadBuffer.isEmpty() ? QStringLiteral("0")
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
    apply(true);

    const QString returnId = m_numpadReturnLayoutId;
    const QString instId = m_numpadInstanceId;
    m_numpadActive = false;
    m_numpadInstanceId.clear();
    m_numpadKey.clear();
    m_numpadBuffer.clear();

    QString loadErr;
    if (!m_instances.loadInto(instId, returnId, &loadErr)) {
        const LayoutDocument* src = m_catalog.document(returnId);
        if (src) {
            LayoutDocument copy = *src;
            decorateDocument(copy);
            (void)m_instances.setInstanceDocument(instId, copy, &loadErr);
        }
    }
    notifyStatus(QStringLiteral("Saved %1 = %2").arg(savedTitle, savedValue));
    return true;
}

bool SettingsUi::openColorPicker(const QString& colorKey, QString* error)
{
    if (!AppSettings::isColorKey(colorKey)) {
        if (error) {
            *error = QStringLiteral("Not a color setting");
        }
        return false;
    }
    auto* focused = m_instances.focusedInstance();
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
    doc.grid.rows = 5;
    doc.grid.gapPx = 12;
    doc.grid.marginPx = 56;
    doc.dwell.ms = m_settings.dwellSequence.isEmpty() ? 650 : m_settings.dwellSequence.first();
    doc.placement.specified = true;
    doc.placement.anchor = LayoutWindowPlacement::Anchor::Center;
    doc.placement.width = DimSpec::pixels(720);
    doc.placement.height = DimSpec::pixels(420);

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

    if (!m_instances.setInstanceDocument(m_colorPickerInstanceId, doc, error)) {
        m_colorPickerActive = false;
        return false;
    }
    notifyStatus(QStringLiteral("Pick color for %1").arg(AppSettings::settingTitle(colorKey)));
    return true;
}

void SettingsUi::closeColorPicker()
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
    (void)m_instances.loadInto(instId, returnId, &err);
}

bool SettingsUi::numpadCancel(QString* error)
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
    if (!m_instances.loadInto(instId, returnId, &loadErr)) {
        if (error) {
            *error = loadErr;
        }
        notifyStatus(QStringLiteral("Cancelled"));
        return false;
    }
    notifyStatus(QStringLiteral("Edit cancelled"));
    return true;
}

void SettingsUi::registerCommands()
{
    auto editCmd = [this](const QString& key) {
        return [this, key](QString* error) { return openNumericEditor(key, error); };
    };

    for (const char* key :
         {"dwellMs", "scanGraceMs", "dwellGraceMs", "mouseMoveDwellMs",
          "mouseMoveSelectTimeoutMs", "magZoom", "magLensSize", "ltsDeadzonePx", "ltsFalloffPx",
          "ltsMaxNotchesPerSec"}) {
        m_commands.registerBuiltin(QStringLiteral("settings.edit.%1").arg(QLatin1String(key)),
                                   editCmd(QLatin1String(key)));
    }

    m_commands.registerBuiltin(QStringLiteral("settings.numpad.noop"),
                               [](QString*) { return true; });
    for (int d = 0; d <= 9; ++d) {
        m_commands.registerBuiltin(
            QStringLiteral("settings.numpad.digit.%1").arg(d), [this, d](QString*) {
                numpadAppend(QString::number(d));
                return true;
            });
    }
    m_commands.registerBuiltin(QStringLiteral("settings.numpad.period"), [this](QString*) {
        numpadAppend(QStringLiteral("."));
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.numpad.comma"), [this](QString*) {
        numpadAppend(QStringLiteral(","));
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.numpad.backspace"), [this](QString*) {
        numpadBackspace();
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.numpad.clear"), [this](QString*) {
        numpadClear();
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.numpad.reset"), [this](QString*) {
        numpadReset();
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.numpad.minus"), [this](QString*) {
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
            } else if (key == QLatin1String("scanGraceMs")) {
                m_settings.scanGraceMs =
                    qBound(0, m_settings.scanGraceMs + dir * 20, 2000);
            } else if (key == QLatin1String("dwellGraceMs")) {
                m_settings.dwellGraceMs =
                    qBound(0, m_settings.dwellGraceMs + dir * 20, 800);
            } else if (key == QLatin1String("mouseMoveDwellMs")) {
                m_settings.mouseMoveDwellMs =
                    qBound(200, m_settings.mouseMoveDwellMs + dir * 50, 2500);
            } else if (key == QLatin1String("mouseMoveSelectTimeoutMs")) {
                m_settings.mouseMoveSelectTimeoutMs =
                    qBound(0, m_settings.mouseMoveSelectTimeoutMs + dir * 500, 120000);
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
            apply(true);
            notifyStatus(QStringLiteral("%1 = %2")
                             .arg(AppSettings::settingTitle(key), m_settings.displayValue(key)));
            return true;
        };
    };

    for (const char* key :
         {"dwellMs", "scanGraceMs", "dwellGraceMs", "mouseMoveDwellMs",
          "mouseMoveSelectTimeoutMs", "magZoom", "magLensSize", "ltsDeadzonePx", "ltsFalloffPx",
          "ltsMaxNotchesPerSec", "ltsAccelPerSec", "ltsCenterDwellMs", "flashMs"}) {
        m_commands.registerBuiltin(QStringLiteral("settings.nudge.%1.dec").arg(QLatin1String(key)),
                                   nudge(QLatin1String(key), -1));
        m_commands.registerBuiltin(QStringLiteral("settings.nudge.%1.inc").arg(QLatin1String(key)),
                                   nudge(QLatin1String(key), +1));
    }

    auto toggleBool = [this](bool AppSettings::*member, const QString& label) {
        return [this, member, label](QString*) {
            m_settings.*member = !(m_settings.*member);
            apply(true);
            notifyStatus(QStringLiteral("%1: %2")
                             .arg(label, (m_settings.*member) ? QStringLiteral("ON")
                                                              : QStringLiteral("OFF")));
            return true;
        };
    };
    m_commands.registerBuiltin(QStringLiteral("settings.progress.radial.toggle"),
                               toggleBool(&AppSettings::progressRadial, QStringLiteral("Radial")));
    m_commands.registerBuiltin(QStringLiteral("settings.progress.fill.toggle"),
                               toggleBool(&AppSettings::progressFill, QStringLiteral("Fill")));
    m_commands.registerBuiltin(QStringLiteral("settings.progress.border.toggle"),
                               toggleBool(&AppSettings::progressBorder, QStringLiteral("Border")));
    m_commands.registerBuiltin(
        QStringLiteral("settings.mouseProgress.radial.toggle"),
        toggleBool(&AppSettings::mouseProgressRadial, QStringLiteral("Mouse radial")));
    m_commands.registerBuiltin(
        QStringLiteral("settings.mouseProgress.fill.toggle"),
        toggleBool(&AppSettings::mouseProgressFill, QStringLiteral("Mouse fill")));
    m_commands.registerBuiltin(
        QStringLiteral("settings.mouseProgress.border.toggle"),
        toggleBool(&AppSettings::mouseProgressBorder, QStringLiteral("Mouse border")));
    m_commands.registerBuiltin(
        QStringLiteral("settings.flash.toggle"),
        toggleBool(&AppSettings::flashOnComplete, QStringLiteral("Completion flash")));

    const char* colorKeys[] = {"progressColor",          "progressFillColor",
                               "progressBorderColor",    "mouseProgressColor",
                               "mouseProgressFillColor", "mouseProgressBorderColor",
                               "flashBorderColor",       "flashFillColor"};
    const char* palette[] = {"00DCFF", "FFFFFF", "FFC828", "00FF88", "FF4488",
                             "8866FF", "FF8800", "33AADD", "AABBCC", "222222"};
    for (const char* ck : colorKeys) {
        m_commands.registerBuiltin(
            QStringLiteral("settings.edit.color.%1").arg(QLatin1String(ck)),
            [this, ck](QString* error) { return openColorPicker(QLatin1String(ck), error); });
        for (const char* hex : palette) {
            m_commands.registerBuiltin(
                QStringLiteral("settings.color.%1.%2").arg(QLatin1String(ck), QLatin1String(hex)),
                [this, ck, hex](QString*) {
                    (void)m_settings.setColorKey(
                        QLatin1String(ck),
                        QColor(QStringLiteral("#%1").arg(QLatin1String(hex))));
                    apply(true);
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
    m_commands.registerBuiltin(QStringLiteral("settings.color.cancel"), [this](QString*) {
        closeColorPicker();
        notifyStatus(QStringLiteral("Color pick cancelled"));
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.numpad.save"),
                               [this](QString* e) { return numpadSave(e); });
    m_commands.registerBuiltin(QStringLiteral("settings.numpad.cancel"),
                               [this](QString* e) { return numpadCancel(e); });

    m_commands.registerBuiltin(QStringLiteral("settings.dwell.slow"), [this](QString*) {
        if (m_mutate) {
            m_mutate([](AppSettings& s) { s.setDwellPreset(0); },
                     QStringLiteral("Dwell: Slow (~1000 ms)"));
        }
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.dwell.normal"), [this](QString*) {
        if (m_mutate) {
            m_mutate([](AppSettings& s) { s.setDwellPreset(1); },
                     QStringLiteral("Dwell: Normal (~700 ms)"));
        }
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.dwell.fast"), [this](QString*) {
        if (m_mutate) {
            m_mutate([](AppSettings& s) { s.setDwellPreset(2); },
                     QStringLiteral("Dwell: Fast (~450 ms)"));
        }
        return true;
    });

    m_commands.registerBuiltin(QStringLiteral("settings.mag.follow.sticky"), [this](QString*) {
        if (m_mutate) {
            m_mutate([](AppSettings& s) { s.setMagFollowProfile(0); },
                     QStringLiteral("Mag follow: Sticky"));
        }
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.mag.follow.balanced"), [this](QString*) {
        if (m_mutate) {
            m_mutate([](AppSettings& s) { s.setMagFollowProfile(1); },
                     QStringLiteral("Mag follow: Balanced"));
        }
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.mag.follow.snappy"), [this](QString*) {
        if (m_mutate) {
            m_mutate([](AppSettings& s) { s.setMagFollowProfile(2); },
                     QStringLiteral("Mag follow: Snappy"));
        }
        return true;
    });

    m_commands.registerBuiltin(QStringLiteral("settings.lts.placeCursor.toggle"), [this](QString*) {
        m_settings.ltsPlaceCursorFirst = !m_settings.ltsPlaceCursorFirst;
        apply(true);
        notifyStatus(m_settings.ltsPlaceCursorFirst
                         ? QStringLiteral("LTS: place cursor first ON")
                         : QStringLiteral("LTS: place cursor first OFF"));
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.session.autoCollapse.toggle"),
                               [this](QString*) {
                                   m_settings.autoCollapseMain = !m_settings.autoCollapseMain;
                                   apply(true);
                                   notifyStatus(m_settings.autoCollapseMain
                                                    ? QStringLiteral("Auto-collapse Main: ON")
                                                    : QStringLiteral("Auto-collapse Main: OFF"));
                                   return true;
                               });
    m_commands.registerBuiltin(QStringLiteral("settings.session.startDocked.toggle"),
                               [this](QString*) {
                                   m_settings.startDocked = !m_settings.startDocked;
                                   apply(true);
                                   notifyStatus(m_settings.startDocked
                                                    ? QStringLiteral("Start docked: ON")
                                                    : QStringLiteral("Start docked: OFF"));
                                   return true;
                               });
    m_commands.registerBuiltin(QStringLiteral("settings.tracker.auto"), [this](QString*) {
        if (m_mutate) {
            m_mutate([](AppSettings& s) { s.trackerPref = 0; },
                     QStringLiteral("Tracker: Auto Tobii (restart to apply)"));
        }
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.tracker.mouse"), [this](QString*) {
        if (m_mutate) {
            m_mutate([](AppSettings& s) { s.trackerPref = 1; },
                     QStringLiteral("Tracker: Mouse only (restart to apply)"));
        }
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.speech.alsoType.toggle"), [this](QString*) {
        m_settings.speakAlsoType = !m_settings.speakAlsoType;
        apply(true);
        notifyStatus(m_settings.speakAlsoType ? QStringLiteral("Speak also types: ON")
                                              : QStringLiteral("Speak also types: OFF"));
        return true;
    });
    m_commands.registerBuiltin(QStringLiteral("settings.reset"), [this](QString*) {
        if (m_reset) {
            m_reset();
        }
        return true;
    });
}

} // namespace gazer
