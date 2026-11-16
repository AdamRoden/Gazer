#include "app/SettingsUi.h"

#include "app/ComposeUi.h"
#include "app/SettingsCatalog.h"
#include "app/SettingsPageBuild.h"
#include "app/SettingsUiInternal.h"
#include "assist/AudioOutput.h"
#include "assist/ElevenRequest.h"
#include "assist/GazeFollowProfile.h"
#include "assist/LookToMap.h"
#include "assist/VoiceCatalog.h"
#include "layout/PageEdit.h"
#include "layout/PageSession.h"
#include "ui/PickStyle.h"
#include "ui/Theme.h"
#include "utils/ScreenCaptureMode.h"

#include <QColor>
#include <QStringList>

namespace gazer {

namespace {

struct Choice {
    QString label;
    QString token;
    bool selected = false;
    bool asToggle = false;
    QString status;
    /// Empty for the mouse-pick mask, speech output, and head-pose Set.
    /// Those saves stay open when they fail, so they do not go through `writeDefault`.
    std::function<void(AppSettings&)> apply;
};

Choice applied(const char* token, const char* label, bool selected, const char* status,
               std::function<void(AppSettings&)> apply)
{
    Choice choice;
    choice.label = QString::fromLatin1(label);
    choice.token = QString::fromLatin1(token);
    choice.selected = selected;
    choice.status = QString::fromLatin1(status);
    choice.apply = std::move(apply);
    return choice;
}

QString defaultsBoardFor(const SettingsCatalogEntry& entry)
{
    const auto page = QLatin1String(entry.page);
    if (page == QLatin1String("Dwell")) {
        return QStringLiteral("main_settings_dwell");
    }
    if (page == QLatin1String("Pointer")) {
        return QStringLiteral("main_settings_pointer");
    }
    if (page == QLatin1String("Gaze")) {
        return QStringLiteral("main_settings_gaze");
    }
    if (page == QLatin1String("Head")) {
        return QStringLiteral("main_settings_head_pose");
    }
    if (page == QLatin1String("Speak")) {
        return QStringLiteral("main_settings_speak");
    }
    if (page == QLatin1String("Theme")) {
        return QStringLiteral("main_settings_theme");
    }
    if (page == QLatin1String("Setup")) {
        return QStringLiteral("main_settings_setup");
    }
    return {};
}

QString choiceTitle(const QString& id)
{
    if (const SettingsCatalogEntry* entry = findSettingsCatalogEntry(id)) {
        return QString::fromLatin1(entry->title);
    }
    return id;
}

int mousePickFlag(const QString& token)
{
    if (token == QLatin1String("cursor")) {
        return PickStyle::Cursor;
    }
    if (token == QLatin1String("dot")) {
        return PickStyle::Dot;
    }
    if (token == QLatin1String("crosshair")) {
        return PickStyle::Crosshair;
    }
    return 0;
}

QVector<Choice> defaultsChoices(const AppSettings& settings, const QString& id)
{
    if (id == QLatin1String("magFollowProfile")) {
        const struct {
            const char* token;
            const char* label;
            const char* status;
            GazeFollowProfile profile;
        } rows[] = {
            {"slow", "Slow", "Gaze follow: Slow", GazeFollowProfile::Slow},
            {"sticky", "Sticky", "Gaze follow: Sticky", GazeFollowProfile::Sticky},
            {"smooth", "Smooth", "Gaze follow: Smooth", GazeFollowProfile::Smooth},
            {"snappy", "Snappy", "Gaze follow: Snappy", GazeFollowProfile::Snappy},
        };
        QVector<Choice> out;
        for (const auto& row : rows) {
            const GazeFollowProfile profile = row.profile;
            out.push_back(applied(row.token, row.label, settings.magFollowProfile == profile,
                                  row.status, [profile](AppSettings& next) {
                                      next.setMagFollowProfile(int(profile));
                                  }));
        }
        return out;
    }
    if (id == QLatin1String("trackerPref")) {
        return {
            applied("auto", "Auto Tobii", settings.trackerPref != 1,
                    "Tracker: Auto Tobii (restart to apply)",
                    [](AppSettings& next) { next.trackerPref = 0; }),
            applied("mouse", "Mouse only", settings.trackerPref == 1,
                    "Tracker: Mouse only (restart to apply)",
                    [](AppSettings& next) { next.trackerPref = 1; }),
        };
    }
    if (id == QLatin1String("screenCapture")) {
        const struct {
            const char* token;
            const char* label;
            const char* status;
            ScreenCaptureMode mode;
        } rows[] = {
            {"all", "All", "Screen capture: all (for documentation)", ScreenCaptureMode::All},
            {"pages", "Pages", "Screen capture: pages", ScreenCaptureMode::Pages},
            {"none", "None", "Screen capture: none", ScreenCaptureMode::None},
        };
        QVector<Choice> out;
        for (const auto& row : rows) {
            const ScreenCaptureMode mode = row.mode;
            out.push_back(applied(row.token, row.label, settings.screenCapture == mode, row.status,
                                  [mode](AppSettings& next) { next.screenCapture = mode; }));
        }
        return out;
    }
    if (id == QLatin1String("themeAppearance")) {
        const struct {
            const char* token;
            const char* label;
            const char* status;
            ThemeAppearance appearance;
        } rows[] = {
            {"light", "Light", "Theme: Light", ThemeAppearance::Light},
            {"lightTinted", "Light tinted", "Theme: Light tint", ThemeAppearance::LightTinted},
            {"darkTinted", "Dark tinted", "Theme: Dark tint", ThemeAppearance::DarkTinted},
            {"dark", "Dark", "Theme: Dark", ThemeAppearance::Dark},
        };
        QVector<Choice> out;
        for (const auto& row : rows) {
            const ThemeAppearance appearance = row.appearance;
            out.push_back(applied(row.token, row.label, settings.themeAppearance == appearance,
                                  row.status, [appearance](AppSettings& next) {
                                      next.setThemeAppearance(appearance);
                                  }));
        }
        return out;
    }
    if (id == QLatin1String("themeTintFamily")) {
        const struct {
            const char* token;
            const char* label;
            const char* status;
            ThemeTintFamily family;
        } rows[] = {
            {"none", "None", "Tint: None", ThemeTintFamily::None},
            {"primary", "Primary", "Tint: Primary", ThemeTintFamily::Primary},
            {"complementary", "Complementary", "Tint: Complementary",
             ThemeTintFamily::Complementary},
            {"analogous1", "Analogous 1", "Tint: Analogous 1", ThemeTintFamily::Analogous1},
            {"analogous2", "Analogous 2", "Tint: Analogous 2", ThemeTintFamily::Analogous2},
            {"tertiary1", "Tertiary 1", "Tint: Tertiary 1", ThemeTintFamily::Tertiary1},
            {"tertiary2", "Tertiary 2", "Tint: Tertiary 2", ThemeTintFamily::Tertiary2},
        };
        QVector<Choice> out;
        for (const auto& row : rows) {
            const ThemeTintFamily family = row.family;
            out.push_back(applied(row.token, row.label, settings.themeTintFamily == family,
                                  row.status, [family](AppSettings& next) {
                                      next.setThemeTintFamily(family);
                                  }));
        }
        return out;
    }
    if (id == QLatin1String("themeBrightness")) {
        QVector<Choice> out;
        for (int i = 0; i < kThemeBrightnessLevels; ++i) {
            Choice choice;
            choice.label = QString::number(i);
            choice.token = choice.label;
            choice.selected = settings.themeBrightness == i;
            choice.status = QStringLiteral("Background shade %1").arg(i + 1);
            choice.apply = [i](AppSettings& next) { next.setThemeBrightness(i); };
            out.push_back(std::move(choice));
        }
        return out;
    }
    if (id == QLatin1String("speechModel")) {
        const QString live = ElevenRequest::normalizeModelId(settings.speechModel);
        const QString sapiId = QString(ElevenRequest::kModelSapi);
        const QString turboId = QString(ElevenRequest::kModelTurbo);
        Choice sapi;
        sapi.label = QStringLiteral("SAPI");
        sapi.token = sapiId;
        sapi.selected = live == sapiId;
        sapi.status = QStringLiteral("Speech model: SAPI");
        sapi.apply = [sapiId](AppSettings& next) {
            next.speechModel = ElevenRequest::normalizeModelId(sapiId);
        };
        Choice eleven;
        eleven.label = QStringLiteral("ElevenLabs");
        eleven.token = turboId;
        eleven.selected = live == turboId;
        eleven.status = QStringLiteral("Speech model: ElevenLabs");
        eleven.apply = [turboId](AppSettings& next) {
            next.speechModel = ElevenRequest::normalizeModelId(turboId);
        };
        return {sapi, eleven};
    }
    if (id == QLatin1String("mousePickStyle")) {
        const struct {
            const char* token;
            const char* label;
            PickStyle::Flag flag;
        } rows[] = {
            {"cursor", "Cursor", PickStyle::Cursor},
            {"dot", "Dot", PickStyle::Dot},
            {"crosshair", "Crosshair", PickStyle::Crosshair},
        };
        QVector<Choice> out;
        for (const auto& row : rows) {
            Choice choice;
            choice.label = QString::fromLatin1(row.label);
            choice.token = QString::fromLatin1(row.token);
            choice.selected = PickStyle::has(settings.mousePickStyle, row.flag);
            out.push_back(std::move(choice));
        }
        return out;
    }
    if (id == QLatin1String("customTiming")) {
        return {
            applied("save", "Save current", false, "Custom timing saved",
                    [](AppSettings& next) { next.saveDwellCustom(); }),
            applied("restore", "Restore saved", false, "Custom timing restored",
                    [](AppSettings& next) { next.applyDwellCustom(); }),
        };
    }
    if (id == QLatin1String("headPoseOrigin")) {
        Choice set;
        set.label = QStringLiteral("Set from current pose");
        set.token = QStringLiteral("set");
        Choice clear = applied("clear", "Clear", false, "Head pose origin cleared",
                               [](AppSettings& next) { next.headPoseOriginSet = false; });
        return {set, clear};
    }
    if (id == QLatin1String("speechOutput")) {
        QVector<Choice> out;
        Choice system;
        system.label = QStringLiteral("System default");
        system.token = QStringLiteral("default");
        system.selected = settings.speechOutputId.isEmpty();
        out.push_back(system);
        bool saw = settings.speechOutputId.isEmpty();
        const QVector<AudioOutput::PlaybackDevice> devices = AudioOutput::playbackDevices();
        for (const AudioOutput::PlaybackDevice& dev : devices) {
            if (dev.key.isEmpty()) {
                continue;
            }
            Choice choice;
            choice.label = dev.name;
            choice.token = VoiceCatalog::encodeId(dev.key);
            choice.selected = dev.key == settings.speechOutputId;
            if (choice.selected) {
                saw = true;
            }
            out.push_back(choice);
        }
        if (!saw) {
            Choice missing;
            missing.label = settings.speechOutputName.isEmpty() ? QStringLiteral("Saved device")
                                                                : settings.speechOutputName;
            missing.token = VoiceCatalog::encodeId(settings.speechOutputId);
            missing.selected = true;
            out.push_back(missing);
        }
        return out;
    }
    return {};
}

} // namespace

void SettingsUi::clearDefaultsDraft()
{
    m_defaultsChoiceId.clear();
    m_defaultsDraftKind = DefaultsDraftKind::None;
    m_defaultsDraftOn = false;
    m_defaultsDraftToken.clear();
    m_defaultsDraftMask = 0;
}

void SettingsUi::closeDefaultsEditor()
{
    clearDefaultsDraft();
    if (m_defaultsPopup.active) {
        closeLive(m_defaultsPopup);
    }
}

void SettingsUi::noteDefaultsSession()
{
    if (m_defaultsSearchOpen && (!m_compose || !m_compose->isOpen())) {
        m_defaultsSearchOpen = false;
    }
    if (m_defaultsPopup.active && !m_pages.hasPage(m_defaultsPopup.pageId)) {
        m_defaultsPopup.reset();
        // The inline switch has no page. A closed pad drops its draft.
        if (m_defaultsDraftKind != DefaultsDraftKind::Bool) {
            clearDefaultsDraft();
        }
    }
}

bool SettingsUi::defaultsDraftActive(const QString& rest) const
{
    const int dot = rest.indexOf(QLatin1Char('.'));
    const QString id = dot < 0 ? rest : rest.left(dot);
    const QString token = dot < 0 ? QString() : rest.mid(dot + 1);
    if (id != m_defaultsChoiceId) {
        return false;
    }
    if (m_defaultsDraftKind == DefaultsDraftKind::Bool) {
        return token.isEmpty() && m_defaultsDraftOn;
    }
    if (m_defaultsDraftKind == DefaultsDraftKind::Mask) {
        const int flag = mousePickFlag(token);
        return flag != 0 && (m_defaultsDraftMask & flag) != 0;
    }
    if (m_defaultsDraftKind == DefaultsDraftKind::Token) {
        return !token.isEmpty() && token == m_defaultsDraftToken;
    }
    return false;
}

void SettingsUi::writeDefault(const std::function<void(AppSettings&)>& fn, const QString& status)
{
    closeDefaultsEditor();
    if (m_mutate) {
        m_mutate(fn, status);
    } else {
        fn(m_settings);
        apply(true);
        if (!status.isEmpty()) {
            notifyStatus(status);
        }
    }
    m_pages.refreshDecorated();
    m_pages.refreshActive();
}

PageDocument SettingsUi::buildDefaultsChoiceDocument() const
{
    using SettingsPageBuild::cell;
    using SettingsPageBuild::initGrid;

    PageDocument doc;
    doc.id = QLatin1String(SettingsUiInternal::kLiveDefaultsChoice);
    doc.name = choiceTitle(m_defaultsChoiceId);
    const ThemeColors theme = m_settings.resolvedTheme();
    // Same center footprint as the numpad.
    initGrid(doc, 4, 3, 560, 700, 10, 20, theme);
    PageGrid& grid = doc.grids[0];
    grid.rowTracks = starTracks({0.7, 4.0, 1.0});
    const EditorSwatch sw = editorSwatch();

    PageCell title = cell(QStringLiteral("title"), choiceTitle(m_defaultsChoiceId), 0, 0, {},
                          QColor(), 4, QStringLiteral("label"));
    title.textStyle = QStringLiteral("title");
    grid.cells.push_back(std::move(title));

    QVector<Choice> choices = defaultsChoices(m_settings, m_defaultsChoiceId);
    const QColor accent = theme.accent.isValid() ? theme.accent : QColor(80, 160, 220);
    for (Choice& choice : choices) {
        if (m_defaultsDraftKind == DefaultsDraftKind::Mask) {
            const int flag = mousePickFlag(choice.token);
            choice.selected = flag != 0 && (m_defaultsDraftMask & flag) != 0;
            choice.asToggle = true;
        } else {
            choice.selected = choice.token == m_defaultsDraftToken;
        }
    }

    const int count = choices.size();
    int columns = 1;
    if (count > 6) {
        columns = 2;
    }
    if (count > 12) {
        columns = 3;
    }
    if (count > 18) {
        columns = 4;
    }
    const int rows = qMax(1, (count + columns - 1) / columns);
    PageGrid keys;
    keys.id = QStringLiteral("choices");
    keys.nested = true;
    keys.row = 1;
    keys.col = 0;
    keys.colSpan = 4;
    keys.rows = rows;
    keys.columns = columns;
    keys.gapPx = 10;
    const QColor keyBg = theme.bgAt(90);
    for (int i = 0; i < count; ++i) {
        const Choice& choice = choices.at(i);
        PageCell button = cell(QStringLiteral("def_ch_%1_%2").arg(m_defaultsChoiceId).arg(i),
                               choice.label, i / columns, i % columns,
                               QStringLiteral("settings.defaults.value.%1.%2")
                                   .arg(m_defaultsChoiceId, choice.token),
                               keyBg);
        button.style.background.token = QStringLiteral("bg90");
        if (choice.asToggle) {
            button.role = QStringLiteral("toggle");
            button.activeState =
                QStringLiteral("settings.defaults.draft.%1.%2").arg(m_defaultsChoiceId, choice.token);
        } else if (choice.selected) {
            button.role = QStringLiteral("choice");
            button.style.background = accent;
            button.style.foreground = ThemeColors::contrastOn(accent);
        } else {
            button.role = QStringLiteral("choice");
        }
        keys.cells.push_back(std::move(button));
    }
    grid.subGrids.push_back(std::move(keys));

    const QString id = m_defaultsChoiceId;
    grid.cells.push_back(cell(QStringLiteral("save"), QStringLiteral("Save"), 2, 0,
                              QStringLiteral("settings.defaults.value.%1.commit").arg(id), sw.save,
                              2));
    grid.cells.push_back(cell(QStringLiteral("cancel"), QStringLiteral("Cancel"), 2, 2,
                              QStringLiteral("settings.defaults.value.%1.cancel").arg(id), sw.cancel,
                              2));
    return doc;
}

bool SettingsUi::presentDefaultsPopup(QString* error)
{
    return presentLive(m_defaultsPopup, QLatin1String(SettingsUiInternal::kLiveDefaultsChoice),
                       buildDefaultsChoiceDocument(), error);
}

bool SettingsUi::editDefaultSetting(const QString& id)
{
    const SettingsCatalogEntry* entry = findSettingsCatalogEntry(id);
    if (!entry) {
        notifyStatus(QStringLiteral("Unknown setting"));
        return false;
    }
    if (m_defaultsChoiceId == id) {
        closeDefaultsEditor();
        m_pages.refreshDecorated();
        m_pages.refreshActive();
        return true;
    }
    closeDefaultsEditor();
    m_filterOpen = false;

    QString err;
    auto finishEditor = [&](bool ok) {
        if (!ok) {
            if (!err.isEmpty()) {
                notifyStatus(err);
            }
            m_pages.refreshDecorated();
            m_pages.refreshActive();
        }
        return ok;
    };

    switch (defaultsEditorKind(m_settings, id)) {
    case DefaultsEditorKind::Sequence:
        return finishEditor(openArrayEditor(id, &err));
    case DefaultsEditorKind::Number:
        return finishEditor(openNumericEditor(id, &err));
    case DefaultsEditorKind::Color:
        return finishEditor(openColorPicker(id, &err));
    case DefaultsEditorKind::LookTo: {
        LookToDest dest = LookToDest::Scroll;
        if (id == QLatin1String("lookToMouse")) {
            dest = LookToDest::Mouse;
        } else if (id == QLatin1String("lookToLeftStick")) {
            dest = LookToDest::LeftStick;
        } else if (id == QLatin1String("lookToRightStick")) {
            dest = LookToDest::RightStick;
        }
        return finishEditor(openLookToEditor(dest, &err));
    }
    case DefaultsEditorKind::Bool:
        m_defaultsChoiceId = id;
        m_defaultsDraftKind = DefaultsDraftKind::Bool;
        m_defaultsDraftOn = settingsBoolValue(m_settings, id).value_or(false);
        m_pages.refreshDecorated();
        m_pages.refreshActive();
        return true;
    case DefaultsEditorKind::Choice: {
        const QVector<Choice> choices = defaultsChoices(m_settings, id);
        if (choices.isEmpty()) {
            notifyStatus(QStringLiteral("This setting has no editor"));
            m_pages.refreshDecorated();
            m_pages.refreshActive();
            return false;
        }
        m_defaultsChoiceId = id;
        if (id == QLatin1String("mousePickStyle")) {
            m_defaultsDraftKind = DefaultsDraftKind::Mask;
            m_defaultsDraftMask = m_settings.mousePickStyle & PickStyle::kMousePickMask;
        } else {
            m_defaultsDraftKind = DefaultsDraftKind::Token;
            for (const Choice& choice : choices) {
                if (choice.selected) {
                    m_defaultsDraftToken = choice.token;
                    break;
                }
            }
        }
        if (!presentDefaultsPopup(&err)) {
            closeDefaultsEditor();
            return finishEditor(false);
        }
        m_pages.refreshDecorated();
        m_pages.refreshActive();
        return true;
    }
    case DefaultsEditorKind::Page:
        break;
    }
    const QString fragment = defaultsBoardFor(*entry);
    if (fragment.isEmpty()) {
        notifyStatus(QStringLiteral("This setting has no editor"));
        return false;
    }
    return hostSettingsFragment(fragment);
}

bool SettingsUi::applyDefaultsChoice(const QString& id, const QString& token)
{
    if (token == QLatin1String("cancel")) {
        if (!m_defaultsChoiceId.isEmpty() && id != m_defaultsChoiceId) {
            return false;
        }
        closeDefaultsEditor();
        m_pages.refreshDecorated();
        m_pages.refreshActive();
        return true;
    }
    if (token == QLatin1String("commit")) {
        if (id != m_defaultsChoiceId) {
            return false;
        }
        return commitDefaultsDraft();
    }
    if (id != m_defaultsChoiceId) {
        return false;
    }
    if (m_defaultsDraftKind == DefaultsDraftKind::Bool) {
        if (token != QLatin1String("flip")) {
            return false;
        }
        m_defaultsDraftOn = !m_defaultsDraftOn;
        m_pages.refreshDecorated();
        m_pages.refreshActive();
        return true;
    }
    if (m_defaultsDraftKind == DefaultsDraftKind::Mask) {
        const int flag = mousePickFlag(token);
        if (flag == 0) {
            return false;
        }
        m_defaultsDraftMask = (m_defaultsDraftMask ^ flag) & PickStyle::kMousePickMask;
        QString err;
        if (!presentDefaultsPopup(&err)) {
            if (!err.isEmpty()) {
                notifyStatus(err);
            }
            return false;
        }
        return true;
    }
    if (m_defaultsDraftKind != DefaultsDraftKind::Token) {
        return false;
    }
    bool known = false;
    for (const Choice& choice : defaultsChoices(m_settings, id)) {
        if (choice.token == token) {
            known = true;
            break;
        }
    }
    if (!known) {
        return false;
    }
    m_defaultsDraftToken = token;
    QString err;
    if (!presentDefaultsPopup(&err)) {
        if (!err.isEmpty()) {
            notifyStatus(err);
        }
        return false;
    }
    return true;
}

bool SettingsUi::commitDefaultsDraft()
{
    const QString id = m_defaultsChoiceId;
    if (id.isEmpty()) {
        return false;
    }
    if (m_defaultsDraftKind == DefaultsDraftKind::Bool) {
        const bool next = m_defaultsDraftOn;
        writeDefault([id, next](AppSettings& settings) { (void)assignSettingsBool(settings, id, next); },
                     QStringLiteral("%1: %2")
                         .arg(choiceTitle(id), next ? QStringLiteral("ON") : QStringLiteral("OFF")));
        return true;
    }
    if (m_defaultsDraftKind == DefaultsDraftKind::Mask) {
        const int mask = m_defaultsDraftMask & PickStyle::kMousePickMask;
        QStringList on;
        if (mask & PickStyle::Cursor) {
            on.push_back(QStringLiteral("cursor"));
        }
        if (mask & PickStyle::Dot) {
            on.push_back(QStringLiteral("dot"));
        }
        if (mask & PickStyle::Crosshair) {
            on.push_back(QStringLiteral("crosshair"));
        }
        const QString status = on.isEmpty()
                                   ? QStringLiteral("Mouse pick: none")
                                   : QStringLiteral("Mouse pick: %1").arg(on.join(QLatin1Char('/')));
        writeDefault([mask](AppSettings& settings) { settings.mousePickStyle = mask; }, status);
        return true;
    }
    if (m_defaultsDraftKind != DefaultsDraftKind::Token || m_defaultsDraftToken.isEmpty()) {
        notifyStatus(QStringLiteral("Pick a value"));
        return false;
    }
    const QString token = m_defaultsDraftToken;
    // Failure leaves the pad open. `writeDefault` would close it first.
    if (id == QLatin1String("headPoseOrigin") && token == QLatin1String("set")) {
        QString err;
        if (!headPoseRecenter(&err)) {
            return false;
        }
        closeDefaultsEditor();
        m_pages.refreshDecorated();
        m_pages.refreshActive();
        return true;
    }
    if (id == QLatin1String("speechOutput")) {
        QString err;
        const QString encoded = token == QLatin1String("default") ? QString() : token;
        if (!selectSpeechOutput(encoded, &err)) {
            if (!err.isEmpty()) {
                notifyStatus(err);
            }
            return false;
        }
        closeDefaultsEditor();
        m_pages.refreshDecorated();
        m_pages.refreshActive();
        return true;
    }
    for (const Choice& choice : defaultsChoices(m_settings, id)) {
        if (choice.token == token && choice.apply) {
            writeDefault(choice.apply, choice.status);
            return true;
        }
    }
    return false;
}

bool SettingsUi::hostSettingsFragment(const QString& fragment)
{
    closeDefaultsEditor();
    m_filterOpen = false;
    PageAction action;
    action.type = PageActionType::HostPage;
    action.targetId = fragment;
    QString error;
    if (!m_pages.applyNav(action, QStringLiteral("main_settings_defaults"), {}, &error)) {
        if (!error.isEmpty()) {
            notifyStatus(error);
        }
        return false;
    }
    return true;
}

} // namespace gazer
