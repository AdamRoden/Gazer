#pragma once

#include "assist/ComboMouseHit.h"
#include "assist/GazeFollowProfile.h"
#include "assist/LtsIndicator.h"
#include "layout/ProgressStyle.h"
#include "ui/Theme.h"
#include "ui/ThemeScheme.h"

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVector>

namespace gazer {

/// User preferences persisted to JSON under AppData.
struct AppSettings {
    // --- Timing ---
    /// Designer dwell steps (ms). Last step repeats while gaze holds.
    /// Default for settings, navigation, and other non-input cells.
    QVector<int> dwellSequence = defaultDwellSequence();
    /// Daily-driver dwell steps. Default for Send, mouse, composer typing, AHK, modifiers.
    QVector<int> dailyDwellSequence = defaultDailyDwellSequence();
    /// Time on-target before designer dwell progress / sequence begins (ms).
    int scanGraceMs = 150;
    /// Time on-target before daily-driver dwell begins (ms).
    int dailyScanGraceMs = 100;
    int dwellGraceMs = 200;
    int mouseMoveDwellMs = 800;
    /// Dwell for the first mag-pick step (choose region to magnify).
    int magPickDwellMs = 800;
    /// Last saved Custom timing package (Speed presets).
    struct TimingPack {
        QVector<int> sequence;
        QVector<int> dailySequence;
        int pointerDwellMs = 800;
        int zoomDwellMs = 800;
        int blinkGraceMs = 200;
        int scanGraceMs = 150;
        int dailyScanGraceMs = 100;
    };
    TimingPack customTiming = defaultTimingPack();
    /// Cancel armed mouse-move / click-loop if no target is selected within this many ms.
    /// 0 = disabled.
    int mouseMoveSelectTimeoutMs = 5000;
    /// Mouse dwell-move uses static magnify + second dwell to refine point.
    bool mouseMoveMagPick = false;
    /// Place static magnifier centered on the first-dwell point (else screen center).
    bool mouseMoveMagPickCenterOnDwell = true;
    /// Grow the static zoom window to fill the monitor's short axis.
    bool mouseMoveMagPickFullScreen = false;
    /// Inscribe the static zoom window in a circle (else a square).
    bool pickWindowRound = false;
    /// Remember a desktop dwell and immediately magnify that point when Move-to arms.
    bool mouseMoveForesight = false;
    /// Hold gaze this long (ms) to store a foresight point.
    int mouseMoveForesightDwellMs = 400;
    /// How long a stored foresight point stays valid (ms).
    int mouseMoveForesightHoldMs = 2000;
    /// When foresight and pre-click zoom both apply: a second zoom inside the
    /// foresight region. Off = place the cursor if the pick is inside foresight.
    bool mouseMoveForesightSecondZoom = false;
    /// Gaze follow for the live lens, gaze→mouse, reticle, dwell-move, mag-pick.
    GazeFollowProfile magFollowProfile = GazeFollowProfile::Sticky;

    // --- Progress visuals (boards + mouse-move) ---
    ProgressStyle progress;
    ProgressStyle mouseProgress = ProgressStyle::pointerDefaults();
    QString progressColor = QStringLiteral("#00DCFF");
    QString progressFillColor = QStringLiteral("#00B4DC46");
    QString progressBorderColor = QStringLiteral("#00DCFF");
    /// PickStyle flags: first dwell (region) and final click/move dwell.
    int magPickStyle = 1;   // Cursor
    int mousePickStyle = 1; // Cursor
    /// When true, flash uses the item foreground at flashForegroundOpacity.
    bool flashUseForeground = true;
    /// Opacity percent (0–100) when flashUseForeground is on.
    int flashForegroundOpacity = 60;
    /// Custom flash fill/border when flashUseForeground is off.
    QString flashColor = QStringLiteral("#FFFFFF");
    int flashMs = 140;

    // --- Live lens (assistant magnifier; not used by pre-click / foresight) ---
    double magZoom = 2.0;
    int magLensSize = 440;
    /// Static pick zoom shared by pre-click and foresight (not the live lens).
    double pickZoom = 4.0;
    int pickWindowPx = 880;

    // --- Look-to-Scroll ---
    int ltsDeadzonePx = 110;
    int ltsFalloffPx = 360;
    double ltsMaxNotchesPerSec = 5.0;
    /// Scroll rate grows by this factor each second gaze stays outside deadzone.
    double ltsAccelPerSec = 0.45;
    int ltsCenterDwellMs = 650;
    LtsIndicator ltsIndicatorStyle = LtsIndicator::Fan;

