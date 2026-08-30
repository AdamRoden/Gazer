#include "app/SettingsUi.h"
#include "app/SettingsPageBuild.h"

#include "app/CommandRegistry.h"
#include "layout/PageDim.h"
#include "layout/PageSession.h"
#include "ui/PageHostWindow.h"
#include "ui/Theme.h"

#include <QColor>
#include <QKeyEvent>
#include <QObject>
#include <QtGlobal>
#include <QVector>
#include <optional>

namespace gazer {

SettingsUi::SettingsUi(AppSettings& settings, CommandRegistry& commands, PageSession& pages)
    : m_settings(settings)
    , m_commands(commands)
    , m_pages(pages)
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

bool SettingsUi::presentLive(LiveBoard& board, const QString& id, PageDocument doc, QString* error)
{
    board.active = true;
    board.pageId = id;
    doc.id = id;
    return m_pages.attachDocument(std::move(doc), error);
}

void SettingsUi::closeLive(LiveBoard& board)
{
    unbindEditorKeyboard();
    if (!board.pageId.isEmpty()) {
        m_pages.closePage(board.pageId);
    }
    board.reset();
}

void SettingsUi::unbindEditorKeyboard()
{
    QObject::disconnect(m_editorKeyConn);
    m_editorKeyConn = {};
    if (PageHostWindow* w = m_pages.window()) {
        w->setInputFocusEnabled(false);
    }
}

void SettingsUi::bindEditorKeyboard()
{
    unbindEditorKeyboard();
    PageHostWindow* w = m_pages.window();
    if (!w) {
        return;
    }
    w->setInputFocusEnabled(true);
    m_editorKeyConn = QObject::connect(w, &PageHostWindow::keyPressed, w,
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

namespace {

void stampSettingVisuals(QString& label, bool interactive, const QString& settingKey,
                         const QString& id, const QString& colorKey,
                         std::optional<QColor>& background, std::optional<QColor>& foreground,
                         const AppSettings& settings, const ThemeColors& theme)
{
    if (!settingKey.isEmpty() && !interactive) {
        label = settings.displayValue(settingKey);
    }
    if (!interactive && !settingKey.isEmpty() && id.contains(QLatin1String("desc"))) {
        label = AppSettings::settingDescription(settingKey);
    }
    QColor sw;
    if (settingKey == QLatin1String("themeVariant")) {
        sw = theme.bgSurface;
    } else if (settingKey == QLatin1String("themeForeground")) {
        sw = theme.text;
    } else if (!colorKey.isEmpty()) {
        sw = settings.colorKey(colorKey);
    }
    if (sw.isValid()) {
        background = sw;
        foreground = ThemeColors::contrastOn(sw);
    }
}

QString colorKeyFromPageCell(const PageCell& cell)
{
    if (AppSettings::isColorKey(cell.settingKey)) {
        return cell.settingKey;
    }
    for (const PageAction& a : cell.actions) {
        if (a.type != PageActionType::Command) {
            continue;
        }
        const QLatin1String prefix("settings.edit.color.");
        if (a.command.startsWith(prefix)) {
            return a.command.mid(int(prefix.size()));
        }
    }
    return {};
}

void stampPageCell(PageCell& cell, const AppSettings& settings, const ThemeColors& theme)
{
    stampSettingVisuals(cell.label, cell.isInteractive(), cell.settingKey, cell.id,
                        colorKeyFromPageCell(cell), cell.style.background, cell.style.foreground,
                        settings, theme);
}

void stampGrid(PageGrid& grid, const AppSettings& settings, const ThemeColors& theme)
{
    for (PageCell& cell : grid.cells) {
        stampPageCell(cell, settings, theme);
    }
    for (PageGrid& sub : grid.subGrids) {
        stampGrid(sub, settings, theme);
    }
}

} // namespace

void SettingsUi::decoratePage(PageDocument& doc) const
{
    if (!doc.id.startsWith(QLatin1String("main_settings"))) {
        return;
    }
    const ThemeColors theme = m_settings.customColors;
    for (PageGrid& g : doc.grids) {
        stampGrid(g, m_settings, theme);
    }
}

} // namespace gazer
