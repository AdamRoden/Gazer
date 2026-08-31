#include "app/SettingsUi.h"
#include "app/SettingsPageBuild.h"

#include "app/CommandRegistry.h"
#include "layout/PageDim.h"
#include "layout/PageSession.h"
#include "layout/PageTypes.h"
#include "ui/PageHostWindow.h"
#include "ui/Theme.h"
#include "ui/ThemeScheme.h"

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

void stampThemeChoice(PageCell& cell, const AppSettings& settings)
{
    const bool swatch = cell.role.compare(QLatin1String("swatch"), Qt::CaseInsensitive) == 0;
    if (!swatch && cell.role.compare(QLatin1String("choice"), Qt::CaseInsensitive) != 0) {
        return;
    }
    int primary = settings.themePrimaryIndex;
    int secondary = settings.themeSecondaryIndex;
    bool selected = false;
    bool match = false;
    for (const PageAction& a : cell.actions) {
        if (a.type != PageActionType::Command) {
            continue;
        }
        if (a.command.startsWith(QLatin1String("theme.primary."))) {
            bool ok = false;
            primary = a.command.mid(int(QLatin1String("theme.primary.").size())).toInt(&ok);
            if (!ok) {
                return;
            }
            selected = !settings.themeCustom && settings.themePrimaryIndex == primary;
            match = true;
        } else if (a.command.startsWith(QLatin1String("theme.secondary."))) {
            bool ok = false;
            secondary = a.command.mid(int(QLatin1String("theme.secondary.").size())).toInt(&ok);
            if (!ok) {
                return;
            }
            selected = !settings.themeCustom && settings.themeSecondaryIndex == secondary;
            match = true;
        }
    }
    if (!match) {
        return;
    }
    const ThemePalette pal =
        ThemeScheme::resolve(settings.themeAppearance, settings.themeSaturation, primary, secondary,
                             false);
    if (swatch) {
        cell.style.background = pal.progress;
        cell.style.foreground.reset();
        cell.style.borderColor.reset();
        cell.style.progressColor.reset();
        cell.style.thickness = PageBox::all(selected ? 3.0 : 1.0);
        return;
    }
    cell.style.background = pal.colors.bgMain;
    cell.style.foreground = pal.colors.accent;
    cell.style.borderColor = pal.colors.bgSurface;
    cell.style.progressColor = pal.progress;
    cell.style.thickness = PageBox::all(selected ? 2.6 : 1.0);
    cell.style.radius = PageBox::all(14.0);
}

void stampPageCell(PageCell& cell, const AppSettings& settings, const ThemeColors& swatchTheme,
                   const ThemeColors& liveTheme)
{
    stampThemeChoice(cell, settings);
    stampSettingVisuals(cell.label, cell.isInteractive(), cell.settingKey, cell.id,
                        colorKeyFromPageCell(cell), cell.style.background, cell.style.foreground,
                        settings, swatchTheme);
    if (cell.role.compare(QLatin1String("value"), Qt::CaseInsensitive) != 0) {
        return;
    }
    if (cell.style.background && cell.style.background->isValid()
        && cell.style.background->alpha() > 0) {
        return;
    }
    QColor chip = liveTheme.bgMain.isValid() ? liveTheme.bgMain : QColor(10, 10, 11);
    chip.setAlpha(255);
    cell.style.background = chip;
    if (!cell.style.foreground) {
        cell.style.foreground = ThemeColors::contrastOn(chip);
    }
}

void stampGrid(PageGrid& grid, const AppSettings& settings, const ThemeColors& swatchTheme,
              const ThemeColors& liveTheme)
{
    for (PageCell& cell : grid.cells) {
        stampPageCell(cell, settings, swatchTheme, liveTheme);
    }
    for (PageGrid& sub : grid.subGrids) {
        stampGrid(sub, settings, swatchTheme, liveTheme);
    }
}

void stampNamedSurface(PageDocument& doc, const QString& styleId, const QColor& fill)
{
    const auto it = doc.styles.find(styleId);
    if (it == doc.styles.end()) {
        return;
    }
    QColor bg = fill.isValid() ? fill : QColor(18, 19, 20);
    bg.setAlpha(255);
    it->background = bg;
}

} // namespace

void SettingsUi::decoratePage(PageDocument& doc) const
{
    if (!doc.id.startsWith(QLatin1String("main_settings"))) {
        return;
    }
    const ThemeColors live = m_settings.resolvedTheme();
    stampNamedSurface(doc, QStringLiteral("group"), live.bgSurface);
    stampNamedSurface(doc, QStringLiteral("tabbar"), live.bgSurface);
    const ThemeColors swatch = live;
    for (PageGrid& g : doc.grids) {
        stampGrid(g, m_settings, swatch, live);
    }
}

} // namespace gazer