    // --- ComboMouse (inner drift annulus + outer command annulus) ---
    /// Inner radius of the drift ring (px). Hole / deadzone.
    int comboInnerRadiusPx = ComboMouseHit::kDefaultInnerRadiusPx;
    /// Shared radius where the drift ring meets the command pie (px).
    int comboSharedRadiusPx = ComboMouseHit::kDefaultSharedRadiusPx;
    /// Outer radius of the command pie (px).
    int comboOuterRadiusPx = ComboMouseHit::kDefaultOuterRadiusPx;
    /// Drift-ring fill (#AARRGGBB).
    QString comboInnerColor = ComboMouseHit::kDefaultInnerFill.name(QColor::HexArgb).toUpper();
    /// Command-slice fill (#AARRGGBB).
    QString comboOuterColor = ComboMouseHit::kDefaultOuterFill.name(QColor::HexArgb).toUpper();

    // --- Session ---
    bool autoCollapseMain = true;
    bool startDocked = false;
    /// Secondaries: no dwell for idleMs → instant 50% for fadeMs → 500ms dismiss shrink → close.
    /// Master shells exempt unless the layout opts in.
    bool layoutAutoClose = true;
    int layoutAutoCloseIdleMs = 10000;
    int layoutAutoCloseFadeMs = 3000;
    int trackerPref = 0;

    // --- Speech ---
    bool speakAlsoType = true;
    QString speechModel = QStringLiteral("sapi");
    QString elevenVoiceId;
    QString sapiVoiceToken;
    double speechSpeed = 1.0;
    double speechPitch = 1.0;
    QString speechLangFilter;
    QStringList elevenFavoriteVoiceIds;
    /// Runtime mirror of SpeechSecrets::hasKey(); not persisted.
    bool elevenApiKeySet = false;
    struct SavedSpeechTag {
        QString name;
        QString color;
        QString icon;
        [[nodiscard]] bool operator==(const SavedSpeechTag& o) const
        {
            return name == o.name && color == o.color && icon == o.icon;
        }
    };
    QVector<SavedSpeechTag> savedSpeechTags = defaultSavedSpeechTags();
    /// Freestyle rail: named voice + speed presets (max 8).
    struct SavedSpeechVoice {
        QString id;
        QString name;
        QString model;
        QString voiceId;
        double speed = 1.0;
        QString color;
        QString icon;
    };
    static constexpr int kMaxSavedSpeechVoices = 8;
    QVector<SavedSpeechVoice> savedSpeechVoices;

    [[nodiscard]] static QVector<SavedSpeechTag> defaultSavedSpeechTags()
    {
        QVector<SavedSpeechTag> out;
        for (const QString& n : {QStringLiteral("laugh"), QStringLiteral("cry"),
                                 QStringLiteral("burp"), QStringLiteral("loud"),
                                 QStringLiteral("soft"), QStringLiteral("sing"),
                                 QStringLiteral("english accent"), QStringLiteral("irish accent"),
                                 QStringLiteral("pirate accent")}) {
            SavedSpeechTag t;
            t.name = n;
            out.push_back(t);
        }
        return out;
    }

    // --- Theme (appearance × Apple system accent × progress × saturation) ---
    ThemeAppearance themeAppearance = ThemeAppearance::Dark;
    bool themeCustom = false;
    int themePrimaryIndex = kThemeDefaultBrandIndex;
    int themeSecondaryIndex = kThemeDefaultBrandIndex;
    int themeSaturation = kThemeSaturationDefault;
    /// Custom seeds. Branded schemes ignore these until Custom is selected.
    QString customBgColor = QStringLiteral("#1C1C1C");
    QString customPrimaryColor = QStringLiteral("#60CDFF");
    QString customSecondaryColor = QStringLiteral("#60CDFF");
    QString customTertiaryColor = QStringLiteral("#005FB8");
    QString customSurfaceColor = QStringLiteral("#262626");
    QString customTextColor = QStringLiteral("#FFFFFF");
    QString customDangerColor = QStringLiteral("#FF99A4");
    /// Unused; kept so older settings files still load.
    int themeBrightness = 4;

    void setThemeAppearance(ThemeAppearance appearance);
    void setThemeCustom(bool on);
    void setThemePrimaryIndex(int index);
    void setThemeSecondaryIndex(int index);
    void setThemeSaturation(int saturation);
    /// Rebuild derived progress colors from the live spec.
    void applyTheme();
    /// Rebuild progress (and optionally seeds) from Fluent. Does not set themeCustom.
    void applyCustomPalette(bool overlayRoles = true);
    [[nodiscard]] QColor suggestedThemeColor(const QString& key) const;
    [[nodiscard]] ThemeSeeds themeSeeds() const;
    [[nodiscard]] ThemePalette resolvedPalette() const;
    [[nodiscard]] ThemeColors resolvedTheme() const;
    [[nodiscard]] static QString themeRoleForColorKey(const QString& key);
    [[nodiscard]] static ThemeColorRole themeColorRoleForKey(const QString& key);
    [[nodiscard]] static bool isThemeSeedKey(const QString& key);

