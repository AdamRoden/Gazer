#pragma once

#include <QByteArray>
#include <QString>

namespace gazer {

constexpr const char kViGEmBusLatestApi[] =
    "https://api.github.com/repos/nefarius/ViGEmBus/releases/latest";

/// `browser_download_url` for a ViGEmBus setup `.exe` / `.msi` in a GitHub release JSON body.
[[nodiscard]] QString vigemBusSetupUrlFromReleaseJson(const QByteArray& json,
                                                      QString* error = nullptr);

} // namespace gazer
