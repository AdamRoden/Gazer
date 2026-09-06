#include "app/SettingsUi.h"
#include "app/SettingsPageBuild.h"
#include "app/SettingsUiInternal.h"

#include "layout/PageSession.h"
#include "ui/MaterialPalette.h"
#include "ui/PageHostWindow.h"
#include "ui/Theme.h"

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

void SettingsUi::colorSyncFromHsl()
{
    QColor c = QColor::fromHsl(qBound(0, m_colorH, 359), qBound(0, m_colorS, 255),
                               qBound(0, m_colorL, 255), qBound(0, m_colorA, 255));
    m_colorDraft = c;
    m_colorR = c.red();
    m_colorG = c.green();
    m_colorB = c.blue();
}

void SettingsUi::colorSyncFromRgb()
{
    m_colorDraft = QColor(qBound(0, m_colorR, 255), qBound(0, m_colorG, 255),
                          qBound(0, m_colorB, 255), qBound(0, m_colorA, 255));
    int h = 0, s = 0, l = 0, a = 255;
    m_colorDraft.getHsl(&h, &s, &l, &a);
    if (h >= 0) {
        m_colorH = h;
    }
    m_colorS = s;
    m_colorL = l;
    m_colorA = a;
}

void SettingsUi::loadColorDraft(const QColor& c)
{
    m_colorDraft = c.isValid() ? c : ThemeColors::defaultProgressColor();
    m_colorR = m_colorDraft.red();
    m_colorG = m_colorDraft.green();
    m_colorB = m_colorDraft.blue();
    m_colorA = m_colorDraft.alpha();
    int h = 0, s = 0, l = 0, a = 255;
    m_colorDraft.getHsl(&h, &s, &l, &a);
    if (h >= 0) {
        m_colorH = h;
    }
    m_colorS = s;
    m_colorL = l;
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
    case ColorAxis::Kind::Light:
        return pct255(m_colorL);
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
        colorSyncFromHsl();
        break;
    case ColorAxis::Kind::Sat:
        m_colorS = fromPct255(value);
        colorSyncFromHsl();
        break;
    case ColorAxis::Kind::Light:
        m_colorL = fromPct255(value);
        colorSyncFromHsl();
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
    storeDraftPending();
    return true;
}

void SettingsUi::colorSetChannel(const QString& channel, int value)
{
    applyColorShownValue(channel, value);
}

void SettingsUi::colorNudge(const QString& channel, int dir)
{
    if (!m_color.active && !m_opacity.active) {
        (void)ensureInlineThemeEditor();
    }
    if (m_scrub.active) {
        endSliderScrub(true);
    }
    applyColorShownValue(channel, colorShownValue(channel) + dir);
    refreshColorPicker();
}

bool SettingsUi::isInlineThemeEditor() const
{
    return m_color.active && m_color.pageId.isEmpty();
}

bool SettingsUi::ensureInlineThemeEditor()
{
    if (m_color.active && !m_color.pageId.isEmpty()) {
        return false;
    }
    if (isInlineThemeEditor()) {
        applyPreviewColor();
        return true;
    }
    m_color.active = true;
    m_color.pageId.clear();
    m_colorPending.clear();
    m_colorPending.insert(QStringLiteral("customPrimaryColor"),
                          m_settings.resolvedPalette().colors.accent);
    QColor sec = m_settings.resolvedPalette().progress;
    sec.setAlpha(kProgressFillAlpha);
    m_colorPending.insert(QStringLiteral("customSecondaryColor"), sec);
    loadActiveThemeColor();
    applyPreviewColor();
    return true;
}

void SettingsUi::stopInlineThemeEditor()
{
    abortSliderScrub();
    m_color.reset();
    m_colorPending.clear();
    m_colorPickerKey.clear();
}

