#pragma once

namespace gazer {

/// How this MSI invocation relates to an existing install.
/// Fresh, upgrade, repair, and same-version reinstall all keep settings.
enum class SettingsInstallKind {
    Fresh,
    Upgrade,
    Repair,
    SameVersionReinstall
};

struct SettingsInstallDecision {
    /// Leave `%AppData%\Gazer\settings.json` in place.
    bool preserveSettings = true;
    /// The installer does not delete the live settings file.
    bool factoryReset = false;
};

/// Installer policy shared with tests. `packaging/Gazer.wxs` keeps
/// `settings.json` on every install kind.
[[nodiscard]] inline SettingsInstallDecision decideSettingsInstall(SettingsInstallKind kind)
{
    (void)kind;
    return {};
}

} // namespace gazer
