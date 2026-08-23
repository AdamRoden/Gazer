#include "app/SettingsUi.h"
#include "app/SettingsPageBuild.h"


#include "layout/PageDim.h"
#include "layout/PageHit.h"
#include "layout/PageSession.h"

#include "ui/PageHostWindow.h"
#include "ui/SliderTrack.h"
#include "ui/Theme.h"
#include "ui/ThemeScheme.h"

#include <QColor>
#include <QtGlobal>
#include <QVector>

#include <iterator>

namespace gazer {

namespace {

constexpr int kMaxArraySteps = 12;
constexpr int kStepNudge = 50;
constexpr auto kLiveNumpad = "settings_numpad_live";
constexpr auto kLiveArray = "settings_array_live";
constexpr auto kLiveColor = "settings_color_live";
constexpr auto kLiveOpacity = "settings_opacity_live";
constexpr auto kLiveHex = "settings_hex_live";

using SettingsPageBuild::cell;
using SettingsPageBuild::initGrid;

QString localIdOf(const PageTarget& t)
{
    if (!t.pageId.isEmpty() && t.id.startsWith(t.pageId + QLatin1Char('/'))) {
        return t.id.mid(t.pageId.size() + 1);
    }
    return t.id;
}

struct ColorAxis {
    const char* id;
    const char* label;
    const char* title;
    const char* hint;
    enum class Kind { Hue, Sat, Val, Red, Green, Blue, Alpha } kind;
};

constexpr ColorAxis kColorAxes[] = {
    {"h", "H", "Hue", "Hue in degrees (0–359).", ColorAxis::Kind::Hue},
    {"s", "S", "Saturation", "Saturation percent (0–100).", ColorAxis::Kind::Sat},
    {"v", "V", "Value", "Brightness percent (0–100).", ColorAxis::Kind::Val},
    {"r", "R", "R", "R channel (0–255).", ColorAxis::Kind::Red},
    {"g", "G", "G", "G channel (0–255).", ColorAxis::Kind::Green},
    {"b", "B", "B", "B channel (0–255).", ColorAxis::Kind::Blue},
    {"a", "A", "Alpha", "Opacity percent (0–100).", ColorAxis::Kind::Alpha},
};

const ColorAxis* findColorAxis(const QString& channel)
{
    for (const ColorAxis& a : kColorAxes) {
        if (channel.compare(QLatin1String(a.id), Qt::CaseInsensitive) == 0) {
            return &a;
        }
    }
    return nullptr;
}

int pct255(int raw)
{
    return qBound(0, qRound(raw / 2.55), 100);
}

int fromPct255(int shown)
{
    return qBound(0, int(qRound(shown * 2.55)), 255);
}

} // namespace

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

QColor SettingsUi::flashOpacityPreview() const
{
    QColor c = m_settings.resolvedTheme().text;
    c.setAlpha(qBound(0, qRound(255.0 * double(m_opacityDraft) / 100.0), 255));
    return c;
}

void SettingsUi::applyPreviewColor()
{
    if (PageHostWindow* w = m_pages.window()) {
        w->setPreviewColor(m_colorDraft);
    }
}

bool SettingsUi::openArrayEditor(const QString& settingKey, QString* error)
{
    m_arrayKey = settingKey;
    m_arrayDraft = m_settings.dwellSequence;
    if (m_arrayDraft.isEmpty()) {
        m_arrayDraft = AppSettings::defaultDwellSequence();
    }
    refreshArrayEditor();
    notifyStatus(QStringLiteral("Edit dwell sequence"));
    return true;
}

PageDocument SettingsUi::buildArrayDocument() const
{
    PageDocument doc;
    doc.id = QStringLiteral("settings_array_live");
    doc.name = QStringLiteral("Dwell sequence");
    const int n = qBound(1, m_arrayDraft.size(), kMaxArraySteps);
    initGrid(doc, 5, n + 2, 920, 200 + (n + 2) * 68, 10, 20, m_settings.resolvedTheme());
    PageGrid& grid = doc.grids[0];

    const EditorSwatch sw = editorSwatch();
    const QColor decBg = sw.nudge;
    const QColor incBg = sw.nudge;
    const QColor editBg = sw.edit;
    const QColor delBg = sw.cancel;
    const QColor valBg = sw.value;

    for (int i = 0; i < n; ++i) {
        const int ms = m_arrayDraft[i];
        grid.cells.push_back(cell(QStringLiteral("dec_%1").arg(i), QStringLiteral("−"), i, 0,
                                  QStringLiteral("settings.array.nudge.%1.dec").arg(i), decBg));
        PageCell val = cell(QStringLiteral("val_%1").arg(i), QStringLiteral("%1 ms").arg(ms), i, 1,
                            {}, valBg, 1, false, QStringLiteral("value"));
        val.clusterSlot = QStringLiteral("value");
        grid.cells.push_back(val);
        grid.cells.push_back(cell(QStringLiteral("inc_%1").arg(i), QStringLiteral("+"), i, 2,
                                  QStringLiteral("settings.array.nudge.%1.inc").arg(i), incBg));
        grid.cells.push_back(cell(QStringLiteral("edit_%1").arg(i), QStringLiteral("Edit"), i, 3,
                                  QStringLiteral("settings.array.edit.%1").arg(i), editBg));
        grid.cells.push_back(cell(QStringLiteral("del_%1").arg(i), QStringLiteral("Trash"), i, 4,
                                  QStringLiteral("settings.array.del.%1").arg(i), delBg));
    }

    const int bar = n;
    grid.cells.push_back(cell(QStringLiteral("decAll"), QStringLiteral("− all"), bar, 0,
                              QStringLiteral("settings.array.decAll"), sw.nudge));
    grid.cells.push_back(cell(QStringLiteral("reset"), QStringLiteral("Reset"), bar, 1,
                              QStringLiteral("settings.array.reset"), sw.warn));
    grid.cells.push_back(cell(QStringLiteral("incAll"), QStringLiteral("+ all"), bar, 2,
                              QStringLiteral("settings.array.incAll"), sw.nudge));
    grid.cells.push_back(cell(QStringLiteral("add"), QStringLiteral("Add"), bar, 3,
                              QStringLiteral("settings.array.add"), sw.add, 2));

    const int foot = n + 1;
    grid.cells.push_back(cell(QStringLiteral("save"), QStringLiteral("Save"), foot, 0,
                              QStringLiteral("settings.array.save"), sw.save, 3));
    grid.cells.push_back(cell(QStringLiteral("cancel"), QStringLiteral("Cancel"), foot, 3,
                              QStringLiteral("settings.array.cancel"), sw.cancel, 2));
    return doc;
}

void SettingsUi::refreshArrayEditor()
{
    QString err;
    if (!presentLive(m_array, QLatin1String(kLiveArray), buildArrayDocument(), &err)) {
        notifyStatus(err);
    }
}

void SettingsUi::arrayNudge(int index, int dir)
{
    if (!m_array.active || index < 0 || index >= m_arrayDraft.size()) {
        return;
    }
    m_arrayDraft[index] = qBound(50, m_arrayDraft[index] + dir * kStepNudge, 10000);
    refreshArrayEditor();
}

void SettingsUi::arrayNudgeAll(int dir)
{
    if (!m_array.active) {
        return;
    }
    for (int& ms : m_arrayDraft) {
        ms = qBound(50, ms + dir * kStepNudge, 10000);
    }
    refreshArrayEditor();
}

void SettingsUi::arrayRemove(int index)
{
    if (!m_array.active || index < 0 || index >= m_arrayDraft.size()) {
        return;
    }
    if (m_arrayDraft.size() <= 1) {
        notifyStatus(QStringLiteral("Need at least one step"));
        return;
    }
    m_arrayDraft.removeAt(index);
    refreshArrayEditor();
}

void SettingsUi::arrayAdd()
{
    if (!m_array.active) {
        return;
    }
    if (m_arrayDraft.size() >= kMaxArraySteps) {
        notifyStatus(QStringLiteral("Maximum %1 steps").arg(kMaxArraySteps));
        return;
    }
    m_arrayDraft.push_back(m_arrayDraft.isEmpty() ? 700 : m_arrayDraft.last());
    refreshArrayEditor();
}

void SettingsUi::arrayReset()
{
    if (!m_array.active) {
        return;
    }
    m_arrayDraft = m_settings.dwellSequence;
    if (m_arrayDraft.isEmpty()) {
        m_arrayDraft = AppSettings::defaultDwellSequence();
    }
    refreshArrayEditor();
}

bool SettingsUi::arraySave(QString* error)
{
    if (!m_array.active) {
        if (error) {
            *error = QStringLiteral("Sequence editor is not open");
        }
        return false;
    }
    QStringList parts;
    for (int ms : m_arrayDraft) {
        parts << QString::number(ms);
    }
    QString err;
    if (!m_settings.applyNumericBuffer(QStringLiteral("dwellSequence"), parts.join(QLatin1Char(',')),
                                       &err)) {
        notifyStatus(err);
        if (error) {
            *error = err;
        }
        return false;
    }
    apply(true);
    m_arrayKey.clear();
    m_arrayDraft.clear();
    closeLive(m_array);
    notifyStatus(QStringLiteral("Saved dwell sequence = %1").arg(m_settings.dwellSequenceString()));
    return true;
}

bool SettingsUi::arrayCancel(QString* error)
{
    if (!m_array.active) {
        if (error) {
            *error = QStringLiteral("Sequence editor is not open");
        }
        return false;
    }
    m_arrayKey.clear();
    m_arrayDraft.clear();
    closeLive(m_array);
    notifyStatus(QStringLiteral("Sequence edit cancelled"));
    return true;
}

bool SettingsUi::arrayEditIndex(int index, QString* error)
{
    if (!m_array.active || index < 0 || index >= m_arrayDraft.size()) {
        if (error) {
            *error = QStringLiteral("Invalid sequence step");
        }
        return false;
    }
    m_numpadKey.clear();
    m_numpadTitle = QStringLiteral("Step %1").arg(index + 1);
    m_numpadHint = QStringLiteral("Dwell time for this step (ms).");
    m_numpadResetSeed = QString::number(m_arrayDraft[index]);
    m_numpadBuffer = m_numpadResetSeed;
    m_numpadReturn = NumpadReturn::Array;
    m_numpadArrayIndex = index;
    m_numpadColorChannel.clear();
    if (!presentNumpad(error)) {
        return false;
    }
    notifyStatus(QStringLiteral("Edit step %1").arg(index + 1));
    return true;
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

namespace {

void colorChannelRange(const QString& channel, int* minV, int* maxV)
{
    const QString ch = channel.toLower();
    if (ch == QLatin1String("h") || ch == QLatin1String("hue")) {
        *minV = 0;
        *maxV = 359;
        return;
    }
    if (ch == QLatin1String("s") || ch == QLatin1String("sat") || ch == QLatin1String("v")
        || ch == QLatin1String("val") || ch == QLatin1String("a") || ch == QLatin1String("alpha")
        || ch == QLatin1String("opacity")) {
        *minV = 0;
        *maxV = 100;
        return;
    }
    *minV = 0;
    *maxV = 255;
}

QString colorChannelValueText(const QString& channel, int shown)
{
    const QString ch = channel.toLower();
    if (ch == QLatin1String("s") || ch == QLatin1String("sat") || ch == QLatin1String("v")
        || ch == QLatin1String("val") || ch == QLatin1String("a") || ch == QLatin1String("alpha")
        || ch == QLatin1String("opacity")) {
        return QStringLiteral("%1%").arg(shown);
    }
    return QString::number(shown);
}

} // namespace

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
        tmp.applyCustomPalette(false);
    }
    return tmp;
}

