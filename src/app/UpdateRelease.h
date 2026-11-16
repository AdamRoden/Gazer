#pragma once

#include <QByteArray>
#include <QString>

namespace gazer {

struct VersionTriplet {
    int major = 0;
    int minor = 0;
    int patch = 0;
    bool ok = false;
};

struct GazerMsiRelease {
    QString version;
    QString name;
    QString url;
    bool found = false;
};

/// First `major.minor.patch` in `text`. A leading `v` and a `-beta` suffix are ignored.
[[nodiscard]] VersionTriplet parseVersionTriplet(const QString& text);

/// Negative when `a` is older, zero when equal, positive when `a` is newer.
[[nodiscard]] int compareVersions(const VersionTriplet& a, const VersionTriplet& b);

/// Highest Gazer MSI asset newer than `currentVersion` in a GitHub releases array.
/// Drafts are skipped. Prereleases are included. The version comes from the asset
/// name `Gazer-X.Y.Z.msi` or `Gazer-X.Y.Z-beta.msi`.
[[nodiscard]] GazerMsiRelease newestGazerMsi(const QByteArray& releasesJson,
                                             const QString& currentVersion);

} // namespace gazer
