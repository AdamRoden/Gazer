#include "app/SettingsUi.h"

#include "app/CommandRegistry.h"
#include "layout/LayoutTypes.h"
#include "layout/LayoutInstance.h"
#include "layout/LayoutInstanceManager.h"
#include "layout/LayoutManager.h"
#include "ui/LayoutQuickWindow.h"
#include "ui/Theme.h"

#include <QColor>
#include <QKeyEvent>
#include <QObject>
#include <QtGlobal>
#include <QVector>

namespace gazer {

LayoutItem SettingsUi::makeItem(const QString& id, const QString& label, int row, int col,
                                LayoutAction::Type type, const QString& payload, const QColor& bg,
                                int colSpan, bool interactive, const QString& caption,
                                const QString& settingKey, const QString& role)
{
    LayoutItem it;
    it.id = id;
    it.label = label;
    it.caption = caption;
    it.settingKey = settingKey;
    it.role = role;
    it.applyKind();
    it.interactive = interactive;
    it.row = row;
    it.col = col;
    it.colSpan = colSpan;
    const QColor fill = bg.isValid() ? bg : ThemeColors::darkPreset().cellBg;
    it.style.background = fill;
    it.style.foreground = fill.alpha() == 0 ? QColor() : ThemeColors::contrastOn(fill);
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

LayoutItem SettingsUi::makeLabel(const QString& id, const QString& label, int row, int col,
                                 int colSpan, const QString& caption, const QString& settingKey)
{
    LayoutItem it = makeItem(id, label, row, col, LayoutAction::Type::Unknown, {}, QColor(0, 0, 0, 0),
                             colSpan, /*interactive=*/false, caption, settingKey);
    it.role = QStringLiteral("label");
    it.applyKind();
    return it;
}

void SettingsUi::applyLiveEditorChrome(LayoutDocument& doc)
{
    doc.autoClose = false;
    doc.dwell.enabled = true;
    doc.placement.specified = true;
    doc.placement.anchor = LayoutWindowPlacement::Anchor::Center;
    doc.placement.aboveTaskbar = true;
    doc.placement.style.background = QColor(0, 0, 0);
    doc.placement.style.radius = 8.0;
    doc.placement.style.borderWidth = 1.0;
}

void SettingsUi::resetNumpad()
{
    unbindEditorKeyboard();
    m_numpad.reset();
    m_numpadKey.clear();
    m_numpadTitle.clear();
    m_numpadHint.clear();
    m_numpadResetSeed.clear();
    m_numpadBuffer.clear();
    m_numpadReturn = NumpadReturn::Catalog;
    m_numpadColorChannel.clear();
    m_numpadArrayIndex = -1;
}

void SettingsUi::unbindEditorKeyboard()
{
    QObject::disconnect(m_editorKeyConn);
    m_editorKeyConn = {};
    if (auto* inst = m_instances.instance(m_numpad.instanceId)) {
        if (inst->window()) {
            inst->window()->setInputFocusEnabled(false);
        }
    }
    if (auto* inst = m_instances.instance(m_color.instanceId)) {
        if (inst->window()) {
            inst->window()->setInputFocusEnabled(false);
        }
    }
}

void SettingsUi::bindEditorKeyboard(const QString& instanceId)
{
    unbindEditorKeyboard();
    auto* inst = m_instances.instance(instanceId);
    if (!inst || !inst->window()) {
        return;
    }
    LayoutQuickWindow* w = inst->window();
    w->setInputFocusEnabled(true);
    m_editorKeyConn = QObject::connect(w, &LayoutQuickWindow::keyPressed, w,
                                       [this](int key, const QString& text) {
                                           handleEditorKey(key, text);
                                       });
}

void SettingsUi::handleEditorKey(int key, const QString& text)
{
    if (m_numpad.active) {
        if (key == Qt::Key_Backspace) {
            numpadBackspace();
            return;
        }
        if (key == Qt::Key_Escape) {
            QString err;
            (void)numpadCancel(&err);
            return;
        }
        if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            QString err;
            (void)numpadSave(&err);
            return;
        }
        if (key == Qt::Key_Minus) {
            numpadMinus();
            return;
        }
        if (key == Qt::Key_Period) {
            numpadAppend(QStringLiteral("."));
            return;
        }
        if (key == Qt::Key_Comma) {
            numpadAppend(QStringLiteral(","));
            return;
        }
        if (key >= Qt::Key_0 && key <= Qt::Key_9) {
            numpadAppend(QString::number(key - Qt::Key_0));
            return;
        }
        for (const QChar ch : text) {
            if (ch.isDigit()) {
                numpadAppend(QString(ch));
            }
        }
        return;
    }
    if (m_hexActive) {
        if (key == Qt::Key_Backspace) {
            hexBackspace();
            return;
        }
        if (key == Qt::Key_Escape) {
            QString err;
            (void)hexCancel(&err);
            return;
        }
        if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            QString err;
            (void)hexSave(&err);
            return;
        }
        if (key >= Qt::Key_0 && key <= Qt::Key_9) {
            hexAppend(QChar(QLatin1Char('0' + (key - Qt::Key_0))));
            return;
        }
        if (key >= Qt::Key_A && key <= Qt::Key_F) {
            hexAppend(QChar(QLatin1Char('A' + (key - Qt::Key_A))));
            return;
        }
        for (const QChar ch : text) {
            if (ch.isDigit()
                || (ch.toUpper() >= QLatin1Char('A') && ch.toUpper() <= QLatin1Char('F'))) {
                hexAppend(ch);
            }
        }
    }
}

