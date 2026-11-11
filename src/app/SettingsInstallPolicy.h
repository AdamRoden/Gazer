#pragma once

namespace gazer {

/// How this MSI invocation relates to an existing install.
/// Fresh, upgrade, repair, and same-version reinstall all keep settings unless
/// the user asked for a factory reset (`RESETUSERSETTINGS=1`).
enum class SettingsInstallKind {
    Fresh,
    Upgrade,
    Repair,
    SameVersionReinstall
};

struct SettingsInstallDecision {
    /// Leave `%AppData%\Gazer\settings.json` in place.
    bool preserveSettings = true;
    /// Delete the live settings file so the next launch writes factory defaults.
    bool factoryReset = false;
};

/// Installer policy shared with tests. `packaging/Gazer.wxs` must match:
/// `settings.json` is removed only when a factory reset was requested.
[[nodiscard]] inline SettingsInstallDecision decideSettingsInstall(SettingsInstallKind kind,
                                                                   bool resetRequested)
{
    (void)kind;
    SettingsInstallDecision d;
    d.factoryReset = resetRequested;
    d.preserveSettings = !resetRequested;
    return d;
}

} // namespace gazer