ThemePalette SettingsUi::draftThemePalette() const
{
    const AppSettings tmp = draftThemeSettings();
    ThemePalette p;
    p.colors = tmp.customColors;
    p.progress = tmp.colorKey(QStringLiteral("progressColor"));
    p.progressFill = tmp.colorKey(QStringLiteral("progressFillColor"));
    p.progressBorder = tmp.colorKey(QStringLiteral("progressBorderColor"));
    return p;
}

QColor SettingsUi::suggestedDraftColor(const QString& colorKey) const
{
    return draftThemeSettings().suggestedThemeColor(colorKey);
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

bool SettingsUi::beginSliderScrub(const QString& channel)
{
    if (m_hexActive || m_numpad.active) {
        return false;
    }
    const bool opacity = m_opacity.active
                         && channel.compare(QLatin1String("opacity"), Qt::CaseInsensitive) == 0;
    if (!opacity && (!m_color.active || !findColorAxis(channel))) {
        return false;
    }
    if (m_scrub.active && m_scrub.channel == channel) {
        return true;
    }
    if (m_scrub.active) {
        endSliderScrub(true);
    }
    m_scrub.active = true;
    m_scrub.channel = channel;
    m_scrub.itemId = QStringLiteral("track_%1").arg(channel);
    m_scrubOpacityRevert = m_opacityDraft;
    m_scrubRevert = m_colorDraft;
    m_scrubDwell.reset();
    m_scrubDwell.setDwellMs(m_settings.mouseMoveDwellMs);
    m_scrubGrace.reset();
    m_scrubGrace.graceMs = qMax(0, m_settings.dwellGraceMs);
    if (!m_scrubClock.isValid()) {
        m_scrubClock.start();
    }
    m_scrubLastSampleMs = -1;
    m_scrubDeadlineMs = m_settings.mouseMoveSelectTimeoutMs > 0
                            ? m_scrubClock.elapsed() + m_settings.mouseMoveSelectTimeoutMs
                            : -1;
    syncSliderScrubVisuals();
    notifyStatus(QStringLiteral("Look along the slider — dwell to set, timeout to cancel"));
    return true;
}

void SettingsUi::endSliderScrub(bool commit)
{
    if (!m_scrub.active) {
        return;
    }
    if (!commit) {
        if (m_opacity.active) {
            m_opacityDraft = m_scrubOpacityRevert;
        } else {
            loadColorDraft(m_scrubRevert);
        }
    }
    m_scrub.reset();
    m_scrubDeadlineMs = -1;
    m_scrubLastSampleMs = -1;
    m_scrubDwell.reset();
    if (PageHostWindow* w = m_pages.window()) {
        w->clearSliderScrub();
    }
    if (m_opacity.active) {
        refreshOpacityEditor();
    } else {
        refreshColorPicker();
    }
}

int SettingsUi::scrubShownValue() const
{
    if (m_opacity.active) {
        return m_opacityDraft;
    }
    return colorShownValue(m_scrub.channel);
}

void SettingsUi::syncSliderScrubVisuals()
{
    PageHostWindow* w = m_pages.window();
    if (!w) {
        return;
    }
    const int shown = scrubShownValue();
    int minV = 0;
    int maxV = 255;
    colorChannelRange(m_scrub.channel, &minV, &maxV);
    const double t = (maxV > minV) ? (shown - minV) / double(maxV - minV) : 0.0;
    if (m_opacity.active) {
        w->setPreviewColor(flashOpacityPreview());
    } else if (m_color.active) {
        w->setPreviewColor(m_colorDraft);
    }
    w->setSliderScrub(m_scrub.itemId, t, colorChannelValueText(m_scrub.channel, shown),
                      m_scrubDwell.progress());
}

void SettingsUi::onGaze(const GazePoint& point)
{
    if (m_scrub.active) {
        feedSliderGaze(point);
    }
}

void SettingsUi::feedSliderGaze(const GazePoint& point)
{
    if (!m_scrub.active) {
        return;
    }
    PageHostWindow* w = m_pages.window();
    if (!w) {
        endSliderScrub(false);
        return;
    }

    const qint64 now = m_scrubClock.isValid() ? m_scrubClock.elapsed() : 0;
    if (m_scrubDeadlineMs >= 0 && now >= m_scrubDeadlineMs) {
        notifyStatus(QStringLiteral("Slider timed out — restored previous value"));
        endSliderScrub(false);
        return;
    }

    if (!point.valid) {
        if (m_scrubGrace.onInvalid() == InvalidGazeGrace::Result::Holding) {
            return;
        }
        return;
    }
    m_scrubGrace.onValid();

    const QPointF gaze = point.toPointF();
    const QTransform* xf = m_pages.window() ? &m_pages.window()->drawerXf() : nullptr;
    const PageTarget* hitT =
        PageHit::at(m_pages.targets(), gaze, m_pages.drawerScale(), {}, m_pages.gridPaints(), xf);
    const QString hit = hitT ? localIdOf(*hitT) : QString();
    const QString editId = QStringLiteral("edit_%1").arg(m_scrub.channel);
    const QString decId = QStringLiteral("dec_%1").arg(m_scrub.channel);
    const QString incId = QStringLiteral("inc_%1").arg(m_scrub.channel);
    const bool companion = hit == m_scrub.itemId || hit == editId || hit == decId || hit == incId;
    if (hitT && hitT->interactive && !companion) {
        endSliderScrub(true);
        return;
    }

    const PageTarget* track = nullptr;
    for (const PageTarget& t : m_pages.targets()) {
        if (localIdOf(t) == m_scrub.itemId) {
            track = &t;
            break;
        }
    }
    if (!track) {
        endSliderScrub(true);
        return;
    }
    const QRectF cell = track->geom.contentOnScreen();
    if (cell.isEmpty()) {
        endSliderScrub(true);
        return;
    }
    const SliderTrack::Visual geom = SliderTrack::visual(cell, /*scrubbing=*/true);
    const QRectF bound = cell.adjusted(0, -12, 0, 12);
    if (!bound.contains(gaze)) {
        return;
    }

    const double t = geom.tAtX(gaze.x());
    int minV = 0;
    int maxV = 255;
    colorChannelRange(m_scrub.channel, &minV, &maxV);
    const int shown = minV + qRound(t * double(maxV - minV));
    if (m_opacity.active) {
        m_opacityDraft = qBound(0, shown, 100);
    } else {
        applyColorShownValue(m_scrub.channel, shown);
    }

    const double dtSec =
        m_scrubLastSampleMs < 0 ? 0.016
                                : qBound(0.004, (now - m_scrubLastSampleMs) / 1000.0, 0.08);
    m_scrubLastSampleMs = now;
    const bool done = m_scrubDwell.sample(geom.posAt(t), dtSec);
    syncSliderScrubVisuals();
    if (done) {
        notifyStatus(QStringLiteral("%1 = %2")
                         .arg(QString(m_scrub.channel).toUpper(),
                              colorChannelValueText(m_scrub.channel, shown)));
        endSliderScrub(true);
    }
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

bool SettingsUi::openFlashForeground(QString* error)
{
    if (!openOpacityEditor(error)) {
        return false;
    }
    if (!m_settings.flashUseForeground) {
        m_settings.flashUseForeground = true;
        m_opacitySetMode = true;
        apply(true);
    }
    return true;
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

bool SettingsUi::openOpacityEditor(QString* error)
{
    m_opacityDraft = qBound(0, m_settings.flashForegroundOpacity, 100);
    m_opacityRevert = m_opacityDraft;
    m_opacitySetMode = false;
    if (!presentLive(m_opacity, QLatin1String(kLiveOpacity), buildOpacityDocument(), error)) {
        m_opacity.reset();
        return false;
    }
    if (PageHostWindow* w = m_pages.window()) {
        w->setPreviewColor(flashOpacityPreview());
    }
    notifyStatus(QStringLiteral("Edit flash opacity"));
    return true;
}

PageDocument SettingsUi::buildOpacityDocument() const
{
    PageDocument doc;
    doc.id = QStringLiteral("settings_opacity_live");
    doc.name = QStringLiteral("Flash opacity");
    initGrid(doc, 12, 3, 1100, 500, 8, 72, m_settings.resolvedTheme());
    PageGrid& grid = doc.grids[0];
    const EditorSwatch pal = editorSwatch();
    grid.cells.push_back(cell(QStringLiteral("edit_opacity"), QStringLiteral("Edit"), 0, 0,
                              QStringLiteral("settings.opacity.scrub"), pal.edit, 1, true, {}, {},
                              QStringLiteral("PhysicalKeys")));
    grid.cells.push_back(cell(QStringLiteral("dec_opacity"), QStringLiteral("−"), 0, 1,
                              QStringLiteral("settings.opacity.nudge.dec"), pal.nudge));
    grid.cells.push_back(cell(QStringLiteral("track_opacity"), QStringLiteral("Opacity"), 0, 2, {},
                              QColor(), 9, false, QStringLiteral("slider"),
                              QStringLiteral("opacity")));
    grid.cells.push_back(cell(QStringLiteral("inc_opacity"), QStringLiteral("+"), 0, 11,
                              QStringLiteral("settings.opacity.nudge.inc"), pal.nudge));
    grid.cells.push_back(cell(QStringLiteral("preview"), QStringLiteral("Preview"), 1, 0, {},
                              QColor(), 12, false, QStringLiteral("preview"),
                              QStringLiteral("%1%").arg(m_opacityDraft)));
    grid.cells.push_back(cell(QStringLiteral("save"), QStringLiteral("Save"), 2, 0,
                              QStringLiteral("settings.opacity.save"), pal.save, 6));
    grid.cells.push_back(cell(QStringLiteral("cancel"), QStringLiteral("Cancel"), 2, 6,
                              QStringLiteral("settings.opacity.cancel"), pal.cancel, 6));
    return doc;
}

void SettingsUi::refreshOpacityEditor()
{
    if (!m_opacity.active || m_numpad.active) {
        return;
    }
    QString err;
    if (!presentLive(m_opacity, QLatin1String(kLiveOpacity), buildOpacityDocument(), &err)) {
        notifyStatus(err);
        return;
    }
    if (PageHostWindow* w = m_pages.window()) {
        w->setPreviewColor(flashOpacityPreview());
    }
}

void SettingsUi::closeOpacityEditor()
{
    if (!m_opacity.active) {
        return;
    }
    if (m_scrub.active) {
        endSliderScrub(true);
    }
    if (m_opacitySetMode) {
        m_settings.flashUseForeground = false;
        apply(true);
    }
    m_opacitySetMode = false;
    QString err;
    closeLive(m_opacity);
}

void SettingsUi::opacityNudge(int dir)
{
    if (!m_opacity.active) {
        return;
    }
    if (m_scrub.active) {
        endSliderScrub(true);
    }
    m_opacityDraft = qBound(0, m_opacityDraft + dir, 100);
    refreshOpacityEditor();
}

bool SettingsUi::opacitySave(QString* error)
{
    if (!m_opacity.active) {
        if (error) {
            *error = QStringLiteral("Opacity editor is not open");
        }
        return false;
    }
    m_settings.flashUseForeground = true;
    m_settings.flashForegroundOpacity = m_opacityDraft;
    m_opacitySetMode = false;
    apply(true);
    const int pct = m_opacityDraft;
    closeOpacityEditor();
    notifyStatus(QStringLiteral("Flash opacity = %1%").arg(pct));
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
    PageDocument doc;
    doc.id = QStringLiteral("settings_color_live");
    doc.name = AppSettings::settingTitle(m_colorPickerKey);
    initGrid(doc, 12, 9, 1400, 980, 8, 20, m_settings.resolvedTheme());
    PageGrid& grid = doc.grids[0];
    const EditorSwatch pal = editorSwatch();

    const AppSettings draft = draftThemeSettings();
    ThemePalette themePal;
    themePal.colors = draft.customColors;
    themePal.progress = draft.colorKey(QStringLiteral("progressColor"));
    const bool themePicker = AppSettings::isThemeSeedKey(m_colorPickerKey);

    for (int i = 0; i < int(std::size(kColorAxes)); ++i) {
        const ColorAxis& axis = kColorAxes[i];
        grid.cells.push_back(cell(QStringLiteral("edit_%1").arg(QLatin1String(axis.id)),
                                  QStringLiteral("Edit"), i, 0,
                                  QStringLiteral("settings.color.scrub.%1").arg(QLatin1String(axis.id)),
                                  pal.edit, 1, true, {}, {}, QStringLiteral("PhysicalKeys")));
        grid.cells.push_back(cell(QStringLiteral("dec_%1").arg(QLatin1String(axis.id)),
                                  QStringLiteral("−"), i, 1,
                                  QStringLiteral("settings.color.nudge.%1.dec").arg(QLatin1String(axis.id)),
                                  pal.nudge));
        grid.cells.push_back(cell(QStringLiteral("track_%1").arg(QLatin1String(axis.id)),
                                  QLatin1String(axis.title), i, 2, {}, QColor(),
                                  themePicker ? 5 : 9, false, QStringLiteral("slider"),
                                  QLatin1String(axis.id)));
        grid.cells.push_back(cell(QStringLiteral("inc_%1").arg(QLatin1String(axis.id)),
                                  QStringLiteral("+"), i, themePicker ? 7 : 11,
                                  QStringLiteral("settings.color.nudge.%1.inc").arg(QLatin1String(axis.id)),
                                  pal.nudge));
    }

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
        {"use_surface", "Surface", "Panels and cells", "customSurfaceColor", themePal.colors.bgSurface,
         1},
        {"use_accent", "Accent", "Primary / buttons", "customPrimaryColor", themePal.colors.accent, 2},
        {"use_progress", "Progress", "Dwell and mouse-move", "customSecondaryColor", themePal.progress,
         3},
        {"use_highlight", "Highlight", "Selected / active fill", "customTertiaryColor",
         themePal.colors.cellActive, 4},
        {"use_text", "Foreground", "Primary text", "customTextColor", themePal.colors.text, 5},
        {"use_danger", "Danger", "Cancel / destructive", "customDangerColor", themePal.colors.danger,
         6},
    };
    for (const RoleRow& role : roles) {
        if (!themePicker) {
            break;
        }
        PageCell sw = cell(QLatin1String(role.id), QLatin1String(role.label), role.row, 8,
                           QStringLiteral("settings.color.select.%1").arg(QLatin1String(role.colorKey)),
                           role.color, 3, true, {}, QLatin1String(role.caption));
        sw.settingKey = QLatin1String(role.colorKey);
        sw.activeState = QStringLiteral("setting.color.editing.%1").arg(QLatin1String(role.colorKey));
        grid.cells.push_back(sw);
        const QColor suggested = draft.suggestedThemeColor(QLatin1String(role.colorKey));
        grid.cells.push_back(
            cell(QStringLiteral("suggest_%1").arg(QLatin1String(role.colorKey)),
                 QStringLiteral("Apply Suggested"), role.row, 11,
                 QStringLiteral("settings.color.suggest.%1").arg(QLatin1String(role.colorKey)),
                 suggested));
    }

    const QString hex = m_colorDraft.name(QColor::HexArgb).toUpper();
    grid.cells.push_back(cell(QStringLiteral("hex"), hex, 7, 0,
                              QStringLiteral("settings.color.editHex"), pal.value, 8));
    grid.cells.push_back(cell(QStringLiteral("save"), QStringLiteral("Save"), 8, 0,
                              QStringLiteral("settings.color.save"), pal.save, 4));
    grid.cells.push_back(cell(QStringLiteral("cancel"), QStringLiteral("Cancel"), 8, 4,
                              QStringLiteral("settings.color.cancel"), pal.cancel, 4));
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
        m_settings.applyCustomPalette(false);
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

bool SettingsUi::openHexEditor(QString* error)
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
    m_hexActive = true;
    QString hex = m_colorDraft.name(QColor::HexArgb).toUpper();
    if (hex.startsWith(QLatin1Char('#'))) {
        hex = hex.mid(1);
    }
    m_hexBuffer = hex;
    LiveBoard hexBoard;
    if (!presentLive(hexBoard, QLatin1String(kLiveHex), buildHexDocument(), error)) {
        m_hexActive = false;
        return false;
    }
    bindEditorKeyboard();
    notifyStatus(QStringLiteral("Enter hex color"));
    return true;
}

PageDocument SettingsUi::buildHexDocument() const
{
    PageDocument doc;
    doc.id = QStringLiteral("settings_hex_live");
    doc.name = QStringLiteral("Hex color");
    initGrid(doc, 4, 7, 560, 700, 10, 16, m_settings.resolvedTheme());
    PageGrid& grid = doc.grids[0];
    const EditorSwatch pal = editorSwatch();
    const QString shown =
        m_hexBuffer.isEmpty() ? QStringLiteral("#") : QStringLiteral("#%1").arg(m_hexBuffer);
    PageCell display = cell(QStringLiteral("display"), shown, 0, 0, {}, pal.value, 4, false,
                            QStringLiteral("value"));
    display.clusterSlot = QStringLiteral("value");
    grid.cells.push_back(display);

    const char* keys[] = {"1", "2", "3", "A", "4", "5", "6", "B",
                          "7", "8", "9", "C", "0", "D", "E", "F"};
    for (int i = 0; i < 16; ++i) {
        const int row = 1 + i / 4;
        const int col = i % 4;
        const QString k = QLatin1String(keys[i]);
        grid.cells.push_back(cell(QStringLiteral("h_%1").arg(k), k, row, col,
                                  QStringLiteral("settings.hex.digit.%1").arg(k), pal.key));
    }
    grid.cells.push_back(cell(QStringLiteral("back"), QStringLiteral("⌫"), 5, 0,
                              QStringLiteral("settings.hex.backspace"), pal.warn, 2));
    grid.cells.push_back(cell(QStringLiteral("clear"), QStringLiteral("Clear"), 5, 2,
                              QStringLiteral("settings.hex.clear"), pal.warn, 2));
    grid.cells.push_back(cell(QStringLiteral("save"), QStringLiteral("Save"), 6, 0,
                              QStringLiteral("settings.hex.save"), pal.save, 2));
    grid.cells.push_back(cell(QStringLiteral("cancel"), QStringLiteral("Cancel"), 6, 2,
                              QStringLiteral("settings.hex.cancel"), pal.cancel, 2));
    return doc;
}

void SettingsUi::hexAppend(QChar ch)
{
    if (!m_hexActive) {
        return;
    }
    if (m_hexBuffer.size() >= 8) {
        return;
    }
    m_hexBuffer += ch.toUpper();
    refreshHexEditor();
}

void SettingsUi::hexBackspace()
{
    if (!m_hexActive || m_hexBuffer.isEmpty()) {
        return;
    }
    m_hexBuffer.chop(1);
    refreshHexEditor();
}

void SettingsUi::refreshHexEditor()
{
    if (!m_hexActive) {
        return;
    }
    QString err;
    LiveBoard hexBoard;
    (void)presentLive(hexBoard, QLatin1String(kLiveHex), buildHexDocument(), &err);
}

bool SettingsUi::hexSave(QString* error)
{
    if (!m_hexActive) {
        if (error) {
            *error = QStringLiteral("Hex editor is not open");
        }
        return false;
    }
    QString hex = m_hexBuffer;
    if (!hex.startsWith(QLatin1Char('#'))) {
        hex.prepend(QLatin1Char('#'));
    }
    const QColor c = AppSettings::parseColor(hex);
    if (!c.isValid()) {
        const QString msg = QStringLiteral("Invalid hex color");
        notifyStatus(msg);
        if (error) {
            *error = msg;
        }
        return false;
    }
    unbindEditorKeyboard();
    m_pages.closePage(QLatin1String(kLiveHex));
    m_hexActive = false;
    m_hexBuffer.clear();
    loadColorDraft(c);
    m_colorPending.insert(m_colorPickerKey, m_colorDraft);
    refreshColorPicker();
    notifyStatus(QStringLiteral("Hex %1").arg(c.name(QColor::HexArgb).toUpper()));
    return true;
}

bool SettingsUi::hexCancel(QString* error)
{
    Q_UNUSED(error);
    if (!m_hexActive) {
        return true;
    }
    unbindEditorKeyboard();
    m_pages.closePage(QLatin1String(kLiveHex));
    m_hexActive = false;
    m_hexBuffer.clear();
    refreshColorPicker();
    notifyStatus(QStringLiteral("Hex edit cancelled"));
    return true;
}

} // namespace gazer
