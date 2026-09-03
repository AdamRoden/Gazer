#include "app/SettingsUi.h"
#include "app/SettingsPageBuild.h"
#include "app/SettingsUiInternal.h"

#include "layout/PageSession.h"
#include "ui/PageHostWindow.h"
#include "ui/Theme.h"
#include "ui/ThemeScheme.h"

#include <QColor>
#include <QtGlobal>
#include <iterator>

namespace gazer {

using SettingsPageBuild::cell;
using SettingsPageBuild::initGrid;
using SettingsUiInternal::ColorAxis;
using SettingsUiInternal::findColorAxis;
using SettingsUiInternal::fromPct255;
using SettingsUiInternal::kColorAxes;
using SettingsUiInternal::kLiveColor;
using SettingsUiInternal::kLiveHex;
using SettingsUiInternal::pct255;

namespace {

void addColorAxis(PageGrid& grid, const ColorAxis& axis, int row, int trackSpan, int incCol,
                  const SettingsUi::EditorSwatch& pal)
{
    grid.cells.push_back(cell(QStringLiteral("edit_%1").arg(QLatin1String(axis.id)),
                              QStringLiteral("Edit"), row, 0,
                              QStringLiteral("settings.color.scrub.%1").arg(QLatin1String(axis.id)),
                              pal.edit, 1, {}, {}, QStringLiteral("edit")));
    grid.cells.push_back(cell(QStringLiteral("dec_%1").arg(QLatin1String(axis.id)),
                              QStringLiteral("−"), row, 1,
                              QStringLiteral("settings.color.nudge.%1.dec").arg(QLatin1String(axis.id)),
                              pal.nudge));
    grid.cells.push_back(cell(QStringLiteral("track_%1").arg(QLatin1String(axis.id)),
                              QLatin1String(axis.title), row, 2, {}, QColor(), trackSpan,
                              QStringLiteral("slider"), QLatin1String(axis.id)));
    grid.cells.push_back(cell(QStringLiteral("inc_%1").arg(QLatin1String(axis.id)),
                              QStringLiteral("+"), row, incCol,
                              QStringLiteral("settings.color.nudge.%1.inc").arg(QLatin1String(axis.id)),
                              pal.nudge));
}

} // namespace

void SettingsUi::applyPreviewColor()
{
    if (PageHostWindow* w = m_pages.window()) {
        w->setPreviewColor(m_colorDraft);
    }
}

void SettingsUi::colorSyncFromHsv()
{
    QColor c = QColor::fromHsv(qBound(0, m_colorH, 359), qBound(0, m_colorS, 255),
                               qBound(0, m_colorV, 255), qBound(0, m_colorA, 255));
    m_colorDraft = c;
    m_colorR = c.red();
    m_colorG = c.green();
    m_colorB = c.blue();
}

void SettingsUi::colorSyncFromRgb()
{
    m_colorDraft = QColor(qBound(0, m_colorR, 255), qBound(0, m_colorG, 255),
                          qBound(0, m_colorB, 255), qBound(0, m_colorA, 255));
    int h = 0, s = 0, v = 0, a = 255;
    m_colorDraft.getHsv(&h, &s, &v, &a);
    if (h >= 0) {
        m_colorH = h;
    }
    m_colorS = s;
    m_colorV = v;
    m_colorA = a;
}

void SettingsUi::loadColorDraft(const QColor& c)
{
    m_colorDraft = c.isValid() ? c : ThemeColors::defaultProgressColor();
    m_colorR = m_colorDraft.red();
    m_colorG = m_colorDraft.green();
    m_colorB = m_colorDraft.blue();
    m_colorA = m_colorDraft.alpha();
    int h = 0, s = 0, v = 0, a = 255;
    m_colorDraft.getHsv(&h, &s, &v, &a);
    if (h >= 0) {
        m_colorH = h;
    }
    m_colorS = s;
    m_colorV = v;
}

int SettingsUi::colorShownValue(const QString& channel) const
{
    const ColorAxis* axis = findColorAxis(channel);
    if (!axis) {
        return 0;
    }
    switch (axis->kind) {
    case ColorAxis::Kind::Hue:
        return m_colorH;
    case ColorAxis::Kind::Sat:
        return pct255(m_colorS);
    case ColorAxis::Kind::Val:
        return pct255(m_colorV);
    case ColorAxis::Kind::Red:
        return m_colorR;
    case ColorAxis::Kind::Green:
        return m_colorG;
    case ColorAxis::Kind::Blue:
        return m_colorB;
    case ColorAxis::Kind::Alpha:
        return pct255(m_colorA);
    }
    return 0;
}

bool SettingsUi::applyColorShownValue(const QString& channel, int value)
{
    const ColorAxis* axis = findColorAxis(channel);
    if (!axis) {
        return false;
    }
    switch (axis->kind) {
    case ColorAxis::Kind::Hue:
        m_colorH = qBound(0, value, 359);
        colorSyncFromHsv();
        break;
    case ColorAxis::Kind::Sat:
        m_colorS = fromPct255(value);
        colorSyncFromHsv();
        break;
    case ColorAxis::Kind::Val:
        m_colorV = fromPct255(value);
        colorSyncFromHsv();
        break;
    case ColorAxis::Kind::Red:
        m_colorR = qBound(0, value, 255);
        colorSyncFromRgb();
        break;
    case ColorAxis::Kind::Green:
        m_colorG = qBound(0, value, 255);
        colorSyncFromRgb();
        break;
    case ColorAxis::Kind::Blue:
        m_colorB = qBound(0, value, 255);
        colorSyncFromRgb();
        break;
    case ColorAxis::Kind::Alpha:
        m_colorA = fromPct255(value);
        m_colorDraft.setAlpha(m_colorA);
        break;
    }
    if (m_color.active && m_colorDraft.isValid() && !m_colorPickerKey.isEmpty()) {
        m_colorPending.insert(m_colorPickerKey, m_colorDraft);

    }
    return true;
}

void SettingsUi::colorSetChannel(const QString& channel, int value)
{
    applyColorShownValue(channel, value);
}

void SettingsUi::colorNudge(const QString& channel, int dir)
{
    if (m_scrub.active) {
        endSliderScrub(true);
    }
    applyColorShownValue(channel, colorShownValue(channel) + dir);
    refreshColorPicker();
}

AppSettings SettingsUi::draftThemeSettings() const
{
    AppSettings tmp = m_settings;
    QHash<QString, QColor> pending = m_colorPending;
    if (m_color.active && m_colorDraft.isValid() && !m_colorPickerKey.isEmpty()) {
        pending.insert(m_colorPickerKey, m_colorDraft);
    }
    bool anyTheme = false;
    for (auto it = pending.constBegin(); it != pending.constEnd(); ++it) {
        if (!tmp.setColorKey(it.key(), it.value(), /*rebuildPalette=*/false)) {
            continue;
        }
        anyTheme = anyTheme || AppSettings::isThemeSeedKey(it.key());
    }
    if (anyTheme) {
        tmp.themeCustom = true;
    }
    return tmp;
}

ThemePalette SettingsUi::draftThemePalette() const
{
    const AppSettings tmp = draftThemeSettings();
    if (m_colorPickerPage == ColorPickerPage::Accent) {
        return ThemeScheme::fluent(tmp.themeAppearance, tmp.themeSaturation, tmp.themeSeeds().primary,
                                   tmp.themeSeeds().secondary);
    }
    return tmp.resolvedPalette();
}

QColor SettingsUi::suggestedDraftColor(const QString& colorKey) const
{
    return draftThemeSettings().suggestedThemeColor(colorKey);
}

void SettingsUi::colorSetRolesMode(bool roles)
{
    if (!m_color.active) {
        return;
    }
    m_colorPickerPage = roles ? ColorPickerPage::Roles : ColorPickerPage::Accent;
    if (!roles && AppSettings::isThemeSeedKey(m_colorPickerKey)
        && m_colorPickerKey != QLatin1String("customPrimaryColor")) {
        (void)selectColorTarget(QStringLiteral("customPrimaryColor"), nullptr);
        return;
    }
    refreshColorPicker();
    notifyStatus(roles ? QStringLiteral("Editing theme roles")
                       : QStringLiteral("Editing accent"));
}

void SettingsUi::colorApplyPreset(int index)
{
    if (index < 0 || index >= ThemeScheme::brandCount() || !m_color.active) {
        return;
    }
    QColor c = ThemeScheme::brandAccent(index, m_settings.themeAppearance);
    c.setAlpha(m_colorA);
    loadColorDraft(c);
    if (!m_colorPickerKey.isEmpty()) {
        m_colorPending.insert(m_colorPickerKey, m_colorDraft);
    }
    refreshColorPicker();
}

void SettingsUi::applySuggestedColor(const QString& colorKey)
{
    const QColor sug = suggestedDraftColor(colorKey);
    if (!sug.isValid()) {
        return;
    }
    if (m_color.active) {
        m_colorPending.insert(colorKey, sug);
        if (colorKey == m_colorPickerKey) {
            loadColorDraft(sug);
        }
        refreshColorPicker();
        notifyStatus(QStringLiteral("Suggested %1").arg(AppSettings::settingTitle(colorKey)));
        return;
    }
    (void)m_settings.setColorKey(colorKey, sug, true);
    apply(true);
    notifyStatus(QStringLiteral("%1 = suggested").arg(AppSettings::settingTitle(colorKey)));
}

void SettingsUi::colorUseSaved(const QString& savedKey)
{
    if (!m_color.active || !AppSettings::isColorKey(savedKey)) {
        return;
    }
    loadColorDraft(m_settings.colorKey(savedKey));
    m_colorPending.insert(m_colorPickerKey, m_colorDraft);
    refreshColorPicker();
}

bool SettingsUi::openFlashCustom(QString* error)
{
    if (!openColorPicker(QStringLiteral("flashColor"), error)) {
        return false;
    }
    m_flashCustomSetMode = false;
    if (m_settings.flashUseForeground) {
        m_settings.flashUseForeground = false;
        m_flashCustomSetMode = true;
        apply(true);
    }
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
    m_colorPickerKey = colorKey;
    if (m_colorPickerPage != ColorPickerPage::Roles) {
        m_colorPickerPage = AppSettings::isThemeSeedKey(colorKey) ? ColorPickerPage::Accent
                                                                  : ColorPickerPage::Generic;
    }
    m_colorPending.clear();
    loadColorDraft(m_settings.colorKey(colorKey));
    m_colorPending.insert(colorKey, m_colorDraft);
    if (!presentLive(m_color, QLatin1String(kLiveColor), buildColorDocument(), error)) {
        m_color.reset();
        return false;
    }
    applyPreviewColor();
    notifyStatus(QStringLiteral("Pick color for %1").arg(AppSettings::settingTitle(colorKey)));
    return true;
}

bool SettingsUi::selectColorTarget(const QString& colorKey, QString* error)
{
    if (!m_color.active) {
        return openColorPicker(colorKey, error);
    }
    if (!AppSettings::isColorKey(colorKey) || !m_settings.colorKey(colorKey).isValid()) {
        if (error) {
            *error = QStringLiteral("Not a color setting");
        }
        return false;
    }
    if (colorKey == m_colorPickerKey) {
        return true;
    }
    if (m_scrub.active) {
        endSliderScrub(true);
    }
    if (m_colorDraft.isValid() && !m_colorPickerKey.isEmpty()) {
        m_colorPending.insert(m_colorPickerKey, m_colorDraft);
    }
    QColor next = m_colorPending.value(colorKey);
    if (!next.isValid()) {
        next = m_settings.colorKey(colorKey);
    }
    m_colorPickerKey = colorKey;
    loadColorDraft(next);
    m_colorPending.insert(colorKey, m_colorDraft);
    refreshColorPicker();
    notifyStatus(QStringLiteral("Editing %1").arg(AppSettings::settingTitle(colorKey)));
    return true;
}

PageDocument SettingsUi::buildColorDocument() const
{
    if (m_colorPickerPage == ColorPickerPage::Accent) {
        return buildAccentColorDocument();
    }
    return buildRolesColorDocument();
}

PageDocument SettingsUi::buildAccentColorDocument() const
{
    PageDocument doc;
    doc.id = QLatin1String(kLiveColor);
    doc.name = AppSettings::settingTitle(m_colorPickerKey);
    initGrid(doc, 12, 9, 1400, 980, 8, 20, m_settings.resolvedTheme());
    PageGrid& grid = doc.grids[0];
    const EditorSwatch pal = editorSwatch();

    const ThemeBrandInfo* brands = ThemeScheme::brands();
    const int nBrands = ThemeScheme::brandCount();
    const int span = qMax(1, 12 / nBrands);
    for (int i = 0; i < nBrands; ++i) {
        grid.cells.push_back(cell(QStringLiteral("preset_%1").arg(i), QLatin1String(brands[i].name),
                                  0, i * span, QStringLiteral("settings.color.preset.%1").arg(i),
                                  brands[i].colorFor(m_settings.themeAppearance), span));
    }
    int axisRow = 1;
    for (const ColorAxis& axis : kColorAxes) {
        if (axis.kind != ColorAxis::Kind::Hue && axis.kind != ColorAxis::Kind::Sat
            && axis.kind != ColorAxis::Kind::Val) {
            continue;
        }
        addColorAxis(grid, axis, axisRow, 9, 11, pal);
        ++axisRow;
    }
    grid.cells.push_back(cell(QStringLiteral("swatch"), QStringLiteral("Accent"), 4, 0, {},
                              m_colorDraft, 6, QStringLiteral("preview")));
    const QString hex = m_colorDraft.name(QColor::HexArgb).toUpper();
    grid.cells.push_back(cell(QStringLiteral("hex"), hex, 4, 6,
                              QStringLiteral("settings.color.editHex"), pal.value, 6));
    grid.cells.push_back(cell(QStringLiteral("roles"), QStringLiteral("Edit roles"), 6, 0,
                              QStringLiteral("settings.color.roles"), pal.edit, 4, {}, {},
                              QStringLiteral("edit")));
    grid.cells.push_back(cell(QStringLiteral("save"), QStringLiteral("Save"), 6, 4,
                              QStringLiteral("settings.color.save"), pal.save, 4));
    grid.cells.push_back(cell(QStringLiteral("cancel"), QStringLiteral("Cancel"), 6, 8,
                              QStringLiteral("settings.color.cancel"), pal.cancel, 4));
    return doc;
}

PageDocument SettingsUi::buildRolesColorDocument() const
{
    PageDocument doc;
    doc.id = QLatin1String(kLiveColor);
    doc.name = AppSettings::settingTitle(m_colorPickerKey);
    initGrid(doc, 12, 9, 1400, 980, 8, 20, m_settings.resolvedTheme());
    PageGrid& grid = doc.grids[0];
    const EditorSwatch pal = editorSwatch();

    const AppSettings draft = draftThemeSettings();
    const ThemePalette themePal = draft.resolvedPalette();
    const bool themePicker = m_colorPickerPage == ColorPickerPage::Roles
                             || AppSettings::isThemeSeedKey(m_colorPickerKey);
    const int trackSpan = themePicker ? 5 : 9;
    const int incCol = themePicker ? 7 : 11;
    for (int i = 0; i < int(std::size(kColorAxes)); ++i) {
        addColorAxis(grid, kColorAxes[i], i, trackSpan, incCol, pal);
    }

    if (themePicker) {
        struct RoleRow {
            const char* id;
            const char* label;
            const char* caption;
            const char* colorKey;
            QColor color;
            int row;
        };
        const RoleRow roles[] = {
            {"use_window", "Window", "Board background", "customBgColor", themePal.colors.bgMain, 0},
            {"use_surface", "Surface", "Panels and cells", "customSurfaceColor",
             themePal.colors.bgSurface, 1},
            {"use_accent", "Accent", "Activated items", "customPrimaryColor", themePal.colors.accent,
             2},
            {"use_progress", "Progress", "Dwell and mouse-move", "customSecondaryColor",
             themePal.progress, 3},
            {"use_text", "Foreground", "Body text", "customTextColor", themePal.colors.text, 4},
            {"use_danger", "Danger", "Cancel / destructive", "customDangerColor",
             themePal.colors.danger, 5},
        };
        for (const RoleRow& role : roles) {
            PageCell sw =
                cell(QLatin1String(role.id), QLatin1String(role.label), role.row, 8,
                     QStringLiteral("settings.color.select.%1").arg(QLatin1String(role.colorKey)),
                     role.color, 3, {}, QLatin1String(role.caption));
            sw.settingKey = QLatin1String(role.colorKey);
            sw.activeState =
                QStringLiteral("setting.color.editing.%1").arg(QLatin1String(role.colorKey));
            grid.cells.push_back(sw);
            const QColor suggested = draft.suggestedThemeColor(QLatin1String(role.colorKey));
            grid.cells.push_back(cell(
                QStringLiteral("suggest_%1").arg(QLatin1String(role.colorKey)),
                QStringLiteral("Reset"), role.row, 11,
                QStringLiteral("settings.color.suggest.%1").arg(QLatin1String(role.colorKey)),
                suggested));
        }
    }

    const QString hex = m_colorDraft.name(QColor::HexArgb).toUpper();
    if (themePicker) {
        grid.cells.push_back(cell(QStringLiteral("accent"), QStringLiteral("Accent"), 7, 0,
                                  QStringLiteral("settings.color.accent"), pal.edit, 4));
        grid.cells.push_back(cell(QStringLiteral("hex"), hex, 7, 4,
                                  QStringLiteral("settings.color.editHex"), pal.value, 4));
        grid.cells.push_back(cell(QStringLiteral("save"), QStringLiteral("Save"), 8, 0,
                                  QStringLiteral("settings.color.save"), pal.save, 6));
        grid.cells.push_back(cell(QStringLiteral("cancel"), QStringLiteral("Cancel"), 8, 6,
                                  QStringLiteral("settings.color.cancel"), pal.cancel, 6));
    } else {
        grid.cells.push_back(cell(QStringLiteral("hex"), hex, 7, 0,
                                  QStringLiteral("settings.color.editHex"), pal.value, 8));
        grid.cells.push_back(cell(QStringLiteral("save"), QStringLiteral("Save"), 8, 0,
                                  QStringLiteral("settings.color.save"), pal.save, 4));
        grid.cells.push_back(cell(QStringLiteral("cancel"), QStringLiteral("Cancel"), 8, 4,
                                  QStringLiteral("settings.color.cancel"), pal.cancel, 4));
    }
    return doc;
}

void SettingsUi::refreshColorPicker()
{
    if (m_hexActive || m_numpad.active) {
        return;
    }
    QString err;
    if (!presentLive(m_color, QLatin1String(kLiveColor), buildColorDocument(), &err)) {
        notifyStatus(err);
        return;
    }
    applyPreviewColor();
}

void SettingsUi::closeColorPicker()
{
    if (!m_color.active) {
        return;
    }
    if (m_scrub.active) {
        m_scrub.reset();
        m_scrubDeadlineMs = -1;
        m_scrubLastSampleMs = -1;
        m_scrubDwell.reset();
        if (PageHostWindow* w = m_pages.window()) {
            w->clearSliderScrub();
        }
    }
    if (m_flashCustomSetMode) {
        m_settings.flashUseForeground = true;
        apply(true);
    }
    m_colorPending.clear();
    m_flashCustomSetMode = false;
    m_colorPickerPage = ColorPickerPage::Generic;
    m_colorPickerKey.clear();
    m_hexBuffer.clear();
    if (m_hexActive) {
        m_pages.closePage(QLatin1String(kLiveHex));
        m_hexActive = false;
    }
    closeLive(m_color);
}

bool SettingsUi::colorSave(QString* error)
{
    if (!m_color.active) {
        if (error) {
            *error = QStringLiteral("Color picker is not open");
        }
        return false;
    }
    if (m_colorDraft.isValid()) {
        m_colorPending.insert(m_colorPickerKey, m_colorDraft);
    }
    bool anyTheme = false;
    for (auto it = m_colorPending.constBegin(); it != m_colorPending.constEnd(); ++it) {
        if (!m_settings.setColorKey(it.key(), it.value(), /*rebuildPalette=*/false)) {
            if (error) {
                *error = QStringLiteral("Could not apply color");
            }
            return false;
        }
        anyTheme = anyTheme || AppSettings::isThemeSeedKey(it.key());
    }
    if (anyTheme) {
        m_settings.themeCustom = true;
        if (m_colorPickerPage == ColorPickerPage::Accent) {
            m_settings.applyCustomPalette(false);
        } else {
            m_settings.applyTheme();
        }
    }
    apply(true);
    const QString title = AppSettings::settingTitle(m_colorPickerKey);
    const QString hex = m_colorDraft.name(QColor::HexArgb).toUpper();
    m_flashCustomSetMode = false;
    closeColorPicker();
    notifyStatus(QStringLiteral("%1 = %2").arg(title, hex));
    return true;
}

bool SettingsUi::colorEditChannel(const QString& channel, QString* error)
{
    if (!m_color.active) {
        if (error) {
            *error = QStringLiteral("Color picker is not open");
        }
        return false;
    }
    if (m_scrub.active) {
        endSliderScrub(true);
    }
    const ColorAxis* axis = findColorAxis(channel);
    if (!axis) {
        if (error) {
            *error = QStringLiteral("Unknown channel");
        }
        return false;
    }
    m_numpadKey.clear();
    m_numpadTitle = QLatin1String(axis->title);
    m_numpadHint = QLatin1String(axis->hint);
    m_numpadResetSeed = QString::number(colorShownValue(QLatin1String(axis->id)));
    m_numpadBuffer = m_numpadResetSeed;
    m_numpadReturn = NumpadReturn::Color;
    m_numpadColorChannel = QLatin1String(axis->id);
    m_numpadArrayIndex = -1;
    if (!presentNumpad(error)) {
        return false;
    }
    notifyStatus(QStringLiteral("Edit %1").arg(QLatin1String(axis->label)));
    return true;
}

} // namespace gazer