bool SettingsUi::presentNumpad(QString* error)
{
    if (m_instances.setInstanceDocument(m_numpad.instanceId, buildNumpadDocument(), error)) {
        bindEditorKeyboard(m_numpad.instanceId);
        return true;
    }
    resetNumpad();
    return false;
}

SettingsUi::EditorSwatch SettingsUi::editorSwatch() const
{
    const ThemeColors t = m_settings.resolvedTheme();
    EditorSwatch s;
    s.key = t.cellActive;
    s.save = t.accent;
    s.cancel = t.danger;
    s.nudge = t.bgSurfaceActive;
    s.warn = t.cellHover;
    s.add = t.accentHover;
    s.value = t.bgMain;
    s.edit = t.accent;
    return s;
}

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

namespace {

QString colorKeyFromItem(const LayoutItem& item)
{
    if (AppSettings::isColorKey(item.settingKey)) {
        return item.settingKey;
    }
    const QString name = item.action.name;
    const QLatin1String prefix("settings.edit.color.");
    if (name.startsWith(prefix)) {
        return name.mid(int(prefix.size()));
    }
    return {};
}

} // namespace

void SettingsUi::decorateDocument(LayoutDocument& doc) const
{
    if (!doc.id.startsWith(QLatin1String("main_settings"))) {
        return;
    }

    static const struct {
        const char* id;
        const char* label;
        const char* layoutId;
        int col;
        int span;
    } kTabs[] = {
        {"tab_buttons", "Button", "main_settings_button_timing", 0, 3},
        {"tab_pointers", "Pointer", "main_settings_pointer_timing", 3, 2},
        {"tab_styles", "Styles", "main_settings_styles", 5, 2},
        {"tab_assist", "Assist", "main_settings_assist", 7, 3},
        {"tab_lts", "LTS", "main_settings_lts", 10, 3},
        {"tab_theme", "Theme", "main_settings_theme", 13, 3},
    };
    QVector<LayoutItem> tabs;
    tabs.reserve(6);
    for (const auto& spec : kTabs) {
        LayoutItem t;
        t.id = QLatin1String(spec.id);
        t.role = QStringLiteral("tab");
        t.label = QLatin1String(spec.label);
        t.row = 0;
        t.col = spec.col;
        t.colSpan = spec.span;
        t.applyKind();
        if (doc.id == QLatin1String(spec.layoutId)) {
            t.interactive = false;
        } else {
            t.action.type = LayoutAction::Type::LoadLayout;
            t.action.layoutId = QLatin1String(spec.layoutId);
        }
        tabs.push_back(std::move(t));
    }
    QVector<LayoutItem> body;
    body.reserve(doc.items.size());
    for (LayoutItem& item : doc.items) {
        if (item.kind == LayoutItemKind::Tab) {
            continue;
        }
        body.push_back(std::move(item));
    }
    doc.items = std::move(tabs);
    doc.items.append(body);
    if (doc.grid.columns < 16) {
        doc.grid.columns = 16;
    }

    const ThemeColors theme = m_settings.customColors;
    for (LayoutItem& item : doc.items) {
        if (!item.settingKey.isEmpty() && !item.interactive) {
            item.label = m_settings.displayValue(item.settingKey);
        }
        if (!item.interactive && item.caption.isEmpty() && !item.settingKey.isEmpty()
            && item.id.contains(QLatin1String("desc"))) {
            item.label = AppSettings::settingDescription(item.settingKey);
        }
        QColor sw;
        if (item.settingKey == QLatin1String("themeVariant")) {
            sw = theme.bgSurface;
        } else if (item.settingKey == QLatin1String("themeForeground")) {
            sw = theme.text;
        } else {
            const QString ck = colorKeyFromItem(item);
            if (!ck.isEmpty()) {
                sw = m_settings.colorKey(ck);
            }
        }
        if (sw.isValid()) {
            item.style.background = sw;
            item.style.foreground = ThemeColors::contrastOn(sw);
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
        if (isLiveEditorInstance(inst->instanceId())) {
            continue;
        }
        const ThemeColors theme = m_settings.customColors;
        inst->mutateItems([&](LayoutItem& item) {
            if (!item.settingKey.isEmpty() && !item.interactive) {
                item.label = m_settings.displayValue(item.settingKey);
            }
            QColor sw;
            if (item.settingKey == QLatin1String("themeVariant")) {
                sw = theme.bgSurface;
            } else if (item.settingKey == QLatin1String("themeForeground")) {
                sw = theme.text;
            } else {
                const QString ck = colorKeyFromItem(item);
                if (!ck.isEmpty()) {
                    sw = m_settings.colorKey(ck);
                }
            }
            if (sw.isValid()) {
                item.style.background = sw;
                item.style.foreground = ThemeColors::contrastOn(sw);
            }
        });
        inst->setGlobalDwellOverride(m_settings.dwellSequence, m_settings.dwellGraceMs,
                                     m_settings.scanGraceMs);
    }
}

LayoutDocument SettingsUi::buildNumpadDocument() const
{
    LayoutDocument doc;
    doc.schemaVersion = 1;
    doc.id = QStringLiteral("settings_numpad_live");
    doc.name = m_numpadTitle;
    doc.description = QStringLiteral("Enter a value, then Save");
    applyLiveEditorChrome(doc);
    doc.grid.columns = 4;
    doc.grid.rows = 7;
    doc.grid.gapPx = 10;
    doc.grid.marginPx = 20;
    doc.placement.width = DimSpec::pixels(520);
    doc.placement.height = DimSpec::pixels(640);

    const EditorSwatch sw = editorSwatch();
    const QString savedLine = (m_numpadReturn == NumpadReturn::Catalog && !m_numpadKey.isEmpty())
                                  ? QStringLiteral("Saved: %1").arg(m_settings.displayValue(m_numpadKey))
                                  : QStringLiteral("Current: %1").arg(m_numpadResetSeed);
    const QString titleCaption =
        m_numpadHint.isEmpty() ? savedLine
                               : QStringLiteral("%1\n%2").arg(savedLine, m_numpadHint);
    doc.items.push_back(makeLabel(QStringLiteral("title"), m_numpadTitle, 0, 0, 4, titleCaption));
    LayoutItem numpadDisplay =
        makeItem(QStringLiteral("display"),
                 m_numpadBuffer.isEmpty() ? QStringLiteral("0") : m_numpadBuffer, 1, 0,
                 LayoutAction::Type::Unknown, {}, sw.value, 4, /*interactive=*/false);
    numpadDisplay.role = QStringLiteral("label");
    numpadDisplay.applyKind();
    numpadDisplay.interactive = false;
    doc.items.push_back(numpadDisplay);
    auto key = [&](const QString& id, const QString& label, int row, int col, const QString& cmd,
                   const QColor& bg = QColor()) {
        doc.items.push_back(makeItem(id, label, row, col, LayoutAction::Type::Command, cmd,
                                     bg.isValid() ? bg : sw.key));
    };

    key(QStringLiteral("d7"), QStringLiteral("7"), 2, 0, QStringLiteral("settings.numpad.digit.7"));
    key(QStringLiteral("d8"), QStringLiteral("8"), 2, 1, QStringLiteral("settings.numpad.digit.8"));
    key(QStringLiteral("d9"), QStringLiteral("9"), 2, 2, QStringLiteral("settings.numpad.digit.9"));
    key(QStringLiteral("back"), QStringLiteral("⌫"), 2, 3,
        QStringLiteral("settings.numpad.backspace"), sw.warn);

    key(QStringLiteral("d4"), QStringLiteral("4"), 3, 0, QStringLiteral("settings.numpad.digit.4"));
    key(QStringLiteral("d5"), QStringLiteral("5"), 3, 1, QStringLiteral("settings.numpad.digit.5"));
    key(QStringLiteral("d6"), QStringLiteral("6"), 3, 2, QStringLiteral("settings.numpad.digit.6"));
    key(QStringLiteral("reset"), QStringLiteral("Reset"), 3, 3,
        QStringLiteral("settings.numpad.reset"), sw.nudge);

    key(QStringLiteral("d1"), QStringLiteral("1"), 4, 0, QStringLiteral("settings.numpad.digit.1"));
    key(QStringLiteral("d2"), QStringLiteral("2"), 4, 1, QStringLiteral("settings.numpad.digit.2"));
    key(QStringLiteral("d3"), QStringLiteral("3"), 4, 2, QStringLiteral("settings.numpad.digit.3"));
    key(QStringLiteral("clear"), QStringLiteral("Clear"), 4, 3,
        QStringLiteral("settings.numpad.clear"), sw.warn);

    key(QStringLiteral("minus"), QStringLiteral("−"), 5, 0,
        QStringLiteral("settings.numpad.minus"), sw.nudge);
    key(QStringLiteral("d0"), QStringLiteral("0"), 5, 1, QStringLiteral("settings.numpad.digit.0"));
    key(QStringLiteral("period"), QStringLiteral("."), 5, 2,
        QStringLiteral("settings.numpad.period"), sw.nudge);
    key(QStringLiteral("comma"), QStringLiteral(","), 5, 3,
        QStringLiteral("settings.numpad.comma"), sw.nudge);

    doc.items.push_back(makeItem(QStringLiteral("save"), QStringLiteral("Save"), 6, 0,
                                 LayoutAction::Type::Command, QStringLiteral("settings.numpad.save"),
                                 sw.save, 2));
    doc.items.push_back(makeItem(QStringLiteral("cancel"), QStringLiteral("Cancel"), 6, 2,
                                 LayoutAction::Type::Command,
                                 QStringLiteral("settings.numpad.cancel"), sw.cancel, 2));
    return doc;
}

bool SettingsUi::openNumericEditor(const QString& settingKey, QString* error)
{
    if (settingKey == QLatin1String("dwellMs")
        || settingKey == QLatin1String("dwellSequence")) {
        return openArrayEditor(settingKey, error);
    }
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

    m_numpad.active = true;
    m_numpad.instanceId = focused->instanceId();
    m_numpad.returnLayoutId = focused->layoutId();
    m_numpadKey = settingKey;
    m_numpadTitle = AppSettings::settingTitle(settingKey);
    m_numpadHint = AppSettings::settingDescription(settingKey);
    m_numpadResetSeed = m_settings.numericBufferSeed(settingKey);
    m_numpadBuffer = m_numpadResetSeed;
    m_numpadReturn = NumpadReturn::Catalog;
    m_numpadArrayIndex = -1;
    m_numpadColorChannel.clear();

    if (!presentNumpad(error)) {
        return false;
    }
    notifyStatus(QStringLiteral("Edit %1").arg(AppSettings::settingTitle(settingKey)));
    return true;
}

void SettingsUi::refreshNumpadDisplay()
{
    if (!m_numpad.active) {
        return;
    }
    auto* inst = m_instances.instance(m_numpad.instanceId);
    if (!inst) {
        return;
    }
    const QString shown = m_numpadBuffer.isEmpty() ? QStringLiteral("0") : m_numpadBuffer;
    inst->setItemText(QStringLiteral("display"), shown, {});
}

void SettingsUi::numpadAppend(const QString& ch)
{
    if (!m_numpad.active) {
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
    if (!m_numpad.active || m_numpadBuffer.isEmpty()) {
        return;
    }
    m_numpadBuffer.chop(1);
    refreshNumpadDisplay();
}

void SettingsUi::numpadClear()
{
    if (!m_numpad.active) {
        return;
    }
    m_numpadBuffer.clear();
    refreshNumpadDisplay();
}

void SettingsUi::numpadReset()
{
    if (!m_numpad.active) {
        return;
    }
    m_numpadBuffer = m_numpadResetSeed;
    refreshNumpadDisplay();
}

void SettingsUi::numpadMinus()
{
    if (!m_numpad.active) {
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
    if (!m_numpad.active) {
        if (error) {
            *error = QStringLiteral("Numeric editor is not open");
        }
        return false;
    }
    const QString buf = m_numpadBuffer.isEmpty() ? QStringLiteral("0") : m_numpadBuffer;
    const NumpadReturn ret = m_numpadReturn;
    const int arrayIndex = m_numpadArrayIndex;
    const QString colorCh = m_numpadColorChannel;
    const QString returnId = m_numpad.returnLayoutId;
    const QString instId = m_numpad.instanceId;
    const QString key = m_numpadKey;

    if (ret == NumpadReturn::Array) {
        bool ok = false;
        const int v = buf.toInt(&ok);
        if (!ok) {
            const QString msg = QStringLiteral("Enter a whole number");
            notifyStatus(msg);
            if (error) {
                *error = msg;
            }
            return false;
        }
        resetNumpad();
        if (arrayIndex >= 0 && arrayIndex < m_arrayDraft.size()) {
            m_arrayDraft[arrayIndex] = qBound(50, v, 10000);
        }
        refreshArrayEditor();
        notifyStatus(QStringLiteral("Step %1 = %2 ms").arg(arrayIndex + 1).arg(v));
        return true;
    }
    if (ret == NumpadReturn::Color) {
        bool ok = false;
        const int v = buf.toInt(&ok);
        if (!ok) {
            const QString msg = QStringLiteral("Enter a whole number");
            notifyStatus(msg);
            if (error) {
                *error = msg;
            }
            return false;
        }
        resetNumpad();
        colorSetChannel(colorCh, v);
        refreshColorPicker();
        return true;
    }

    QString err;
    if (!m_settings.applyNumericBuffer(key, buf, &err)) {
        notifyStatus(err);
        if (error) {
            *error = err;
        }
        return false;
    }
    const QString savedTitle = AppSettings::settingTitle(key);
    const QString savedValue = m_settings.displayValue(key);
    apply(true);

    resetNumpad();
    if (!returnEditorInstance(instId, returnId, error)) {
        return false;
    }
    notifyStatus(QStringLiteral("Saved %1 = %2").arg(savedTitle, savedValue));
    return true;
}

bool SettingsUi::numpadCancel(QString* error)
{
    if (!m_numpad.active) {
        if (error) {
            *error = QStringLiteral("Numeric editor is not open");
        }
        return false;
    }
    const NumpadReturn ret = m_numpadReturn;
    const QString instId = m_numpad.instanceId;
    const QString returnId = m_numpad.returnLayoutId;
    resetNumpad();

    if (ret == NumpadReturn::Array && m_array.active) {
        refreshArrayEditor();
        notifyStatus(QStringLiteral("Edit cancelled"));
        return true;
    }
    if (ret == NumpadReturn::Color && m_color.active) {
        refreshColorPicker();
        notifyStatus(QStringLiteral("Edit cancelled"));
        return true;
    }
    if (!returnEditorInstance(instId, returnId, error)) {
        notifyStatus(QStringLiteral("Cancelled"));
        return false;
    }
    notifyStatus(QStringLiteral("Edit cancelled"));
    return true;
}

} // namespace gazer
