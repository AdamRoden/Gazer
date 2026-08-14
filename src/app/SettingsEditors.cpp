#include "app/SettingsUi.h"

#include "layout/LayoutInstance.h"
#include "layout/LayoutInstanceManager.h"
#include "layout/LayoutManager.h"
#include "ui/LayoutQuickWindow.h"
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

LayoutItem cell(const QString& id, const QString& label, int row, int col,
                const QString& command, const QColor& bg, int colSpan = 1,
                bool interactive = true, const QString& role = {},
                const QString& caption = {})
{
    return SettingsUi::makeItem(id, label, row, col,
                                interactive ? LayoutAction::Type::Command
                                            : LayoutAction::Type::Unknown,
                                command, bg, colSpan, interactive, caption, {}, role);
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

bool SettingsUi::isLiveEditorInstance(const QString& instanceId) const
{
    if (instanceId.isEmpty()) {
        return false;
    }
    return (m_numpad.active && instanceId == m_numpad.instanceId)
           || (m_array.active && instanceId == m_array.instanceId)
           || (m_color.active && instanceId == m_color.instanceId);
}

bool SettingsUi::returnEditorInstance(const QString& instId, const QString& layoutId,
                                      QString* error)
{
    if (instId.isEmpty() || layoutId.isEmpty()) {
        return true;
    }
    QString loadErr;
    if (m_instances.loadInto(instId, layoutId, &loadErr)) {
        return true;
    }
    const LayoutDocument* src = m_catalog.document(layoutId);
    if (!src) {
        if (error) {
            *error = loadErr;
        }
        return false;
    }
    LayoutDocument copy = *src;
    decorateDocument(copy);
    return m_instances.setInstanceDocument(instId, copy, error);
}

void SettingsUi::applyPreviewColor()
{
    auto* inst = m_instances.instance(m_color.instanceId);
    if (inst && inst->window()) {
        inst->window()->setPreviewColor(m_colorDraft);
    }
}

bool SettingsUi::openArrayEditor(const QString& settingKey, QString* error)
{
    auto* focused = m_instances.focusedInstance();
    if (!focused) {
        if (error) {
            *error = QStringLiteral("No focused board for sequence editor");
        }
        return false;
    }
    m_array.active = true;
    m_array.instanceId = focused->instanceId();
    m_array.returnLayoutId = focused->layoutId();
    m_arrayKey = settingKey;
    m_arrayDraft = m_settings.dwellSequence;
    if (m_arrayDraft.isEmpty()) {
        m_arrayDraft = AppSettings::defaultDwellSequence();
    }
    refreshArrayEditor();
    notifyStatus(QStringLiteral("Edit dwell sequence"));
    return true;
}

LayoutDocument SettingsUi::buildArrayDocument() const
{
    LayoutDocument doc;
    doc.schemaVersion = 1;
    doc.id = QStringLiteral("settings_array_live");
    doc.name = QStringLiteral("Dwell sequence");
    doc.description = QStringLiteral("Each step is a dwell time in ms. Last step repeats.");
    SettingsUi::applyLiveEditorChrome(doc);

    const int n = qBound(1, m_arrayDraft.size(), kMaxArraySteps);
    doc.grid.columns = 5;
    doc.grid.rows = n + 2;
    doc.grid.gapPx = 10;
    doc.grid.marginPx = 20;
    doc.placement.width = DimSpec::pixels(920);
    doc.placement.height = DimSpec::pixels(200 + (n + 2) * 68);

    const EditorSwatch sw = editorSwatch();
    const QColor decBg = sw.nudge;
    const QColor incBg = sw.nudge;
    const QColor editBg = sw.edit;
    const QColor delBg = sw.cancel;
    const QColor valBg = sw.value;

    for (int i = 0; i < n; ++i) {
        const int ms = m_arrayDraft[i];
        doc.items.push_back(cell(QStringLiteral("dec_%1").arg(i), QStringLiteral("−"), i, 0,
                                 QStringLiteral("settings.array.nudge.%1.dec").arg(i), decBg));
        doc.items.push_back(cell(QStringLiteral("val_%1").arg(i), QStringLiteral("%1 ms").arg(ms),
                                 i, 1, {}, valBg, 1, false, QStringLiteral("value")));
        doc.items.push_back(cell(QStringLiteral("inc_%1").arg(i), QStringLiteral("+"), i, 2,
                                 QStringLiteral("settings.array.nudge.%1.inc").arg(i), incBg));
        doc.items.push_back(cell(QStringLiteral("edit_%1").arg(i), QStringLiteral("Edit"), i, 3,
                                 QStringLiteral("settings.array.edit.%1").arg(i), editBg));
        doc.items.push_back(cell(QStringLiteral("del_%1").arg(i), QStringLiteral("Trash"), i, 4,
                                 QStringLiteral("settings.array.del.%1").arg(i), delBg));
    }

    const int bar = n;
    doc.items.push_back(cell(QStringLiteral("decAll"), QStringLiteral("− all"), bar, 0,
                             QStringLiteral("settings.array.decAll"), sw.nudge));
    doc.items.push_back(cell(QStringLiteral("reset"), QStringLiteral("Reset"), bar, 1,
                             QStringLiteral("settings.array.reset"), sw.warn));
    doc.items.push_back(cell(QStringLiteral("incAll"), QStringLiteral("+ all"), bar, 2,
                             QStringLiteral("settings.array.incAll"), sw.nudge));
    doc.items.push_back(cell(QStringLiteral("add"), QStringLiteral("Add"), bar, 3,
                             QStringLiteral("settings.array.add"), sw.add, 2));

    const int foot = n + 1;
    doc.items.push_back(cell(QStringLiteral("save"), QStringLiteral("Save"), foot, 0,
                             QStringLiteral("settings.array.save"), sw.save, 3));
    doc.items.push_back(cell(QStringLiteral("cancel"), QStringLiteral("Cancel"), foot, 3,
                             QStringLiteral("settings.array.cancel"), sw.cancel, 2));
    return doc;
}

void SettingsUi::refreshArrayEditor()
{
    if (!m_array.active) {
        return;
    }
    QString err;
    if (!m_instances.setInstanceDocument(m_array.instanceId, buildArrayDocument(), &err)) {
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
    const QString instId = m_array.instanceId;
    const QString returnId = m_array.returnLayoutId;
    m_array.reset();
    m_arrayKey.clear();
    m_arrayDraft.clear();
    if (!returnEditorInstance(instId, returnId, error)) {
        return false;
    }
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
    const QString instId = m_array.instanceId;
    const QString returnId = m_array.returnLayoutId;
    m_array.reset();
    m_arrayKey.clear();
    m_arrayDraft.clear();
    if (!returnEditorInstance(instId, returnId, error)) {
        notifyStatus(QStringLiteral("Cancelled"));
        return false;
    }
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
    auto* inst = m_instances.instance(m_array.instanceId);
    if (!inst) {
        if (error) {
            *error = QStringLiteral("Sequence editor instance missing");
        }
        return false;
    }
    m_numpad.active = true;
    m_numpad.instanceId = m_array.instanceId;
    m_numpad.returnLayoutId.clear();
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
        updateLiveThemeSwatches();
    }
    return true;
}

void SettingsUi::updateLiveThemeSwatches()
{
    auto* inst = m_instances.instance(m_color.instanceId);
    if (!inst || !m_color.active) {
        return;
    }
    const AppSettings draft = draftThemeSettings();
    const struct {
        const char* id;
        const char* key;
        QColor color;
    } rows[] = {
        {"use_window", "customBgColor", draft.customColors.bgMain},
        {"use_surface", "customSurfaceColor", draft.customColors.bgSurface},
        {"use_accent", "customPrimaryColor", draft.customColors.accent},
        {"use_progress", "customSecondaryColor", draft.colorKey(QStringLiteral("progressColor"))},
        {"use_highlight", "customTertiaryColor", draft.customColors.cellActive},
        {"use_text", "customTextColor", draft.customColors.text},
        {"use_danger", "customDangerColor", draft.customColors.danger},
    };
    inst->mutateItems([&](LayoutItem& item) {
        for (const auto& row : rows) {
            if (item.id == QLatin1String(row.id)) {
                item.style.background = row.color;
                item.style.foreground = ThemeColors::contrastOn(row.color);
                break;
            }
            if (item.id == QStringLiteral("suggest_%1").arg(QLatin1String(row.key))) {
                const QColor sug = draft.suggestedThemeColor(QLatin1String(row.key));
                item.style.background = sug;
                item.style.foreground = ThemeColors::contrastOn(sug);
                break;
            }
        }
    });
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
        || ch == QLatin1String("val") || ch == QLatin1String("a") || ch == QLatin1String("alpha")) {
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
        || ch == QLatin1String("val") || ch == QLatin1String("a") || ch == QLatin1String("alpha")) {
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
    if (!m_color.active || !findColorAxis(channel)) {
        return false;
    }
    if (m_scrub.active && m_scrub.channel == channel) {
        return true;
    }
    if (m_scrub.active) {
        endSliderScrub(true);
    }
    auto* inst = m_instances.instance(m_color.instanceId);
    if (!inst) {
        return false;
    }
    m_scrub.active = true;
    m_scrub.channel = channel;
    m_scrub.itemId = QStringLiteral("track_%1").arg(channel);
    m_scrub.instanceId = inst->instanceId();
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
    const QString instId = m_scrub.instanceId;
    if (!commit) {
        loadColorDraft(m_scrubRevert);
    }
    m_scrub.reset();
    m_scrubDeadlineMs = -1;
    m_scrubLastSampleMs = -1;
    m_scrubDwell.reset();
    if (auto* inst = m_instances.instance(instId.isEmpty() ? m_color.instanceId : instId)) {
        if (inst->window()) {
            inst->window()->clearSliderScrub();
        }
    }
    refreshColorPicker();
}

int SettingsUi::scrubShownValue() const
{
    return colorShownValue(m_scrub.channel);
}

void SettingsUi::syncSliderScrubVisuals()
{
    auto* inst = m_instances.instance(m_scrub.instanceId.isEmpty() ? m_color.instanceId
                                                                  : m_scrub.instanceId);
    if (!inst || !inst->window()) {
        return;
    }
    const int shown = scrubShownValue();
    int minV = 0;
    int maxV = 255;
    colorChannelRange(m_scrub.channel, &minV, &maxV);
    const double t = (maxV > minV) ? (shown - minV) / double(maxV - minV) : 0.0;
    if (m_color.active) {
        inst->window()->setPreviewColor(m_colorDraft);
        updateLiveThemeSwatches();
    }
    inst->window()->setSliderScrub(m_scrub.itemId, t, colorChannelValueText(m_scrub.channel, shown),
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
    auto* inst = m_instances.instance(m_scrub.instanceId.isEmpty() ? m_color.instanceId
                                                                  : m_scrub.instanceId);
    if (!inst || !inst->window()) {
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

    const QString hit = inst->hitTest(point.toPointF());
    const QString editId = QStringLiteral("edit_%1").arg(m_scrub.channel);
    const QString decId = QStringLiteral("dec_%1").arg(m_scrub.channel);
    const QString incId = QStringLiteral("inc_%1").arg(m_scrub.channel);
    const bool companion = hit == m_scrub.itemId || hit == editId || hit == decId || hit == incId;
    if (!hit.isEmpty() && !companion) {
        const LayoutItem* other = inst->document().findItem(hit);
        if (other && other->interactive) {
            endSliderScrub(true);
            return;
        }
    }

    const QRectF local = inst->window()->itemLocalRects().value(m_scrub.itemId);
    if (local.isEmpty()) {
        endSliderScrub(true);
        return;
    }
    const QPoint origin = inst->window()->boardTopLeftGlobal();
    const double localX = point.x - origin.x();
    const double localY = point.y - origin.y();
    const LayoutQuickWindow::SliderVisual geom =
        LayoutQuickWindow::sliderVisual(local, /*scrubbing=*/true);
    const QRectF bound = local.adjusted(0, -12, 0, 12);
    if (!bound.contains(QPointF(localX, localY))) {
        return;
    }

    const double t = geom.tAtX(localX);
    int minV = 0;
    int maxV = 255;
    colorChannelRange(m_scrub.channel, &minV, &maxV);
    const int shown = minV + qRound(t * double(maxV - minV));
    applyColorShownValue(m_scrub.channel, shown);

    const double dtSec =
        m_scrubLastSampleMs < 0 ? 0.016
                                : qBound(0.004, (now - m_scrubLastSampleMs) / 1000.0, 0.08);
    m_scrubLastSampleMs = now;
    const QPointF sample(geom.posAt(t).x() + origin.x(), geom.trackCy + origin.y());
    const bool done = m_scrubDwell.sample(sample, dtSec);
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
    m_color.active = true;
    m_color.instanceId = focused->instanceId();
    m_color.returnLayoutId = focused->layoutId();
    m_colorPickerKey = colorKey;
    m_colorPending.clear();
    colorUseSaved(colorKey);
    m_colorPending.insert(colorKey, m_colorDraft);
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

LayoutDocument SettingsUi::buildColorDocument() const
{
    LayoutDocument doc;
    doc.schemaVersion = 1;
    doc.id = QStringLiteral("settings_color_live");
    doc.name = AppSettings::settingTitle(m_colorPickerKey);
    doc.description = QStringLiteral("HSV + RGB + alpha. Save applies this color.");
    SettingsUi::applyLiveEditorChrome(doc);
    doc.grid.columns = 12;
    doc.grid.rows = 9;
    doc.grid.gapPx = 8;
    doc.grid.marginPx = 20;
    doc.placement.width = DimSpec::pixels(1400);
    doc.placement.height = DimSpec::pixels(980);

    const EditorSwatch pal = editorSwatch();
    const AppSettings draft = draftThemeSettings();
    ThemePalette themePal;
    themePal.colors = draft.customColors;
    themePal.progress = draft.colorKey(QStringLiteral("progressColor"));
    themePal.progressFill = draft.colorKey(QStringLiteral("progressFillColor"));
    themePal.progressBorder = draft.colorKey(QStringLiteral("progressBorderColor"));
    const bool themePicker = AppSettings::isThemeSeedKey(m_colorPickerKey);

    for (int i = 0; i < int(std::size(kColorAxes)); ++i) {
        const ColorAxis& axis = kColorAxes[i];
        LayoutItem edit;
        edit.id = QStringLiteral("edit_%1").arg(QLatin1String(axis.id));
        edit.label = QStringLiteral("Edit");
        edit.icon = QStringLiteral("edit");
        edit.row = i;
        edit.col = 0;
        edit.action.type = LayoutAction::Type::Command;
        edit.action.name = QStringLiteral("settings.color.scrub.%1").arg(QLatin1String(axis.id));
        edit.style.background = pal.edit;
        edit.style.foreground = ThemeColors::contrastOn(pal.edit);
        doc.items.push_back(edit);

        doc.items.push_back(cell(QStringLiteral("dec_%1").arg(QLatin1String(axis.id)),
                                 QStringLiteral("−"), i, 1,
                                 QStringLiteral("settings.color.nudge.%1.dec").arg(QLatin1String(axis.id)),
                                 pal.nudge));

        LayoutItem track;
        track.id = QStringLiteral("track_%1").arg(QLatin1String(axis.id));
        track.role = QStringLiteral("slider");
        track.caption = QLatin1String(axis.id);
        track.label = QLatin1String(axis.title);
        track.interactive = false;
        track.row = i;
        track.col = 2;
        track.colSpan = themePicker ? 5 : 9;
        doc.items.push_back(track);

        doc.items.push_back(cell(QStringLiteral("inc_%1").arg(QLatin1String(axis.id)),
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
        LayoutItem sw;
        sw.id = QLatin1String(role.id);
        sw.role = QStringLiteral("swatch");
        sw.label = QLatin1String(role.label);
        sw.caption = QLatin1String(role.caption);
        sw.settingKey = QLatin1String(role.colorKey);
        sw.activeState = QStringLiteral("setting.color.editing.%1").arg(QLatin1String(role.colorKey));
        sw.interactive = true;
        sw.row = role.row;
        sw.col = 8;
        sw.colSpan = 3;
        sw.style.background = role.color;
        sw.style.foreground = ThemeColors::contrastOn(role.color);
        sw.action.type = LayoutAction::Type::Command;
        sw.action.name = QStringLiteral("settings.color.select.%1").arg(QLatin1String(role.colorKey));
        doc.items.push_back(sw);

        const QColor suggested = draft.suggestedThemeColor(QLatin1String(role.colorKey));
        LayoutItem lock;
        lock.id = QStringLiteral("suggest_%1").arg(QLatin1String(role.colorKey));
        lock.role = QStringLiteral("swatch");
        lock.label = QStringLiteral("Apply Suggested");
        lock.row = role.row;
        lock.col = 11;
        lock.action.type = LayoutAction::Type::Command;
        lock.action.name =
            QStringLiteral("settings.color.suggest.%1").arg(QLatin1String(role.colorKey));
        lock.style.background = suggested;
        lock.style.foreground = ThemeColors::contrastOn(suggested);
        doc.items.push_back(lock);
    }

    const QString hex = m_colorDraft.name(QColor::HexArgb).toUpper();
    doc.items.push_back(cell(QStringLiteral("hex"), hex, 7, 0,
                             QStringLiteral("settings.color.editHex"), pal.value, 8));
    doc.items.push_back(cell(QStringLiteral("save"), QStringLiteral("Save"), 8, 0,
                             QStringLiteral("settings.color.save"), pal.save, 4));
    doc.items.push_back(cell(QStringLiteral("cancel"), QStringLiteral("Cancel"), 8, 4,
                             QStringLiteral("settings.color.cancel"), pal.cancel, 4));
    return doc;
}

void SettingsUi::refreshColorPicker()
{
    if (!m_color.active || m_hexActive || m_numpad.active) {
        return;
    }
    QString err;
    if (!m_instances.setInstanceDocument(m_color.instanceId, buildColorDocument(), &err)) {
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
        if (auto* inst = m_instances.instance(m_color.instanceId)) {
            if (inst->window()) {
                inst->window()->clearSliderScrub();
            }
        }
    }
    const QString instId = m_color.instanceId;
    const QString returnId = m_color.returnLayoutId;
    m_color.reset();
    m_colorPending.clear();
    m_hexActive = false;
    m_colorPickerKey.clear();
    m_hexBuffer.clear();
    QString err;
    (void)returnEditorInstance(instId, returnId, &err);
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
    m_numpad.active = true;
    m_numpad.instanceId = m_color.instanceId;
    m_numpad.returnLayoutId.clear();
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
    if (!m_instances.setInstanceDocument(m_color.instanceId, buildHexDocument(), error)) {
        m_hexActive = false;
        return false;
    }
    notifyStatus(QStringLiteral("Enter hex color"));
    return true;
}

LayoutDocument SettingsUi::buildHexDocument() const
{
    LayoutDocument doc;
    doc.schemaVersion = 1;
    doc.id = QStringLiteral("settings_hex_live");
    doc.name = QStringLiteral("Hex color");
    doc.description = QStringLiteral("#AARRGGBB or RRGGBB");
    SettingsUi::applyLiveEditorChrome(doc);
    doc.grid.columns = 4;
    doc.grid.rows = 7;
    doc.grid.gapPx = 10;
    doc.grid.marginPx = 16;
    doc.placement.width = DimSpec::pixels(560);
    doc.placement.height = DimSpec::pixels(700);

    const EditorSwatch pal = editorSwatch();
    const QString shown =
        m_hexBuffer.isEmpty() ? QStringLiteral("#") : QStringLiteral("#%1").arg(m_hexBuffer);
    doc.items.push_back(cell(QStringLiteral("display"), shown, 0, 0, {}, pal.value, 4, false,
                             QStringLiteral("value")));

    const char* keys[] = {"1", "2", "3", "A", "4", "5", "6", "B",
                          "7", "8", "9", "C", "0", "D", "E", "F"};
    for (int i = 0; i < 16; ++i) {
        const int row = 1 + i / 4;
        const int col = i % 4;
        const QString k = QLatin1String(keys[i]);
        doc.items.push_back(cell(QStringLiteral("h_%1").arg(k), k, row, col,
                                 QStringLiteral("settings.hex.digit.%1").arg(k), pal.key));
    }
    doc.items.push_back(cell(QStringLiteral("back"), QStringLiteral("⌫"), 5, 0,
                             QStringLiteral("settings.hex.backspace"), pal.warn, 2));
    doc.items.push_back(cell(QStringLiteral("clear"), QStringLiteral("Clear"), 5, 2,
                             QStringLiteral("settings.hex.clear"), pal.warn, 2));
    doc.items.push_back(cell(QStringLiteral("save"), QStringLiteral("Save"), 6, 0,
                             QStringLiteral("settings.hex.save"), pal.save, 2));
    doc.items.push_back(cell(QStringLiteral("cancel"), QStringLiteral("Cancel"), 6, 2,
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
    (void)m_instances.setInstanceDocument(m_color.instanceId, buildHexDocument(), &err);
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
    m_hexActive = false;
    m_hexBuffer.clear();
    refreshColorPicker();
    notifyStatus(QStringLiteral("Hex edit cancelled"));
    return true;
}

} // namespace gazer
