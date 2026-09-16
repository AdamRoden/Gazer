#pragma once

#include "assist/ComboMouseHit.h"
#include "assist/GazeFollowProfile.h"
#include "assist/LtsIndicator.h"
#include "assist/LtsScrollMode.h"
#include "assist/LtsSpeed.h"
#include "core/HeadPose.h"
#include "layout/ProgressStyle.h"
#include "mapping/HeadPoseTypes.h"
#include "ui/Theme.h"
#include "ui/ThemeScheme.h"

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVector>

namespace gazer {

/// User preferences persisted to JSON under AppData.
/// In-class initializers are the factory. defaults() calls applyTheme().
struct AppSettings {
    // --- Timing ---
    /// Standard dwell steps (ms). Last step repeats while gaze holds.
    /// Default for settings, navigation, mouse, AHK, composer word chips, and other non-key cells.
    QVector<int> dwellSequence = defaultDwellSequence();
    /// Rapid dwell steps. Default for Send, composer typing, modifiers, and mapping keys.
    QVector<int> rapidDwellSequence = defaultRapidDwellSequence();
    /// Time on-target before dwell progress / sequence begins (ms).
    int scanGraceMs = 100;
    int dwellGraceMs = 200;
    int mouseMoveDwellMs = 800;
    /// Dwell for the first mag-pick step (choose region to magnify).
    int magPickDwellMs = 600;
    /// Last saved Custom timing package (Speed presets). Scan grace is not part of a pack.
    struct TimingPack {
        QVector<int> sequence;
        QVector<int> rapidSequence;
        int mouseMoveDwellMs = 800;
        int magPickDwellMs = 600;
        int blinkGraceMs = 200;
    };
    TimingPack customTiming = defaultTimingPack();
    /// Cancel armed mouse-move / click-loop if no target is selected within this many ms.
    /// 0 = disabled.
    int mouseMoveSelectTimeoutMs = 1500;
    /// Mouse dwell-move uses static magnify + second dwell to refine point.
    bool mouseMoveMagPick = true;
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
    /// Gaze follow for the live lens, gaze→mouse, and reticle (not mag-pick or dwell-move).
    GazeFollowProfile magFollowProfile = GazeFollowProfile::Sticky;

    // --- Progress visuals (boards + mouse-move) ---
    ProgressStyle progress;
    ProgressStyle mouseProgress;
    QString progressColor = QStringLiteral("#99FF473D");
    QString progressFillColor = QStringLiteral("#99FF473D");
    /// Hover outline while gazing at a cell. Custom off uses progressColor.
    QString hoverColor = QStringLiteral("#99FF473D");
    int hoverBorderWeight = 4;
    bool hoverCustom = false;
    /// PickStyle flags: first dwell (region) and final click/move dwell.
    int magPickStyle = 1;   // Cursor
    int mousePickStyle = 1; // Cursor
    /// Custom flash color. Off uses the item foreground at flashForegroundOpacity.
    bool flashCustom = false;
    /// Opacity percent (0–100) when flashCustom is off.
    int flashForegroundOpacity = 60;
    QString flashColor = QStringLiteral("#FFFFFF");
    int flashMs = 60;

    // --- Live lens (assistant magnifier; not used by pre-click / foresight) ---
    double magZoom = 2.0;
    int magLensSize = 400;
    /// Static pick zoom shared by pre-click and foresight (not the live lens).
    double pickZoom = 4.0;
    int pickWindowPx = 600;

    // --- Look-to-Scroll ---
    int ltsDeadzonePx = 80;
    int ltsFalloffPx = 300;
    double ltsMaxNotchesPerSec = kLtsSpeedDefault;
    /// Scroll rate grows by this factor each second gaze stays outside deadzone.
    double ltsAccelPerSec = kLtsAccelDefault;
    int ltsCenterDwellMs = 700;
    LtsIndicator ltsIndicatorStyle = LtsIndicator::Filled;
    /// Gaze-scroll axes: vertical, horizontal, or both.
    LtsScrollMode ltsScrollMode = LtsScrollMode::Both;

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
    /// Full-screen dock tour when Gazer starts.
    bool showSplash = true;
    /// Secondaries: no dwell for idleMs → instant 50% for fadeMs → 500ms dismiss shrink → close.
    /// Master shells exempt unless the layout opts in.
    bool layoutAutoClose = true;
    int layoutAutoCloseIdleMs = 10000;
    int layoutAutoCloseFadeMs = 3000;
    int trackerPref = 0;

