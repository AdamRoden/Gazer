#include "app/SettingsUi.h"

#include "app/CommandRegistry.h"
#include "assist/LookToMaps.h"
#include "assist/SpeechSecrets.h"
#include "layout/PageCompose.h"
#include "layout/PageEdit.h"
#include "layout/PageSession.h"
#include "layout/PageTypes.h"
#include "ui/PageHostWindow.h"
#include "ui/MaterialPalette.h"
#include "ui/Theme.h"
#include "ui/ThemeScheme.h"

#include <QColor>
#include <QKeyEvent>
#include <QObject>
#include <QtGlobal>
#include <QVector>

namespace gazer {

SettingsUi::SettingsUi(AppSettings& settings, CommandRegistry& commands, PageSession& pages,
                       SpeechSecrets& secrets, ElevenClient& eleven)
    : m_settings(settings)
    , m_commands(commands)
    , m_pages(pages)
    , m_secrets(secrets)
    , m_eleven(eleven)
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
    if (&board == &m_headMap) {
        endCurveScrub();
    }
    if (&board == &m_lookToMap && m_lookTo) {
        m_lookTo->clearPreview();
    }
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
        return;
    }
}

SettingsUi::EditorSwatch SettingsUi::editorSwatch() const
{
    const ThemeColors t = m_settings.resolvedTheme();
    EditorSwatch s;
    s.key = t.defaultActive();
    s.save = t.accent;
    s.cancel = t.danger;
    s.nudge = t.defaultActive();
    s.warn = t.defaultHover();
    s.add = t.accentHover;
    s.value = t.bgMain;
    s.edit = t.accent;
    return s;
}