    [[nodiscard]] static QVector<int> defaultDwellSequence()
    {
        return {800, 700, 600, 500, 400, 200};
    }
    [[nodiscard]] static QVector<int> defaultDailyDwellSequence()
    {
        return {400, 600, 400, 200, 100, 50};
    }
    [[nodiscard]] static TimingPack defaultTimingPack()
    {
        return {defaultDwellSequence(), defaultDailyDwellSequence(), 800, 800, 200, 150, 100};
    }
    [[nodiscard]] static AppSettings defaults();
    [[nodiscard]] static QString defaultFilePath();
    [[nodiscard]] static QString normalizeSpeechTag(QString raw);

    /// JSON keys stay `progressRadial` / `mouseProgressPie` / … for compatibility.
    struct StyleToggle {
        const char* jsonKey;
        const char* command;
        const char* label;
        ProgressStyle AppSettings::* group;
        bool ProgressStyle::* flag;
    };
    static constexpr StyleToggle kStyleToggles[] = {
        {"progressRadial", "settings.progress.radial.toggle", "Radial", &AppSettings::progress,
         &ProgressStyle::radial},
        {"progressPie", "settings.progress.pie.toggle", "Pie", &AppSettings::progress,
         &ProgressStyle::pie},
        {"progressFill", "settings.progress.fill.toggle", "Fill", &AppSettings::progress,
         &ProgressStyle::fillBackground},
        {"progressBorder", "settings.progress.border.toggle", "Border", &AppSettings::progress,
         &ProgressStyle::border},
        {"mouseProgressRadial", "settings.mouseProgress.radial.toggle", "Mouse radial",
         &AppSettings::mouseProgress, &ProgressStyle::radial},
        {"mouseProgressPie", "settings.mouseProgress.pie.toggle", "Mouse pie",
         &AppSettings::mouseProgress, &ProgressStyle::pie},
        {"mouseProgressFill", "settings.mouseProgress.fill.toggle", "Mouse fill",
         &AppSettings::mouseProgress, &ProgressStyle::fillBackground},
        {"mouseProgressBorder", "settings.mouseProgress.border.toggle", "Mouse border",
         &AppSettings::mouseProgress, &ProgressStyle::border},
    };
    [[nodiscard]] bool& styleFlag(const StyleToggle& t) { return (this->*t.group).*t.flag; }
    [[nodiscard]] const bool& styleFlag(const StyleToggle& t) const
    {
        return (this->*t.group).*t.flag;
    }
    [[nodiscard]] static const StyleToggle* findStyleToggle(const QString& jsonKey);

    [[nodiscard]] bool loadFromFile(const QString& path, QString* error = nullptr);
    [[nodiscard]] bool saveToFile(const QString& path, QString* error = nullptr) const;

    void setDwellPreset(int preset);
    /// 0 Slow, 1 Normal, 2 Fast, 3 Custom (current timings match none of the packages).
    [[nodiscard]] int dwellPreset() const;
    void saveDwellCustom();
    void applyDwellCustom();
    /// Fill daily-driver timings from the matching designer pack when JSON omitted them.
    void inferMissingDailyDwell();
    void setMagFollowProfile(int profile);
    void setLtsIndicatorStyle(int style);
    void clamp();
    /// Nudge a numeric field by one step. Returns false if key is not numeric.
    bool nudge(const QString& key, int dir);

    [[nodiscard]] QString displayValue(const QString& key) const;
    [[nodiscard]] static QString settingTitle(const QString& key);
    [[nodiscard]] static QString settingDescription(const QString& key);
    [[nodiscard]] static bool isSequenceKey(const QString& key);
    [[nodiscard]] static bool isNumericKey(const QString& key);
    [[nodiscard]] static QStringList numericKeys();
    [[nodiscard]] static bool isColorKey(const QString& key);
    [[nodiscard]] bool applyNumericBuffer(const QString& key, const QString& buffer,
                                          QString* error = nullptr);
    [[nodiscard]] QString numericBufferSeed(const QString& key) const;
    [[nodiscard]] bool setColorKey(const QString& key, const QColor& c,
                                   bool rebuildPalette = true);
    [[nodiscard]] QColor colorKey(const QString& key) const;

    [[nodiscard]] QString dwellSequenceString() const;
    [[nodiscard]] QString dailyDwellSequenceString() const;
    [[nodiscard]] static QVector<int> parseDwellSequence(const QString& text, QString* error = nullptr);
    [[nodiscard]] static QColor parseColor(const QString& hex, const QColor& fallback = Qt::cyan);
    [[nodiscard]] static QString colorToHex(const QColor& c);
};

} // namespace gazer
