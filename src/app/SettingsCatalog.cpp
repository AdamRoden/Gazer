#include "app/SettingsCatalog.h"

#include "app/SettingsUi.h"
#include "assist/LtsScrollMode.h"
#include "layout/PageBox.h"
#include "ui/Theme.h"

#include <QJsonValue>

namespace gazer {

namespace {

constexpr SettingsCatalogEntry kEntries[] = {
    {"dwellSequence", "Dwell", "Standard dwell",
     "Dwell times for settings, navigation, mouse, and AHK. The last step repeats.",
     "dwellSequence"},
    {"rapidDwellSequence", "Dwell", "Rapid dwell",
     "Dwell times for keys, composer typing, and modifiers. The last step repeats.",
     "rapidDwellSequence"},
    {"keyGravitySize", "Dwell", "Dynamic size",
     "How far a likely next character can claim the edge of a key. 0 is off.",
     "keyGravitySize"},
    {"keyGravityDuration", "Dwell", "Dynamic duration",
     "How much a likely next character shortens rapid dwell. 0 is off.", "keyGravityDuration"},
    {"scanGraceMs", "Dwell", "Scan grace",
     "Time on a cell before dwell progress starts.", "scanGraceMs"},
    {"dwellGraceMs", "Dwell", "Blink grace",
     "How long gaze can leave a cell before dwell resets.", "dwellGraceMs"},
    {"progressRadial", "Dwell", "Radial progress",
     "Draw the dwell ring on buttons.", "progressRadial"},
    {"progressPie", "Dwell", "Pie progress", "Draw the dwell pie on buttons.", "progressPie"},
    {"progressFill", "Dwell", "Fill progress", "Fill buttons as dwell advances.", "progressFill"},
    {"customTiming", "Dwell", "Custom timing",
     "The saved Custom package: both dwell sequences, blink grace, pointer dwell, and zoom dwell.",
     "customDwellSequence,customRapidDwellSequence,customDwellGraceMs,customMouseMoveDwellMs,"
     "customMagPickDwellMs"},

    {"mouseMoveDwellMs", "Pointer", "Pointer dwell",
     "Dwell time for mouse move and the click loop.", "mouseMoveDwellMs"},
    {"mouseMoveSelectTimeoutMs", "Pointer", "Pick timeout",
     "Cancel an armed move when no target is chosen in this time. 0 stays armed.",
     "mouseMoveSelectTimeoutMs"},
    {"mousePickStyle", "Pointer", "Move marker",
     "Cursor, dot, and crosshair drawn for a move-to pick.", "mousePickStyle"},
    {"mouseProgressRadial", "Pointer", "Mouse radial",
     "Draw the dwell ring on the pointer.", "mouseProgressRadial"},
    {"mouseProgressPie", "Pointer", "Mouse pie", "Draw the dwell pie on the pointer.",
     "mouseProgressPie"},
    {"mouseProgressFill", "Pointer", "Mouse fill", "Fill the pointer as dwell advances.",
     "mouseProgressFill"},
    {"pickZoom", "Pointer", "Zoom amount", "Magnification of the static pick window.", "pickZoom"},
    {"pickWindowPx", "Pointer", "Zoom size", "Size of the static pick window.", "pickWindowPx"},
    {"pickWindowRound", "Pointer", "Round zoom window",
     "Use a circle for the static pick window.", "pickWindowRound"},
    {"mouseMoveMagPick", "Pointer", "Magnify pick",
     "A second dwell on a magnified view refines the click.", "mouseMoveMagPick"},
    {"mouseMoveMagPickCenterOnDwell", "Pointer", "Zoom at gaze",
     "Place the pick window on the first dwell point.", "mouseMoveMagPickCenterOnDwell"},
    {"mouseMoveMagPickFullScreen", "Pointer", "Full-screen zoom",
     "Grow the pick window to the short side of the monitor.", "mouseMoveMagPickFullScreen"},

    {"magFollowProfile", "Gaze", "Gaze follow",
     "How the indicator, gaze mouse, and live lens follow the eyes.", "magFollowProfile"},
    {"magZoom", "Gaze", "Lens zoom", "Magnification of the live lens.", "magZoom"},
    {"magLensSize", "Gaze", "Lens size", "Size of the live lens.", "magLensSize"},
    {"lookToScroll", "Gaze", "Look to scroll",
     "Gaze disk that scrolls. Hub, deadzone, speed, and axis.", "lookToMaps.scroll"},
    {"lookToMouse", "Gaze", "Look to mouse",
     "Gaze disk that moves the pointer.", "lookToMaps.mouse"},
    {"lookToLeftStick", "Gaze", "Look to left stick",
     "Gaze disk that drives the left gamepad stick.", "lookToMaps.leftStick"},
    {"lookToRightStick", "Gaze", "Look to right stick",
     "Gaze disk that drives the right gamepad stick.", "lookToMaps.rightStick"},
    {"comboInnerRadiusPx", "Gaze", "Combo inner radius",
     "Hole in the middle of ComboMouse.", "comboInnerRadiusPx"},
    {"comboSharedRadiusPx", "Gaze", "Combo shared radius",
     "Where the drift ring meets the command pie.", "comboSharedRadiusPx"},
    {"comboOuterRadiusPx", "Gaze", "Combo outer radius",
     "Outer edge of the ComboMouse command pie.", "comboOuterRadiusPx"},
    {"comboInnerColor", "Gaze", "Combo drift color",
     "Fill of the ComboMouse drift ring.", "comboInnerColor"},
    {"comboOuterColor", "Gaze", "Combo command color",
     "Fill of the ComboMouse command slices.", "comboOuterColor"},

    {"headPoseEnabled", "Head", "Head pose",
     "Turn head-pose maps on.", "headPoseEnabled"},
    {"headPoseMaps", "Head", "Head pose maps",
     "Axis maps from head pose to mouse, sticks, or commands.", "headPoseMaps"},
    {"headPoseOrigin", "Head", "Head pose origin",
     "The neutral pose. Reset clears the saved origin.", "headPoseOrigin,headPoseOriginSet"},

    {"speechModel", "Speak", "Speech model",
     "SAPI or an ElevenLabs model.", "speechModel"},
    {"elevenVoiceId", "Speak", "ElevenLabs voice",
     "Voice id used when the model is ElevenLabs.", "elevenVoiceId"},
    {"sapiVoiceToken", "Speak", "Windows voice",
     "SAPI voice. Empty uses the Windows default.", "sapiVoiceToken"},
    {"speechSpeed", "Speak", "Speech speed", "Speaking rate.", "speechSpeed"},
    {"speechPitch", "Speak", "Speech pitch", "Speaking pitch.", "speechPitch"},
    {"speechVolume", "Speak", "Speech boost",
     "Playback gain for composer clips.", "speechVolume"},
    {"speechOutput", "Speak", "Speech output",
     "Playback device. Empty follows the Windows default.", "speechOutputId,speechOutputName"},
    {"composePredictions", "Speak", "Word suggestions",
     "Show next-word chips on the Speak page.", "composePredictions"},
    {"speechLangFilter", "Speak", "Voice language",
     "Limit the voice list to this language. Empty shows every language.", "speechLangFilter"},
    {"elevenFavoriteVoiceIds", "Speak", "Favorite voices",
     "ElevenLabs voices pinned in the voice list.", "elevenFavoriteVoiceIds"},
    {"savedSpeechTags", "Speak", "Speech tags",
     "Saved Speak tags and their colors.", "savedSpeechTags"},
    {"speechStyle", "Speak", "Speech style",
     "Standing ElevenLabs style. Empty means none.", "speechStyle"},
    {"voiceStyles", "Speak", "Voice styles",
     "Custom style names and favorites. Reset restores the built-in catalog.", "voiceStyles"},
    {"railFavorites", "Speak", "Favorites rail",
     "Actions, styles, and tags pinned on the Speak rail.", "railFavorites"},
    {"savedSpeechVoices", "Speak", "Saved voice presets",
     "Older voice presets kept so existing files still load.", "savedSpeechVoices"},

    {"themeAppearance", "Theme", "Appearance",
     "Light or dark background, with or without a tint.", "themeAppearance"},
    {"themeSaturation", "Theme", "Saturation",
     "How strong the background tint is.", "themeSaturation"},
    {"themeTintFamily", "Theme", "Tint",
     "Hue washed onto the background shades.", "themeTintFamily"},
    {"themeBrightness", "Theme", "Brightness",
     "How light or dark the background shades are.", "themeBrightness"},
    {"customPrimaryColor", "Theme", "Accent",
     "Highlighted foreground.", "customPrimaryColor"},
    {"customSecondaryColor", "Theme", "Progress color",
     "Dwell progress color.", "customSecondaryColor"},
    {"customTextColor", "Theme", "Text color", "Custom text color.", "customTextColor"},
    {"customDangerColor", "Theme", "Danger color", "Custom danger color.", "customDangerColor"},
    {"customSourceColor", "Theme", "Source color",
     "Kept for older files. Accent and progress use their own colors.", "customSourceColor"},
    {"progressColor", "Theme", "Progress ring", "Color of the dwell ring.", "progressColor"},
    {"progressFillColor", "Theme", "Progress fill", "Color of the dwell fill.", "progressFillColor"},
    {"hoverColor", "Theme", "Hover color",
     "Outline color when custom hover is on.", "hoverColor"},
    {"hoverBorderWeight", "Theme", "Hover weight",
     "Thickness of the hover outline.", "hoverBorderWeight"},
    {"hoverCustom", "Theme", "Custom hover",
     "Use the hover color instead of the progress color.", "hoverCustom"},
    {"flashCustom", "Theme", "Custom flash",
     "Flash a chosen color instead of the item foreground.", "flashCustom"},
    {"flashForegroundOpacity", "Theme", "Flash opacity",
     "Opacity of the foreground flash when custom flash is off.", "flashForegroundOpacity"},
    {"flashColor", "Theme", "Flash color", "Color used when custom flash is on.", "flashColor"},
    {"flashMs", "Theme", "Flash time", "How long the completion flash stays.", "flashMs"},

    {"trackerPref", "Setup", "Tracker",
     "Auto Tobii or mouse only. Applies the next time Gazer starts.", "trackerPref"},
    {"screenCapture", "Setup", "Screen capture",
     "What a screenshot includes: everything, boards, or neither.", "screenCapture"},
    {"showSplash", "Setup", "Startup splash",
     "Play the dock tour when Gazer starts.", "showSplash"},
    {"layoutAutoClose", "Setup", "Auto-close boards",
     "Close idle boards that opted in.", "layoutAutoClose"},
    {"layoutAutoCloseIdleMs", "Setup", "Auto-close idle",
     "How long a board sits idle before it starts to close.", "layoutAutoCloseIdleMs"},
    {"layoutAutoCloseFadeMs", "Setup", "Auto-close fade",
     "How long the closing fade lasts.", "layoutAutoCloseFadeMs"},
    {"autoCollapseMain", "Setup", "Collapse the dock",
     "Collapse the main dock after a board opens.", "autoCollapseMain"},
    {"startDocked", "Setup", "Start docked",
     "Start with the dock collapsed.", "startDocked"},
};

QString onOff(bool value)
{
    return value ? QStringLiteral("ON") : QStringLiteral("OFF");
}

QString countText(int count, const QString& singular, const QString& plural)
{
    return QString::number(count) + QLatin1Char(' ') + (count == 1 ? singular : plural);
}

QString lookToText(const LookToMapSettings& map)
{
    return QStringLiteral("%1, hub %2")
        .arg(QLatin1String(ltsScrollModeName(map.axisMode)))
        .arg(map.hubPx);
}

bool objectHasKey(const QJsonObject& object, const QString& key)
{
    const int dot = key.indexOf(QLatin1Char('.'));
    if (dot < 0) {
        return object.contains(key);
    }
    return object.value(key.left(dot)).toObject().contains(key.mid(dot + 1));
}

} // namespace

const SettingsCatalogEntry* settingsCatalogEntries()
{
    return kEntries;
}

int settingsCatalogCount()
{
    return int(sizeof(kEntries) / sizeof(kEntries[0]));
}

const SettingsCatalogEntry* findSettingsCatalogEntry(const QString& id)
{
    for (const SettingsCatalogEntry& entry : kEntries) {
        if (id == QLatin1String(entry.id)) {
            return &entry;
        }
    }
    return nullptr;
}

QStringList settingsCatalogJsonKeys(const SettingsCatalogEntry& entry)
{
    return QString::fromLatin1(entry.jsonKeys).split(QLatin1Char(','), Qt::SkipEmptyParts);
}

QString settingValueText(const AppSettings& settings, const QString& id)
{
    if (id == QLatin1String("customTiming")) {
        return QStringLiteral("blink %1 ms, pointer %2 ms")
            .arg(settings.customTiming.blinkGraceMs)
            .arg(settings.customTiming.mouseMoveDwellMs);
    }
    if (id == QLatin1String("mouseMoveMagPick")) {
        return onOff(settings.mouseMoveMagPick);
    }
    if (id == QLatin1String("mouseMoveMagPickCenterOnDwell")) {
        return onOff(settings.mouseMoveMagPickCenterOnDwell);
    }
    if (id == QLatin1String("mouseMoveMagPickFullScreen")) {
        return onOff(settings.mouseMoveMagPickFullScreen);
    }
    if (id == QLatin1String("lookToScroll")) {
        return lookToText(settings.lookToScroll);
    }
    if (id == QLatin1String("lookToMouse")) {
        return lookToText(settings.lookToMouse);
    }
    if (id == QLatin1String("lookToLeftStick")) {
        return lookToText(settings.lookToLeftStick);
    }
    if (id == QLatin1String("lookToRightStick")) {
        return lookToText(settings.lookToRightStick);
    }
    if (id == QLatin1String("headPoseEnabled")) {
        return onOff(settings.headPoseEnabled);
    }
    if (id == QLatin1String("headPoseMaps")) {
        return countText(settings.headPoseMaps.size(), QStringLiteral("map"),
                          QStringLiteral("maps"));
    }
    if (id == QLatin1String("headPoseOrigin")) {
        return settings.headPoseOriginSet ? QStringLiteral("Set") : QStringLiteral("Not set");
    }
    if (id == QLatin1String("speechModel")) {
        return settings.speechModel.isEmpty() ? QStringLiteral("sapi") : settings.speechModel;
    }
    if (id == QLatin1String("elevenVoiceId")) {
        return settings.elevenVoiceId.isEmpty() ? QStringLiteral("None") : settings.elevenVoiceId;
    }
    if (id == QLatin1String("sapiVoiceToken")) {
        return settings.sapiVoiceToken.isEmpty() ? QStringLiteral("Default")
                                                  : QStringLiteral("Chosen");
    }
    if (id == QLatin1String("speechOutput")) {
        if (settings.speechOutputId.isEmpty()) {
            return QStringLiteral("System default");
        }
        return settings.speechOutputName.isEmpty() ? QStringLiteral("Chosen device")
                                                    : settings.speechOutputName;
    }
    if (id == QLatin1String("speechLangFilter")) {
        return settings.speechLangFilter.isEmpty() ? QStringLiteral("All")
                                                    : settings.speechLangFilter;
    }
    if (id == QLatin1String("elevenFavoriteVoiceIds")) {
        return countText(settings.elevenFavoriteVoiceIds.size(), QStringLiteral("voice"),
                          QStringLiteral("voices"));
    }
    if (id == QLatin1String("savedSpeechTags")) {
        return countText(settings.savedSpeechTags.size(), QStringLiteral("tag"),
                          QStringLiteral("tags"));
    }
    if (id == QLatin1String("speechStyle")) {
        return settings.speechStyle.isEmpty() ? QStringLiteral("None") : settings.speechStyle;
    }
    if (id == QLatin1String("voiceStyles")) {
        if (!settings.voiceStylesSet) {
            return QStringLiteral("Built-in");
        }
        return countText(settings.voiceStyleNames.size(), QStringLiteral("style"),
                          QStringLiteral("styles"));
    }
    if (id == QLatin1String("railFavorites")) {
        if (!settings.railFavoritesSet) {
            return QStringLiteral("Default rail");
        }
        return countText(settings.railFavorites.size(), QStringLiteral("pin"),
                          QStringLiteral("pins"));
    }
    if (id == QLatin1String("savedSpeechVoices")) {
        return countText(settings.savedSpeechVoices.size(), QStringLiteral("preset"),
                          QStringLiteral("presets"));
    }
    if (id == QLatin1String("themeAppearance")) {
        return themeAppearanceToString(settings.themeAppearance);
    }
    if (id == QLatin1String("themeTintFamily")) {
        return themeTintFamilyToString(settings.themeTintFamily);
    }
    if (id == QLatin1String("themeBrightness")) {
        return QString::number(settings.themeBrightness);
    }
    if (id == QLatin1String("layoutAutoCloseFadeMs")) {
        return QString::number(settings.layoutAutoCloseFadeMs) + QStringLiteral(" ms");
    }
    const QString shown = settings.displayValue(id);
    if (!shown.isEmpty()) {
        return shown;
    }
    return QStringLiteral("—");
}

DefaultsEditorKind defaultsEditorKind(const AppSettings& settings, const QString& id)
{
    if (AppSettings::isSequenceKey(id)) {
        return DefaultsEditorKind::Sequence;
    }
    if (AppSettings::isNumericKey(id)) {
        return DefaultsEditorKind::Number;
    }
    if (AppSettings::isColorKey(id)) {
        return DefaultsEditorKind::Color;
    }
    if (id == QLatin1String("lookToScroll") || id == QLatin1String("lookToMouse")
        || id == QLatin1String("lookToLeftStick") || id == QLatin1String("lookToRightStick")) {
        return DefaultsEditorKind::LookTo;
    }
    if (settingsBoolValue(settings, id).has_value()) {
        return DefaultsEditorKind::Bool;
    }
    static const char* kChoices[] = {
        "magFollowProfile", "trackerPref",     "screenCapture",  "themeAppearance",
        "themeTintFamily",  "themeBrightness", "speechModel",    "mousePickStyle",
        "customTiming",     "headPoseOrigin",  "speechOutput",
    };
    for (const char* name : kChoices) {
        if (id == QLatin1String(name)) {
            return DefaultsEditorKind::Choice;
        }
    }
    return DefaultsEditorKind::Page;
}

namespace {

struct CatalogBool {
    const char* id;
    bool AppSettings::* member;
};

const CatalogBool* catalogBool(const QString& id)
{
    static constexpr CatalogBool kFields[] = {
        {"autoCollapseMain", &AppSettings::autoCollapseMain},
        {"startDocked", &AppSettings::startDocked},
        {"showSplash", &AppSettings::showSplash},
        {"composePredictions", &AppSettings::composePredictions},
        {"layoutAutoClose", &AppSettings::layoutAutoClose},
        {"flashCustom", &AppSettings::flashCustom},
        {"hoverCustom", &AppSettings::hoverCustom},
        {"pickWindowRound", &AppSettings::pickWindowRound},
        {"headPoseEnabled", &AppSettings::headPoseEnabled},
        {"mouseMoveMagPick", &AppSettings::mouseMoveMagPick},
        {"mouseMoveMagPickCenterOnDwell", &AppSettings::mouseMoveMagPickCenterOnDwell},
        {"mouseMoveMagPickFullScreen", &AppSettings::mouseMoveMagPickFullScreen},
    };
    for (const CatalogBool& field : kFields) {
        if (id == QLatin1String(field.id)) {
            return &field;
        }
    }
    return nullptr;
}

} // namespace

std::optional<bool> settingsBoolValue(const AppSettings& settings, const QString& id)
{
    if (const AppSettings::StyleToggle* toggle = AppSettings::findStyleToggle(id)) {
        return settings.styleFlag(*toggle);
    }
    if (const CatalogBool* field = catalogBool(id)) {
        return settings.*(field->member);
    }
    return std::nullopt;
}

bool assignSettingsBool(AppSettings& settings, const QString& id, bool value)
{
    if (const AppSettings::StyleToggle* toggle = AppSettings::findStyleToggle(id)) {
        settings.styleFlag(*toggle) = value;
        return true;
    }
    if (const CatalogBool* field = catalogBool(id)) {
        settings.*(field->member) = value;
        return true;
    }
    return false;
}

namespace {

constexpr int kDefaultsHeaderPx = 48;
constexpr int kDefaultsRowPx = 96;

PageCell defaultsLabel(const QString& id, const QString& label, const QString& caption, int col)
{
    PageCell cell;
    cell.id = id;
    cell.label = label;
    cell.caption = caption;
    cell.role = QStringLiteral("label");
    cell.row = 0;
    cell.col = col;
    cell.styleId = QStringLiteral("plain");
    return cell;
}

PageAction defaultsCommand(const QString& command)
{
    PageAction action;
    action.type = PageActionType::Command;
    action.command = command;
    return action;
}

PageCell defaultsSwatch(const QString& id, const QColor& color, int col)
{
    PageCell cell;
    cell.id = id;
    cell.role = QStringLiteral("swatchrect");
    cell.row = 0;
    cell.col = col;
    cell.styleId = QStringLiteral("plain");
    if (color.isValid()) {
        cell.style.background = color;
        cell.style.foreground = ThemeColors::contrastOn(color);
    }
    return cell;
}

PageCell defaultsButton(const QString& id, const QString& label, const QString& icon,
                        const QString& command, int col)
{
    PageCell cell;
    cell.id = id;
    cell.label = label;
    cell.icon = icon;
    cell.row = 0;
    cell.col = col;
    cell.styleId = QStringLiteral("button");
    cell.actions.push_back(defaultsCommand(command));
    return cell;
}

PageGrid defaultsRow(const SettingsCatalogEntry& entry, int row, const AppSettings& factory,
                     const AppSettings& live, const QString& editingId, bool editingOn)
{
    const QString id = QLatin1String(entry.id);
    PageGrid grid;
    grid.nested = true;
    grid.id = QStringLiteral("def_row_%1").arg(id);
    grid.styleId = QStringLiteral("row");
    grid.row = row;
    grid.col = 0;
    grid.rows = 1;
    grid.columns = 5;
    grid.gapPx = 6;
    grid.columnTracks = {PageTrackSize::starWeight(5), PageTrackSize::starWeight(3),
                         PageTrackSize::starWeight(3), PageTrackSize::starWeight(2),
                         PageTrackSize::starWeight(2)};
    grid.cells.push_back(defaultsLabel(QStringLiteral("def_n_%1").arg(id),
                                       QLatin1String(entry.title),
                                       QLatin1String(entry.description), 0));
    if (AppSettings::isColorKey(id)) {
        PageCell factorySwatch = defaultsSwatch(QStringLiteral("def_d_%1").arg(id),
                                                factory.colorKey(id), 1);
        grid.cells.push_back(std::move(factorySwatch));
    } else {
        grid.cells.push_back(defaultsLabel(QStringLiteral("def_d_%1").arg(id),
                                           settingValueText(factory, id), {}, 1));
    }

    const bool editing = editingId == id && settingsBoolValue(live, id).has_value();
    if (editing) {
        const ThemeColors theme = live.resolvedTheme();
        const QColor accent = theme.accent.isValid() ? theme.accent : QColor(80, 160, 220);
        const QColor danger = theme.danger.isValid() ? theme.danger : QColor(180, 60, 60);
        PageCell toggle;
        toggle.id = QStringLiteral("def_c_%1").arg(id);
        toggle.label = editingOn ? QStringLiteral("On") : QStringLiteral("Off");
        toggle.role = QStringLiteral("toggle");
        toggle.row = 0;
        toggle.col = 2;
        toggle.styleId = QStringLiteral("button");
        toggle.style.background.token = QStringLiteral("bg95");
        toggle.style.borderColor = accent;
        toggle.style.thickness = PageBox::all(2.5);
        toggle.activeState = QStringLiteral("settings.defaults.draft.%1").arg(id);
        toggle.actions.push_back(
            defaultsCommand(QStringLiteral("settings.defaults.value.%1.flip").arg(id)));
        grid.cells.push_back(std::move(toggle));

        PageCell save = defaultsButton(QStringLiteral("def_e_%1").arg(id), QStringLiteral("Save"),
                                       {}, QStringLiteral("settings.defaults.value.%1.commit").arg(id),
                                       3);
        save.style.background = accent;
        save.style.foreground = ThemeColors::contrastOn(accent);
        grid.cells.push_back(std::move(save));

        PageCell cancel =
            defaultsButton(QStringLiteral("def_p_%1").arg(id), QStringLiteral("Cancel"), {},
                           QStringLiteral("settings.defaults.value.%1.cancel").arg(id), 4);
        cancel.role = QStringLiteral("button");
        cancel.activeState.clear();
        cancel.style.background = danger;
        cancel.style.foreground = ThemeColors::contrastOn(danger);
        grid.cells.push_back(std::move(cancel));
        return grid;
    }

    if (AppSettings::isColorKey(id)) {
        PageCell current = defaultsSwatch(QStringLiteral("def_c_%1").arg(id), live.colorKey(id), 2);
        current.settingKey = id;
        grid.cells.push_back(std::move(current));
    } else {
        grid.cells.push_back(defaultsLabel(QStringLiteral("def_c_%1").arg(id),
                                           settingValueText(live, id), {}, 2));
    }

    const bool opensPage = defaultsEditorKind(live, id) == DefaultsEditorKind::Page;
    grid.cells.push_back(defaultsButton(
        QStringLiteral("def_e_%1").arg(id), opensPage ? QString() : QStringLiteral("Edit"),
        opensPage ? QStringLiteral("ArrowRight") : QStringLiteral("edit"),
        QStringLiteral("settings.defaults.edit.%1").arg(id), 3));

    const QString pick = QStringLiteral("settings.defaults.pick.%1").arg(id);
    PageCell toggle = defaultsButton(QStringLiteral("def_p_%1").arg(id), QStringLiteral("Select"),
                                    {}, pick, 4);
    toggle.role = QStringLiteral("toggle");
    toggle.activeState = pick;
    grid.cells.push_back(std::move(toggle));
    return grid;
}

} // namespace

void fillSettingsDefaultsGrid(PageGrid& host, const AppSettings& live, const QString& filter,
                              const QString& query, const QString& editingId, bool editingOn)
{
    const SettingsCatalogEntry* entries = settingsCatalogEntries();
    const int count = settingsCatalogCount();

    // A few star rows on the scroller, one group card each. Catalog rows stay
    // inside the card at a fixed pixel height so the scroller's own gap stays
    // inside the viewport.
    host.cells.clear();
    host.subGrids.clear();
    host.columns = 1;
    host.columnTracks.clear();
    host.gapPx = 12;
    host.marginPx = 0;
    host.rowTracks.clear();

    struct Section {
        const char* page = nullptr;
        QVector<const SettingsCatalogEntry*> rows;
    };
    QVector<Section> sections;
    for (const char* page : kSettingsDefaultsPages) {
        if (QLatin1String(page) == QLatin1String("All")) {
            continue;
        }
        if (filter != QLatin1String("All") && filter != QLatin1String(page)) {
            continue;
        }
        Section section;
        section.page = page;
        for (int i = 0; i < count; ++i) {
            if (QLatin1String(entries[i].page) != QLatin1String(page)) {
                continue;
            }
            if (settingsEntryVisible(entries[i], filter, query)) {
                section.rows.push_back(&entries[i]);
            }
        }
        if (!section.rows.isEmpty()) {
            sections.push_back(std::move(section));
        }
    }

    if (sections.isEmpty()) {
        host.rows = 1;
        host.rowTracks = {PageTrackSize::starWeight()};
        PageGrid card;
        card.nested = true;
        card.id = QStringLiteral("def_sec_empty");
        card.styleId = QStringLiteral("group");
        card.row = 0;
        card.col = 0;
        card.rows = 1;
        card.columns = 1;
        card.rowTracks = {PageTrackSize::fromDim(PageDim::pixels(kDefaultsRowPx))};
        card.cells.push_back(defaultsLabel(QStringLiteral("def_empty"),
                                           QStringLiteral("Nothing matches"), {}, 0));
        host.subGrids.push_back(std::move(card));
        return;
    }

    static const AppSettings factory = AppSettings::defaults();
    host.rows = sections.size();
    for (int s = 0; s < sections.size(); ++s) {
        const Section& section = sections[s];
        host.rowTracks.push_back(PageTrackSize::starWeight());

        PageGrid card;
        card.nested = true;
        card.id = QStringLiteral("def_sec_%1").arg(QLatin1String(section.page));
        card.styleId = QStringLiteral("group");
        card.row = s;
        card.col = 0;
        card.columns = 1;
        card.gapPx = 8;
        card.marginPx = 8;
        card.rows = section.rows.size() + 1;
        card.rowTracks.push_back(PageTrackSize::fromDim(PageDim::pixels(kDefaultsHeaderPx)));
        PageCell header = defaultsLabel(QStringLiteral("def_h_%1").arg(QLatin1String(section.page)),
                                        QLatin1String(section.page), {}, 0);
        header.row = 0;
        header.textStyle = QStringLiteral("section");
        card.cells.push_back(header);

        for (int i = 0; i < section.rows.size(); ++i) {
            card.rowTracks.push_back(PageTrackSize::fromDim(PageDim::pixels(kDefaultsRowPx)));
            card.subGrids.push_back(
                defaultsRow(*section.rows[i], i + 1, factory, live, editingId, editingOn));
        }
        host.subGrids.push_back(std::move(card));
    }
}

SettingsImportKind classifySettingsImport(const QJsonObject& object)
{
    if (object.contains(QStringLiteral("schemaVersion"))) {
        const int schema = object.value(QStringLiteral("schemaVersion")).toInt(0);
        if (schema > AppSettings::kSettingsSchemaVersion) {
            return SettingsImportKind::Newer;
        }
        return SettingsImportKind::Ok;
    }
    for (const SettingsCatalogEntry& entry : kEntries) {
        for (const QString& key : settingsCatalogJsonKeys(entry)) {
            if (objectHasKey(object, key)) {
                return SettingsImportKind::Ok;
            }
        }
    }
    return SettingsImportKind::Empty;
}

} // namespace gazer
