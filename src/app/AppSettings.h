#pragma once

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
    /// Progressive dwell steps (ms). Last step repeats while gaze holds.
    QVector<int> dwellSequence = defaultDwellSequence();
    /// Time on-target before dwell progress animation / sequence begins (ms).
    int scanGraceMs = 100;
    int dwellGraceMs = 180;
    int mouseMoveDwellMs = 700;
    /// Dwell for the first mag-pick step (choose region to magnify).
    int magPickDwellMs = 700;
    /// Cancel armed mouse-move / click-loop if no target is selected within this many ms.
    /// 0 = disabled.
    int mouseMoveSelectTimeoutMs = 5000;
    /// Mouse dwell-move uses static magnify + second dwell to refine point.
    bool mouseMoveMagPick = false;
    /// Place static magnifier centered on the first-dwell point (else screen center).
    bool mouseMoveMagPickCenterOnDwell = true;
    /// Grow the static zoom window to fill the monitor's short axis.
    bool mouseMoveMagPickFullScreen = false;
    /// Remember a desktop dwell and immediately magnify that point when Move-to arms.
    bool mouseMoveForesight = false;
    /// Hold gaze this long (ms) to store a foresight point (kept for 2s).
    int mouseMoveForesightDwellMs = 400;
    /// When foresight and pre-click zoom both apply: a second zoom inside the
    /// foresight region. Off = place the cursor if the pick is inside foresight.
    bool mouseMoveForesightDoubleZoom = false;

    // --- Progress visuals (boards + mouse-move) ---
    bool progressRadial = true;
    bool progressFill = false;
    bool progressBorder = false;
    QString progressColor = QStringLiteral("#00DCFF");
    QString progressFillColor = QStringLiteral("#00B4DC46");
    QString progressBorderColor = QStringLiteral("#00DCFF");
    bool mouseProgressRadial = true;
    bool mouseProgressFill = false;
    bool mouseProgressBorder = true;
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

    // --- Magnifier ---
    double magZoom = 2.0;
    int magLensSize = 440;
    int magFollowProfile = 1;

    // --- Look-to-Scroll ---
    int ltsDeadzonePx = 110;
    int ltsFalloffPx = 360;
    double ltsMaxNotchesPerSec = 6.0;
    /// Scroll rate grows by this factor each second gaze stays outside deadzone.
    double ltsAccelPerSec = 0.45;
    int ltsCenterDwellMs = 650;
    bool ltsPlaceCursorFirst = true;

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

    // --- Theme (Voice-aligned) ---
    ThemeMode themeMode = ThemeMode::Dark;
    ThemeColors lightColors = ThemeColors::lightPreset();
    ThemeColors darkColors = ThemeColors::darkPreset();
    ThemeColors customColors = ThemeColors::darkPreset();
    /// Custom theme seeds (Material 3-style). Contrast remaps tones of all four.
    QString customBgColor = QStringLiteral("#0A0A0B");
    QString customPrimaryColor = QStringLiteral("#8AB4F8");
    QString customSecondaryColor = QStringLiteral("#00DCFF");
    QString customTertiaryColor = QStringLiteral("#7E5260");
    QString customSurfaceColor = QStringLiteral("#121314");
    QString customTextColor = QStringLiteral("#E6E1E5");
    QString customDangerColor = QStringLiteral("#FFB4AB");
    /// Contrast intensity: 70 (Low), 85 (Medium), or 100 (High).
    int customContrast = kThemeContrastMediumPct;
    /// Unused; kept so older settings files still load.
    int themeBrightness = 4;

    void setCustomContrast(int contrastPercent);
    /// Rebuild customColors + progress. When fitContrast is false, stored role colors stay put.
    void applyCustomPalette(bool fitContrast = false);
    /// Infer Low/Medium/High from the current background and primary (migration / diagnostics).
    void syncThemeSlidersFromSeeds();
    [[nodiscard]] QColor suggestedThemeColor(const QString& key) const;
    [[nodiscard]] ThemeSeeds themeSeeds() const;
    [[nodiscard]] static QString themeRoleForColorKey(const QString& key);
    [[nodiscard]] static ThemeColorRole themeColorRoleForKey(const QString& key);
    [[nodiscard]] static bool isThemeSeedKey(const QString& key);

    [[nodiscard]] ThemeColors resolvedTheme() const
    {
        switch (themeMode) {
        case ThemeMode::Light:
            return lightColors;
        case ThemeMode::Custom:
            return customColors;
        case ThemeMode::Dark:
        default:
            return darkColors;
        }
    }

    [[nodiscard]] static QVector<int> defaultDwellSequence()
    {
        return {800, 600, 400, 200, 100, 50};
    }
    [[nodiscard]] static AppSettings defaults();
    [[nodiscard]] static QString defaultFilePath();

    [[nodiscard]] bool loadFromFile(const QString& path, QString* error = nullptr);
    [[nodiscard]] bool saveToFile(const QString& path, QString* error = nullptr) const;

    void setDwellPreset(int preset);
    [[nodiscard]] int dwellPreset() const;
    void setMagFollowProfile(int profile);
    void clamp();
    /// Nudge a numeric field by one step. Returns false if key is not numeric.
    bool nudge(const QString& key, int dir);

    [[nodiscard]] QString displayValue(const QString& key) const;
    [[nodiscard]] static QString settingTitle(const QString& key);
    [[nodiscard]] static QString settingDescription(const QString& key);
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
    [[nodiscard]] static QVector<int> parseDwellSequence(const QString& text, QString* error = nullptr);
    [[nodiscard]] static QColor parseColor(const QString& hex, const QColor& fallback = Qt::cyan);
    [[nodiscard]] static QString colorToHex(const QColor& c);
};

} // namespace gazer