namespace {

void stampSettingVisuals(QString& label, bool interactive, const QString& settingKey,
                         const QString& id, const QString& colorKey, PageColor& background,
                         PageColor& foreground, const AppSettings& settings)
{
    if (!settingKey.isEmpty() && !interactive) {
        label = settings.displayValue(settingKey);
    }
    if (!interactive && !settingKey.isEmpty() && id.contains(QLatin1String("desc"))) {
        label = AppSettings::settingDescription(settingKey);
    }
    QColor sw;
    if (!colorKey.isEmpty()) {
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

void stampPaletteSwatch(PageCell& cell, const QColor& source, const QColor& secondary)
{
    if (!cell.id.startsWith(QLatin1String("pal_"))) {
        return;
    }
    const QString rest = cell.id.mid(4);
    const int split = rest.lastIndexOf(QLatin1Char('_'));
    if (split <= 0) {
        return;
    }
    MaterialPalette::Family family = MaterialPalette::Family::Primary;
    if (!MaterialPalette::parseFamily(rest.left(split), &family)) {
        return;
    }
    bool ok = false;
    const int index = rest.mid(split + 1).toInt(&ok);
    if (!ok) {
        return;
    }
    const MaterialPalette::Palettes pals = MaterialPalette::generate(source);
    const QColor c = MaterialPalette::shade(pals, family, index);
    cell.style.background = c;
    cell.style.foreground = ThemeColors::contrastOn(c);
    cell.caption.clear();
    if (MaterialPalette::sameRgb(c, secondary)) {
        cell.label = QStringLiteral("S");
        cell.style.thickness = PageBox::all(3.0);
    } else {
        cell.label.clear();
        cell.style.thickness = PageBox::all(0.0);
    }
}

void stampBrightnessShade(PageCell& cell, const AppSettings& settings, const QColor& primary,
                          const QColor& secondary)
{
    if (!cell.id.startsWith(QLatin1String("shade_bg_"))) {
        return;
    }
    bool ok = false;
    const int index = cell.id.mid(9).toInt(&ok);
    if (!ok) {
        return;
    }
    const ThemePalette pal =
        ThemeScheme::fluent(settings.themeAppearance, settings.themeSaturation, primary, secondary,
                            index, settings.surfaceTintColor());
    cell.style.background = pal.colors.bgMain;
    cell.style.borderColor = pal.colors.bgAt(95);
    cell.style.foreground = pal.colors.accent;
    cell.style.progressColor = pal.progress;
}

void stampTintChip(PageCell& cell, const AppSettings& settings, const QColor& primary)
{
    if (!cell.id.startsWith(QLatin1String("tint_"))) {
        return;
    }
    const QString id = cell.id.mid(5);
    QColor c;
    if (id == QLatin1String("none")) {
        const ThemeAppearance plain = themeAppearanceIsDark(settings.themeAppearance)
                                          ? ThemeAppearance::Dark
                                          : ThemeAppearance::Light;
        const ThemePalette pal =
            ThemeScheme::fluent(plain, settings.themeSaturation, primary, {},
                                settings.themeBrightness, {});
        c = pal.colors.bgMain;
    } else {
        MaterialPalette::Family family = MaterialPalette::Family::Primary;
        if (!MaterialPalette::parseFamily(id, &family)) {
            return;
        }
        c = MaterialPalette::shade(MaterialPalette::generate(primary), family, 5);
    }
    if (!c.isValid()) {
        return;
    }
    cell.style.background = c;
    cell.style.foreground = ThemeColors::contrastOn(c);
}

void stampSelectedWells(PageCell& cell, const QColor& source, const QColor& primary,
                        const QColor& secondary)
{
    if (cell.id == QLatin1String("hex")) {
        cell.label = source.name(QColor::HexRgb).toUpper();
        cell.style.background = source;
        cell.style.foreground = ThemeColors::contrastOn(source);
        return;
    }
    if (cell.id == QLatin1String("p_swatch")) {
        cell.style.background = primary;
        cell.style.foreground = ThemeColors::contrastOn(primary);
        cell.caption = primary.name(QColor::HexRgb).toUpper();
        return;
    }
    if (cell.id == QLatin1String("s_swatch")) {
        cell.style.background = secondary;
        cell.style.foreground = ThemeColors::contrastOn(secondary);
        cell.caption = secondary.name(QColor::HexRgb).toUpper();
    }
}

void stampPageCell(PageCell& cell, const AppSettings& settings, const ThemeColors& swatchTheme,
                   const ThemeColors& liveTheme, const QColor& source, const QColor& primary,
                   const QColor& secondary)
{
    stampPaletteSwatch(cell, source, secondary);
    stampBrightnessShade(cell, settings, primary, secondary);
    stampTintChip(cell, settings, primary);
    stampSelectedWells(cell, source, primary, secondary);
    stampSettingVisuals(cell.label, cell.isInteractive(), cell.settingKey, cell.id,
                        colorKeyFromPageCell(cell), cell.style.background, cell.style.foreground,
                        settings);
    if (cell.role.compare(QLatin1String("value"), Qt::CaseInsensitive) != 0) {
        return;
    }
    if (cell.style.background.isSet()) {
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
              const ThemeColors& liveTheme, const QColor& source, const QColor& primary,
              const QColor& secondary)
{
    for (PageCell& cell : grid.cells) {
        stampPageCell(cell, settings, swatchTheme, liveTheme, source, primary, secondary);
    }
    for (PageGrid& sub : grid.subGrids) {
        stampGrid(sub, settings, swatchTheme, liveTheme, source, primary, secondary);
    }
}

} // namespace

void SettingsUi::decoratePage(PageDocument& doc)
{
    if (!doc.id.startsWith(QLatin1String("main_settings"))) {
        return;
    }
    if (doc.id.startsWith(QLatin1String("main_settings_"))
        && doc.id != QLatin1String("main_settings_theme") && !PageCompose::findSrcSlot(doc)
        && isInlineThemeEditor()) {
        stopInlineThemeEditor();
    }
    const ThemeColors live = m_settings.resolvedTheme();
    const ThemeColors swatch = live;
    QColor primary = m_colorPending.value(QStringLiteral("customPrimaryColor"));
    if (!primary.isValid()) {
        primary = live.accent;
    }
    const QColor source = primary;
    QColor secondary = m_colorPending.value(QStringLiteral("customSecondaryColor"));
    if (!secondary.isValid()) {
        secondary = m_settings.resolvedPalette().progress;
    }
    for (PageGrid& g : doc.grids) {
        stampGrid(g, m_settings, swatch, live, source, primary, secondary);
    }
    if (doc.id == QLatin1String("main_settings_speech")) {
        PageEdit::forEachCell(doc, [this](PageGrid&, PageCell& c) {
            if (c.id == QLatin1String("key_status")) {
                c.label = m_settings.elevenApiKeySet
                              ? QStringLiteral("Key set (\u2022\u2022\u2022\u2022%1)")
                                    .arg(m_secrets.lastFour())
                              : QStringLiteral("No API key");
            }
        });
    }
    if (doc.id == QLatin1String("main_settings_head_pose")) {
        decorateHeadPosePage(doc);
    }
}

} // namespace gazer
