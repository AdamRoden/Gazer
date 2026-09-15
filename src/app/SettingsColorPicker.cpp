#include "app/SettingsUi.h"
#include "app/SettingsPageBuild.h"
#include "app/SettingsUiInternal.h"

#include "assist/MouseDwellMove.h"
#include "layout/PageHit.h"
#include "layout/PageSession.h"
#include "ui/ColorField.h"
#include "ui/PickerPalette.h"
#include "ui/MaterialPalette.h"
#include "ui/PageHostWindow.h"
#include "ui/SliderTrack.h"
#include "ui/Theme.h"
#include "utils/ScreenGrab.h"

#include <QColor>
#include <QGuiApplication>
#include <QPoint>
#include <QScreen>
#include <QtGlobal>

namespace gazer {

using SettingsPageBuild::adoptCallerBoard;
using SettingsPageBuild::cell;
using SettingsPageBuild::initGrid;
using SettingsPageBuild::makeNested;
using SettingsUiInternal::ColorAxis;
using SettingsUiInternal::findColorAxis;
using SettingsUiInternal::fromPct255;
using SettingsUiInternal::hexSeedFromColor;
using SettingsUiInternal::kLiveColor;
using SettingsUiInternal::kLiveHex;
using SettingsUiInternal::pct255;

void SettingsUi::applyPreviewColor()
{
    if (PageHostWindow* w = m_pages.window()) {
        w->setPreviewColor(m_colorDraft);
    }
}

void SettingsUi::loadColorDraft(const QColor& c)
{
    m_colorDraft = c.isValid() ? c : ThemeColors::defaultProgressColor();
    m_colorA = m_colorDraft.alpha();
}

void SettingsUi::applyHsv(int h, int s, int v, int alpha)
{
    QColor c = QColor::fromHsv(qBound(0, h, 359), qBound(0, s, 255), qBound(0, v, 255),
                               qBound(0, alpha, 255));
    loadColorDraft(c);
    storeDraftPending();
}

int SettingsUi::colorShownValue(const QString& channel) const
{
    const ColorAxis* axis = findColorAxis(channel);
    if (!axis) {
        return 0;
    }
    if (axis->kind == ColorAxis::Kind::Hue) {
        int h = 0, s = 0, v = 0, a = 255;
        m_colorDraft.getHsv(&h, &s, &v, &a);
        return h < 0 ? 0 : h;
    }
    return pct255(m_colorDraft.alpha());
}

bool SettingsUi::applyColorShownValue(const QString& channel, int value)
{
    const ColorAxis* axis = findColorAxis(channel);
    if (!axis) {
        return false;
    }
    if (axis->kind == ColorAxis::Kind::Hue) {
        int h = 0, s = 0, v = 0, a = 255;
        m_colorDraft.getHsv(&h, &s, &v, &a);
        applyHsv(value, s, v, m_colorA);
        return true;
    }
    m_colorA = fromPct255(value);
    m_colorDraft.setAlpha(m_colorA);
    storeDraftPending();
    return true;
}

void SettingsUi::colorSetChannel(const QString& channel, int value)
{
    applyColorShownValue(channel, value);
}

void SettingsUi::colorNudge(const QString& channel, int dir)
{
    if (!m_color.active) {
        return;
    }
    applyColorShownValue(channel, colorShownValue(channel) + dir);
    refreshColorPicker();
}

void SettingsUi::colorNudgeField(int ds, int dv)
{
    if (!m_color.active) {
        return;
    }
    int h = 0, s = 0, v = 0, a = 255;
    m_colorDraft.getHsv(&h, &s, &v, &a);
    if (h < 0) {
        h = 0;
    }
    applyHsv(h, fromPct255(pct255(s) + ds), fromPct255(pct255(v) + dv), m_colorA);
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
    (void)m_settings.setColorKey(m_colorPickerKey, stored, false);
    m_settings.applyTheme();
    apply(persist);
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
    QColor src = m_colorPending.value(QStringLiteral("customPrimaryColor"));
    if (!src.isValid()) {
        src = m_settings.resolvedPalette().colors.accent;
    }
    const MaterialPalette::Palettes pal = MaterialPalette::generate(src);
    QColor c = MaterialPalette::shade(pal, f, index);
    c.setAlpha(kProgressFillAlpha);
    if (m_mutate) {
        m_mutate([c](AppSettings& s) {
                     (void)s.setColorKey(QStringLiteral("customSecondaryColor"), c, true);
                 },
                 QStringLiteral("Progress = %1").arg(c.name(QColor::HexRgb).toUpper()));
        return;
    }
    (void)m_settings.setColorKey(QStringLiteral("customSecondaryColor"), c, true);
    apply(true);
    notifyStatus(QStringLiteral("Progress = %1").arg(c.name(QColor::HexRgb).toUpper()));
}

void SettingsUi::colorApplyPalette(int index)
{
    if (!m_color.active) {
        return;
    }
    QColor c = pickerPaletteColor(index, m_colorA);
    if (!c.isValid()) {
        return;
    }
    loadColorDraft(c);
    storeDraftPending();
    refreshColorPicker();
}

PageDocument SettingsUi::buildGenericColorDocument() const
{
    PageDocument doc;
    doc.id = QLatin1String(kLiveColor);
    doc.name = AppSettings::settingTitle(m_colorPickerKey);
    initGrid(doc, 2, 1, 1080, 1080, 6, 12, m_settings.resolvedTheme());
    PageGrid& grid = doc.grids[0];
    adoptCallerBoard(grid, m_pages.pageBehind(QLatin1String(kLiveColor)));
    grid.columnTracks = starTracks({1.0, 2.0});
    const EditorSwatch pal = editorSwatch();
    constexpr int kGap = 6;

    PageGrid left = makeNested(QStringLiteral("picker"), 0, 0, 5, 10, kGap);
    left.rowTracks = starTracks({6.5, 1.0, 0.9, 0.9, 0.8});

    PageGrid hsv = makeNested(QStringLiteral("hsv"), 0, 0, 3, 3, kGap);
    hsv.colSpan = 10;
    hsv.rowTracks = starTracks({0.9, 6.0, 0.9});
    hsv.columnTracks = starTracks({0.9, 8.0, 0.9});
    hsv.cells.push_back(cell(QStringLiteral("field_up"), QStringLiteral("↑"), 0, 1,
                             QStringLiteral("settings.color.field.up"), pal.nudge));
    hsv.cells.push_back(cell(QStringLiteral("field_left"), QStringLiteral("←"), 1, 0,
                             QStringLiteral("settings.color.field.left"), pal.nudge));
    hsv.cells.push_back(cell(QStringLiteral("colorfield"), {}, 1, 1, {}, QColor(), 1,
                             QStringLiteral("colorfield")));
    hsv.cells.push_back(cell(QStringLiteral("field_right"), QStringLiteral("→"), 1, 2,
                             QStringLiteral("settings.color.field.right"), pal.nudge));
    hsv.cells.push_back(cell(QStringLiteral("field_down"), QStringLiteral("↓"), 2, 1,
                             QStringLiteral("settings.color.field.down"), pal.nudge));
    left.subGrids.push_back(std::move(hsv));

    PageCell eyedrop = cell(QStringLiteral("eyedrop"), {}, 1, 0,
                            QStringLiteral("settings.color.eyedropper"), pal.edit, 5, {}, {},
                            QStringLiteral("colorize"));
    eyedrop.activeState = QStringLiteral("settings.color.eyedropper");
    left.cells.push_back(std::move(eyedrop));
    PageCell clickAt = cell(QStringLiteral("click_gaze"), {}, 1, 5,
                            QStringLiteral("settings.color.pickAtGaze"), pal.key, 5, {}, {},
                            QStringLiteral("adsClick"));
    clickAt.activeState = QStringLiteral("settings.color.pickAtGaze");
    left.cells.push_back(std::move(clickAt));

    left.cells.push_back(cell(QStringLiteral("dec_h"), QStringLiteral("←"), 2, 0,
                              QStringLiteral("settings.color.nudge.h.dec"), pal.nudge));
    left.cells.push_back(cell(QStringLiteral("track_h"), QStringLiteral("Hue"), 2, 1, {}, QColor(),
                              8, QStringLiteral("slider"), QStringLiteral("h")));
    left.cells.push_back(cell(QStringLiteral("inc_h"), QStringLiteral("→"), 2, 9,
                              QStringLiteral("settings.color.nudge.h.inc"), pal.nudge));

    left.cells.push_back(cell(QStringLiteral("dec_a"), QStringLiteral("←"), 3, 0,
                              QStringLiteral("settings.color.nudge.a.dec"), pal.nudge));
    left.cells.push_back(cell(QStringLiteral("track_a"), QStringLiteral("Opacity"), 3, 1, {},
                              QColor(), 8, QStringLiteral("slider"), QStringLiteral("a")));
    left.cells.push_back(cell(QStringLiteral("inc_a"), QStringLiteral("→"), 3, 9,
                              QStringLiteral("settings.color.nudge.a.inc"), pal.nudge));

    const QString hexShown = QStringLiteral("#%1").arg(hexSeedFromColor(m_colorDraft));
    left.cells.push_back(cell(QStringLiteral("hex"), hexShown, 4, 0,
                              QStringLiteral("settings.color.editHex"), m_colorDraft, 6));
    left.cells.push_back(cell(QStringLiteral("opacity_label"),
                              QStringLiteral("%1%").arg(pct255(m_colorA)), 4, 6,
                              QStringLiteral("settings.color.edit.a"), pal.value, 4));
    grid.subGrids.push_back(std::move(left));

    PageGrid right = makeNested(QStringLiteral("swatches"), 0, 1, 2, 1, kGap);
    right.rowTracks = starTracks({11.0, 1.1});

    PageGrid palette = makeNested(QStringLiteral("palette"), 0, 0, kPickerShadeCount,
                                  kPickerFamilyCount, 4);
    for (int family = 0; family < kPickerFamilyCount; ++family) {
        for (int shade = 0; shade < kPickerShadeCount; ++shade) {
            const int i = family * kPickerShadeCount + shade;
            const QPoint pos = pickerPaletteRowCol(i);
            palette.cells.push_back(cell(QStringLiteral("palette_%1").arg(i), {}, pos.y(), pos.x(),
                                         QStringLiteral("settings.color.palette.%1").arg(i),
                                         pickerPaletteColor(i, m_colorA), 1,
                                         QStringLiteral("swatchrect")));
        }
    }
    right.subGrids.push_back(std::move(palette));

    PageGrid actions = makeNested(QStringLiteral("actions"), 1, 0, 1, 2, kGap);
    actions.cells.push_back(cell(QStringLiteral("save"), QStringLiteral("Save"), 0, 0,
                                 QStringLiteral("settings.color.save"), pal.save));
    actions.cells.push_back(cell(QStringLiteral("cancel"), QStringLiteral("Cancel"), 0, 1,
                                 QStringLiteral("settings.color.cancel"), pal.cancel));
    right.subGrids.push_back(std::move(actions));
    grid.subGrids.push_back(std::move(right));
    return doc;
}

void SettingsUi::refreshColorPicker()
{
    if (m_hexActive || m_numpad.active) {
        return;
    }
    if (isInlineThemeEditor()) {
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
    if (m_eyedropActive && m_mouseDwell) {
        m_mouseDwell->setArmed(false);
    }
    cancelEyedropper();
    m_colorPending.clear();
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
        const bool rebuild = !AppSettings::themeRoleForColorKey(it.key()).isEmpty();
        if (!m_settings.setColorKey(it.key(), it.value(), rebuild)) {
            if (error) {
                *error = QStringLiteral("Could not apply color");
            }
            return false;
        }
    }
    apply(true);
    const QString title = AppSettings::settingTitle(m_colorPickerKey);
    const QString hex = m_colorDraft.name(QColor::HexArgb).toUpper();
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

void SettingsUi::restoreEyedropHost()
{
    if (!m_eyedropHostHidden) {
        return;
    }
    m_pages.showHost();
    m_eyedropHostHidden = false;
}

void SettingsUi::cancelEyedropper()
{
    const bool was = m_eyedropActive;
    m_eyedropActive = false;
    restoreEyedropHost();
    if (was && m_color.active && !m_hexActive && !m_numpad.active) {
        refreshColorPicker();
    }
}

void SettingsUi::sampleScreenColor(const QPoint& pos)
{
    QScreen* screen = QGuiApplication::screenAt(pos);
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) {
        return;
    }
    const QPixmap grab = grabScreenRect(screen, QRect(pos, QSize(1, 1)));
    if (grab.isNull()) {
        return;
    }
    const QColor sampled = grab.toImage().pixelColor(0, 0);
    if (!sampled.isValid()) {
        return;
    }
    QColor c = sampled;
    if (c.alpha() <= 0) {
        c.setAlpha(m_colorA);
    }
    loadColorDraft(c);
    storeDraftPending();
}

bool SettingsUi::beginColorPick()
{
    if (!m_color.active || !m_mouseDwell) {
        return false;
    }
    m_mouseDwell->toggleArmed(MouseDwellMove::ArmPurpose::ColorPick);
    if (m_mouseDwell->isColorPick()) {
        m_mouseDwell->ensureSelectDeadline(8000);
        notifyStatus(QStringLiteral("Look at the square, slider, or swatch"));
    }
    return true;
}

bool SettingsUi::beginEyedropper()
{
    if (!m_color.active && !ensureInlineThemeEditor()) {
        return false;
    }
    if (!m_color.active) {
        return false;
    }
    const QRect gate = m_pages.targetScreenRect(m_color.pageId, QStringLiteral("eyedrop"));
    m_eyedropActive = true;
    if (PageHostWindow* w = m_pages.window(); w && w->isVisible()) {
        m_pages.hideHost();
        m_eyedropHostHidden = true;
    }
    if (m_mouseDwell) {
        m_mouseDwell->setArmed(true, MouseDwellMove::ArmPurpose::ColorSample);
        m_mouseDwell->ensureSelectDeadline(8000);
        if (!gate.isEmpty()) {
            m_mouseDwell->gateUntilGazeLeaves(gate);
            m_pages.setAimActivator(m_color.pageId, QStringLiteral("eyedrop"));
        }
    }
    notifyStatus(QStringLiteral("Eyedropper — dwell to sample a screen color"));
    return true;
}

void SettingsUi::onColorAimMoved(const QPoint& pos)
{
    if (!m_mouseDwell) {
        return;
    }
    if (m_mouseDwell->isColorSample() || m_eyedropActive) {
        sampleScreenColor(pos);
        m_eyedropActive = false;
        restoreEyedropHost();
        if (m_color.active) {
            refreshColorPicker();
        }
        notifyStatus(QStringLiteral("Sampled %1").arg(m_colorDraft.name(QColor::HexArgb).toUpper()));
        return;
    }
    if (m_mouseDwell->isColorPick() && m_color.active && !m_hexActive && !m_numpad.active) {
        applyPickAt(pos);
    }
}

void SettingsUi::applyPickAt(const QPoint& pos)
{
    const QPointF gaze(pos);
    auto local = [](const PageTarget& t) { return localIdOf(t); };
    auto cellOf = [](const PageTarget& t) { return t.geom.contentOnScreen(); };

    const PageTarget* field = nullptr;
    const PageTarget* hue = nullptr;
    const PageTarget* alpha = nullptr;
    const PageTarget* swatch = nullptr;
    for (const PageTarget& t : m_pages.targets()) {
        const QString id = local(t);
        if (id == QLatin1String("colorfield")) {
            field = &t;
        } else if (id == QLatin1String("track_h")) {
            hue = &t;
        } else if (id == QLatin1String("track_a")) {
            alpha = &t;
        } else if (id.startsWith(QLatin1String("palette_")) && cellOf(t).contains(gaze)) {
            swatch = &t;
        }
    }

    if (field && cellOf(*field).contains(gaze)) {
        const ColorField::Visual geom = ColorField::visual(cellOf(*field));
        double s01 = 0.0, v01 = 0.0;
        geom.svAt(gaze, &s01, &v01);
        int h = 0, s = 0, v = 0, a = 255;
        m_colorDraft.getHsv(&h, &s, &v, &a);
        if (h < 0) {
            h = 0;
        }
        applyHsv(h, qRound(s01 * 255.0), qRound(v01 * 255.0), m_colorA);
        refreshColorPicker();
        return;
    }
    if (hue && cellOf(*hue).contains(gaze)) {
        const double t = SliderTrack::visual(cellOf(*hue), false).tAtX(gaze.x());
        applyColorShownValue(QStringLiteral("h"), qRound(t * 359.0));
        refreshColorPicker();
        return;
    }
    if (alpha && cellOf(*alpha).contains(gaze)) {
        const double t = SliderTrack::visual(cellOf(*alpha), false).tAtX(gaze.x());
        applyColorShownValue(QStringLiteral("a"), qRound(t * 100.0));
        refreshColorPicker();
        return;
    }
    if (!swatch) {
        return;
    }
    const QString id = local(*swatch);
    bool ok = false;
    if (id.startsWith(QLatin1String("palette_"))) {
        const int idx = id.mid(int(QStringLiteral("palette_").size())).toInt(&ok);
        if (ok) {
            colorApplyPalette(idx);
        }
    }
}

} // namespace gazer
