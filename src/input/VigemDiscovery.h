#pragma once

#include <QString>
#include <QStringList>

namespace gazer {

/// Find ViGEmClient.dll and the ViGEmBus driver. No process-global cache.
struct VigemDiscovery {
    /// `GAZER_VIGEM_DLL`, next to Gazer.exe, `%AppData%\Gazer`, Nefarius "ViGEm Client" folder.
    [[nodiscard]] static QStringList clientDllCandidates();
    [[nodiscard]] static QString findClientDll();
    [[nodiscard]] static bool busLooksInstalled();
    [[nodiscard]] static QString describeInstall();
};

} // namespace gazer