void SettingsUi::persistThemeDraft(bool persist)
{
    if (!isInlineThemeEditor() || !m_colorDraft.isValid()) {
        return;
    }
    if (m_colorPickerKey.isEmpty()) {
        m_colorPickerKey = activeThemeColorKey();
    }
    storeDraftPending();
    const QColor stored = m_colorPending.value(m_colorPickerKey, m_colorDraft);
    m_settings.themeCustom = true;
    (void)m_settings.setColorKey(m_colorPickerKey, stored, false);
    m_settings.applyTheme();
    apply(persist);
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
    if (colorKey == QLatin1String("customPrimaryColor")
        || colorKey == QLatin1String("customSecondaryColor")) {
        m_themeAssignPrimary = colorKey != QLatin1String("customSecondaryColor");
        if (!ensureInlineThemeEditor()) {
            if (error) {
                *error = QStringLiteral("Color picker is busy");
            }
            return false;
        }
        loadActiveThemeColor();
        if (!m_pages.hasPage(QStringLiteral("main_settings_theme"))
            && !m_pages.openPage(QStringLiteral("main_settings_theme"), error)) {
            return false;
        }
        applyPreviewColor();
        m_pages.refreshDecorated();
        notifyStatus(m_themeAssignPrimary ? QStringLiteral("Editing Primary")
                                          : QStringLiteral("Editing Secondary"));
        return true;
    }
    m_colorPickerKey = colorKey;
    m_colorPending.clear();
    loadColorDraft(m_settings.colorKey(m_colorPickerKey));
    m_colorPending.insert(m_colorPickerKey, m_colorDraft);
    if (!presentLive(m_color, QLatin1String(kLiveColor), buildGenericColorDocument(), error)) {
        m_color.reset();
        return false;
    }
    applyPreviewColor();
    notifyStatus(QStringLiteral("Pick color for %1").arg(AppSettings::settingTitle(colorKey)));
    return true;
}

QColor SettingsUi::liveThemeSource() const
{
    if (isInlineThemeEditor() && m_colorDraft.isValid()) {
        return m_colorDraft;
    }
    return colorForThemeKey(activeThemeColorKey());
}

QString SettingsUi::activeThemeColorKey() const
{
    return m_themeAssignPrimary ? QStringLiteral("customPrimaryColor")
                                : QStringLiteral("customSecondaryColor");
}

QColor SettingsUi::colorForThemeKey(const QString& key) const
{
    const QColor pending = m_colorPending.value(key);
    if (pending.isValid()) {
        return pending;
    }
    if (key == QLatin1String("customPrimaryColor")) {
        return m_settings.resolvedPalette().colors.accent;
    }
    if (key == QLatin1String("customSecondaryColor")) {
        QColor sec = m_settings.resolvedPalette().progress;
        sec.setAlpha(kProgressFillAlpha);
        return sec;
    }
    return m_settings.colorKey(key);
}

void SettingsUi::storeDraftPending()
{
    if (!m_color.active || !m_colorDraft.isValid() || m_colorPickerKey.isEmpty()) {
        return;
    }
    QColor stored = m_colorDraft;
    if (m_colorPickerKey == QLatin1String("customSecondaryColor")) {
        stored.setAlpha(kProgressFillAlpha);
    }
    m_colorPending.insert(m_colorPickerKey, stored);
}

void SettingsUi::loadActiveThemeColor()
{
    m_colorPickerKey = activeThemeColorKey();
    loadColorDraft(colorForThemeKey(m_colorPickerKey));
    storeDraftPending();
}

void SettingsUi::themeSetAssignPrimary(bool primary)
{
    if (m_scrub.active) {
        endSliderScrub(true);
    }
    const bool switched = m_themeAssignPrimary != primary;
    if (isInlineThemeEditor() && switched) {
        persistThemeDraft(true);
    }
    m_themeAssignPrimary = primary;
    if (!ensureInlineThemeEditor()) {
        apply(false);
        notifyStatus(primary ? QStringLiteral("Editing Primary")
                             : QStringLiteral("Editing Secondary"));
        return;
    }
    if (switched || m_colorPickerKey != activeThemeColorKey()) {
        loadActiveThemeColor();
    }
    applyPreviewColor();
    m_pages.refreshDecorated();
    notifyStatus(primary ? QStringLiteral("Editing Primary")
                         : QStringLiteral("Editing Secondary"));
}