    // --- Head pose analog maps ---
    bool headPoseEnabled = false;
    bool headPoseOriginSet = false;
    HeadPose headPoseOrigin;
    QVector<HeadPoseMap> headPoseMaps;
    static constexpr int kMaxHeadPoseMaps = gazer::kMaxHeadPoseMaps;
    [[nodiscard]] static HeadPoseMap makeDefaultHeadPoseMap();

    // --- Speech ---
    QString speechModel = QStringLiteral("sapi");
    QString elevenVoiceId;
    QString sapiVoiceToken;
    double speechSpeed = 1.0;
    double speechPitch = 1.0;
    /// Playback gain for composer clips (1–5×). SAPI cannot go above 100%.
    double speechVolume = 1.0;
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
        double volume = 1.0;
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

    // --- Theme (appearance × brightness × tint family × accent × progress) ---
    ThemeAppearance themeAppearance = ThemeAppearance::Dark;
    int themeSaturation = kThemeSaturationDefault;
    ThemeTintFamily themeTintFamily = ThemeTintFamily::None;
    /// Accent / progress seeds (`customPrimaryColor` / `customSecondaryColor`).
    /// `customSourceColor` is loaded/saved for old JSON; Fluent ignores it.
    QString customSourceColor = QStringLiteral("#1E97F3");
    QString customPrimaryColor = QStringLiteral("#1E97F3");
    QString customSecondaryColor = QStringLiteral("#99FF473D");
    QString customTextColor = QStringLiteral("#FFFFFF");
    QString customDangerColor = QStringLiteral("#FC1C1C");
    int themeBrightness = kThemeBrightnessDefault;

    void setThemeAppearance(ThemeAppearance appearance);
    void setThemeDark(bool dark);
    void setThemeTintFamily(ThemeTintFamily family);
    void setThemeBrightness(int brightness);
    void setThemeSaturation(int saturation);
    [[nodiscard]] QColor surfaceTintColor() const;
    /// Rebuild derived progress colors from the live spec.
    void applyTheme();
    [[nodiscard]] ThemeSeeds themeSeeds() const;
    [[nodiscard]] ThemePalette resolvedPalette() const;
    [[nodiscard]] ThemeColors resolvedTheme() const;
    [[nodiscard]] QColor resolvedHoverBorder() const;
    [[nodiscard]] static QString themeRoleForColorKey(const QString& key);

    [[nodiscard]] static QVector<int> defaultDwellSequence()
    {
        return {800, 700, 600, 500, 400, 200};
    }
    [[nodiscard]] static QVector<int> defaultRapidDwellSequence()
    {
        return {400, 600, 400, 300, 200, 100};
    }
    [[nodiscard]] static TimingPack defaultTimingPack()
    {
        return {defaultDwellSequence(), defaultRapidDwellSequence(), 800, 600, 200};
    }
    /// In-class factory plus Fluent neutrals and applyTheme(). JSON overlays this.
    [[nodiscard]] static AppSettings defaults();
    [[nodiscard]] static QString defaultFilePath();
    [[nodiscard]] static QString normalizeSpeechTag(QString raw);

    /// JSON keys: `progressRadial` / `mouseProgressPie` / …
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
        {"mouseProgressRadial", "settings.mouseProgress.radial.toggle", "Mouse radial",
         &AppSettings::mouseProgress, &ProgressStyle::radial},
        {"mouseProgressPie", "settings.mouseProgress.pie.toggle", "Mouse pie",
         &AppSettings::mouseProgress, &ProgressStyle::pie},
        {"mouseProgressFill", "settings.mouseProgress.fill.toggle", "Mouse fill",
         &AppSettings::mouseProgress, &ProgressStyle::fillBackground},
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
    void setMagFollowProfile(int profile);
    void setLtsIndicatorStyle(int style);
    void clamp();
    /// Nudge a numeric field by one step. Returns false if key is not numeric.
    bool nudge(const QString& key, int dir);

    [[nodiscard]] QString displayValue(const QString& key) const;
    [[nodiscard]] static QString settingTitle(const QString& key);
    [[nodiscard]] static QString settingDescription(const QString& key);
    [[nodiscard]] static bool isSequenceKey(const QString& key);
    [[nodiscard]] static bool isRapidSequenceKey(const QString& key);
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
    [[nodiscard]] QString rapidDwellSequenceString() const;
    [[nodiscard]] static QVector<int> parseDwellSequence(const QString& text, QString* error = nullptr);
    [[nodiscard]] static QColor parseColor(const QString& hex, const QColor& fallback = Qt::cyan);
    [[nodiscard]] static QString colorToHex(const QColor& c);
};

} // namespace gazer
