#pragma once

#include <QColor>
#include <QJsonObject>
#include <QString>
#include <QVector>

namespace gazer {

/// Voice-aligned chrome palette (light / dark / custom bags).
/// JSON ownership lives here (toJson/fromJson); AppSettings only stores bags + mode.
struct ThemeColors {
    QColor bgMain;
    QColor bgSurface;
    QColor bgSurfaceHover;
    QColor bgSurfaceActive;
    QColor border;
    QColor accent;
    QColor accentHover;
    QColor text;
    QColor textSecondary;
    QColor cellBg;
    QColor cellHover;
    QColor cellActive;
    QColor danger;

    [[nodiscard]] static ThemeColors darkPreset();
    [[nodiscard]] static ThemeColors lightPreset();

    [[nodiscard]] QJsonObject toJson() const;
    void fromJson(const QJsonObject& o);

    /// Hex helpers used by settings persistence and UI.
    [[nodiscard]] static QColor parseColor(const QString& hex, const QColor& fallback = Qt::cyan);
    [[nodiscard]] static QString colorToHex(const QColor& c);
};

enum class ThemeMode {
    Dark,
    Light,
    Custom
};

[[nodiscard]] QString themeModeToString(ThemeMode m);
[[nodiscard]] ThemeMode themeModeFromString(const QString& s);

/// Voice sample palette (from Voice/js/app.js COLOR_PALETTE).
[[nodiscard]] QVector<QString> voiceColorPalette();

} // namespace gazer