void SettingsUi::themePickShade(int family, int index)
{
    if (family < 0 || family >= MaterialPalette::kFamilyCount) {
        return;
    }
    const auto f = static_cast<MaterialPalette::Family>(family);
    const MaterialPalette::Palettes pal = MaterialPalette::generate(liveThemeSource());
    QColor c = MaterialPalette::shade(pal, f, index);
    if (!m_themeAssignPrimary) {
        c.setAlpha(kProgressFillAlpha);
    }
    if (!ensureInlineThemeEditor()) {
        return;
    }
    m_colorPickerKey = activeThemeColorKey();
    loadColorDraft(c);
    storeDraftPending();
    persistThemeDraft(true);
    notifyStatus(QStringLiteral("%1 = %2").arg(m_themeAssignPrimary ? QStringLiteral("Primary")
                                                                    : QStringLiteral("Secondary"),
                                               c.name(QColor::HexRgb).toUpper()));
}

void SettingsUi::colorApplyDraftShade(int index)
{
    if (!m_color.active) {
        return;
    }
    const MaterialPalette::Palettes pal = MaterialPalette::generate(m_colorDraft);
    loadColorDraft(MaterialPalette::shade(pal, MaterialPalette::Family::Primary, index));
    storeDraftPending();
    refreshColorPicker();
}

PageDocument SettingsUi::buildGenericColorDocument() const
{
    PageDocument doc;
    doc.id = QLatin1String(kLiveColor);
    doc.name = AppSettings::settingTitle(m_colorPickerKey);
    initGrid(doc, 12, 9, 1400, 980, 8, 20, m_settings.resolvedTheme());
    PageGrid& grid = doc.grids[0];
    const EditorSwatch pal = editorSwatch();
    for (int i = 0; i < int(std::size(kColorAxes)); ++i) {
        addColorAxis(grid, kColorAxes[i], i, 9, 11, pal);
    }
    const MaterialPalette::Palettes pals = MaterialPalette::generate(m_colorDraft);
    for (int display = 0; display < MaterialPalette::kShadeCount; ++display) {
        const int idx = MaterialPalette::kShadeCount - 1 - display;
        const QColor c = pals.primary[idx];
        grid.cells.push_back(cell(QStringLiteral("draft_shade_%1").arg(idx),
                                  QString::number(MaterialPalette::kShades[idx]), 7, display,
                                  QStringLiteral("settings.color.draftShade.%1").arg(idx), c));
    }
    const QString hex = m_colorDraft.name(QColor::HexArgb).toUpper();
    grid.cells.push_back(cell(QStringLiteral("hex"), hex, 8, 0,
                              QStringLiteral("settings.color.editHex"), pal.value, 4));
    grid.cells.push_back(cell(QStringLiteral("save"), QStringLiteral("Save"), 8, 4,
                              QStringLiteral("settings.color.save"), pal.save, 4));
    grid.cells.push_back(cell(QStringLiteral("cancel"), QStringLiteral("Cancel"), 8, 8,
                              QStringLiteral("settings.color.cancel"), pal.cancel, 4));
    return doc;
}

void SettingsUi::refreshColorPicker()
{
    if (m_hexActive || m_numpad.active) {
        return;
    }
    if (isInlineThemeEditor()) {
        if (m_scrub.active) {
            applyPreviewColor();
            m_pages.refreshDecorated();
            return;
        }
        persistThemeDraft(true);
        return;
    }
    QString err;
    if (!presentLive(m_color, QLatin1String(kLiveColor), buildGenericColorDocument(), &err)) {
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
    abortSliderScrub();
    if (m_flashCustomSetMode) {
        m_settings.flashUseForeground = true;
        apply(true);
    }
    m_colorPending.clear();
    m_flashCustomSetMode = false;
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
    storeDraftPending();
    for (auto it = m_colorPending.constBegin(); it != m_colorPending.constEnd(); ++it) {
        if (!m_settings.setColorKey(it.key(), it.value(), /*rebuildPalette=*/false)) {
            if (error) {
                *error = QStringLiteral("Could not apply color");
            }
            return false;
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
