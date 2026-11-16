#pragma once

#include "app/AppSettings.h"

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <optional>

namespace gazer {

struct SettingsCatalogEntry {
    const char* id = nullptr;
    const char* page = nullptr;
    const char* title = nullptr;
    const char* description = nullptr;
    /// Comma-separated JSON keys. One dot selects a child (`lookToMaps.scroll`).
    const char* jsonKeys = nullptr;
};

[[nodiscard]] const SettingsCatalogEntry* settingsCatalogEntries();
[[nodiscard]] int settingsCatalogCount();
[[nodiscard]] const SettingsCatalogEntry* findSettingsCatalogEntry(const QString& id);
[[nodiscard]] QStringList settingsCatalogJsonKeys(const SettingsCatalogEntry& entry);
[[nodiscard]] QString settingValueText(const AppSettings& settings, const QString& id);

/// Which editor View defaults opens. Sequence is tested before Number because
/// `isNumericKey` is also true for sequences. `Page` jumps to the board that
/// hosts the control. The row icon and `editDefaultSetting` both use this.
enum class DefaultsEditorKind { Sequence, Number, Color, LookTo, Bool, Choice, Page };

[[nodiscard]] DefaultsEditorKind defaultsEditorKind(const AppSettings& settings, const QString& id);
/// On/off catalog rows. Empty when `id` is not a bool or a progress-style toggle.
[[nodiscard]] std::optional<bool> settingsBoolValue(const AppSettings& settings, const QString& id);
/// Write a row from `settingsBoolValue`. Returns false when `id` is not one of those rows.
bool assignSettingsBool(AppSettings& settings, const QString& id, bool value);

/// View-defaults filter menu, in toolbar order. `All` is the first entry.
inline constexpr const char* kSettingsDefaultsPages[] = {
    "All", "Dwell", "Pointer", "Gaze", "Head", "Speak", "Theme", "Setup"};

[[nodiscard]] inline bool settingsEntryVisible(const SettingsCatalogEntry& entry,
                                              const QString& filter, const QString& query)
{
    if (filter != QLatin1String("All") && filter != QLatin1String(entry.page)) {
        return false;
    }
    const QString needle = query.trimmed();
    if (needle.isEmpty()) {
        return true;
    }
    const auto has = [&](const char* text) {
        return QString::fromLatin1(text).contains(needle, Qt::CaseInsensitive);
    };
    return has(entry.title) || has(entry.description) || has(entry.id) || has(entry.page);
}

enum class SettingsImportKind { Ok, Empty, Newer };

/// `Empty` is an object with no schema version and no catalog key.
/// `Newer` is a schemaVersion this build does not understand.
[[nodiscard]] SettingsImportKind classifySettingsImport(const QJsonObject& object);

} // namespace gazer
