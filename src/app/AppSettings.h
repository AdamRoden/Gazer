#pragma once

#include "ui/Theme.h"

#include <QColor>
#include <QString>
#include <QVector>

namespace gazer {

/// User preferences persisted to JSON under AppData.
struct AppSettings {
    // --- Timing ---
    /// Progressive dwell steps (ms), cycled while holding. e.g. 600,300,100,600
    QVector<int> dwellSequence = {700};
    /// Time on-target before dwell progress animation / sequence begins (ms).
    int scanGraceMs = 100;
    int dwellGraceMs = 180;
    int mouseMoveDwellMs = 700;
    /// Cancel armed mouse-move / click-loop if no target is selected within this many ms.
    /// 0 = disabled.
    int mouseMoveSelectTimeoutMs = 5000;
    /// Mouse dwell-move uses static magnify + second dwell to refine point.
    bool mouseMoveMagPick = false;
    /// Place static magnifier centered on the first-dwell point (else screen center).
    bool mouseMoveMagPickCenterOnDwell = true;

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
    QString mouseProgressColor = QStringLiteral("#FFC828");
    QString mouseProgressFillColor = QStringLiteral("#FFC82840");
    QString mouseProgressBorderColor = QStringLiteral("#FFC828");
    bool flashOnComplete = true;
    QString flashBorderColor = QStringLiteral("#FFFFFF");
    QString flashFillColor = QStringLiteral("#00DCFF78");
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
    /// Secondaries: no dwell for idleMs → instant 50% for fadeMs → 500ms suck → close.
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

    [[nodiscard]] static AppSettings defaults();
    [[nodiscard]] static QString defaultFilePath();

    [[nodiscard]] bool loadFromFile(const QString& path, QString* error = nullptr);
    [[nodiscard]] bool saveToFile(const QString& path, QString* error = nullptr) const;

    void setDwellPreset(int preset);
    [[nodiscard]] int dwellPreset() const;
    void setMagFollowProfile(int profile);
    void clamp();

    [[nodiscard]] QString displayValue(const QString& key) const;
    [[nodiscard]] static QString settingTitle(const QString& key);
    [[nodiscard]] static QString settingDescription(const QString& key);
    [[nodiscard]] static bool isNumericKey(const QString& key);
    [[nodiscard]] static bool isColorKey(const QString& key);
    [[nodiscard]] bool applyNumericBuffer(const QString& key, const QString& buffer,
                                          QString* error = nullptr);
    [[nodiscard]] QString numericBufferSeed(const QString& key) const;
    [[nodiscard]] bool setColorKey(const QString& key, const QColor& c);
    [[nodiscard]] QColor colorKey(const QString& key) const;

    [[nodiscard]] QString dwellSequenceString() const;
    [[nodiscard]] static QVector<int> parseDwellSequence(const QString& text, QString* error = nullptr);
    [[nodiscard]] static QColor parseColor(const QString& hex, const QColor& fallback = Qt::cyan);
    [[nodiscard]] static QString colorToHex(const QColor& c);
};

} // namespace gazer
