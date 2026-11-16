#include "app/UpdateRelease.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace gazer {

VersionTriplet parseVersionTriplet(const QString& text)
{
    static const QRegularExpression re(QStringLiteral(R"((\d+)\.(\d+)\.(\d+))"));
    const QRegularExpressionMatch match = re.match(text);
    VersionTriplet out;
    if (!match.hasMatch()) {
        return out;
    }
    out.major = match.captured(1).toInt();
    out.minor = match.captured(2).toInt();
    out.patch = match.captured(3).toInt();
    out.ok = true;
    return out;
}

int compareVersions(const VersionTriplet& a, const VersionTriplet& b)
{
    if (a.major != b.major) {
        return a.major < b.major ? -1 : 1;
    }
    if (a.minor != b.minor) {
        return a.minor < b.minor ? -1 : 1;
    }
    if (a.patch != b.patch) {
        return a.patch < b.patch ? -1 : 1;
    }
    return 0;
}

namespace {

bool gazerMsiName(const QString& name, QString* versionOut)
{
    static const QRegularExpression re(
        QStringLiteral(R"(^Gazer-(\d+\.\d+\.\d+(?:-beta)?)\.msi$)"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = re.match(name);
    if (!match.hasMatch()) {
        return false;
    }
    if (versionOut) {
        *versionOut = match.captured(1);
    }
    return parseVersionTriplet(match.captured(1)).ok;
}

} // namespace

GazerMsiRelease newestGazerMsi(const QByteArray& releasesJson, const QString& currentVersion)
{
    GazerMsiRelease best;
    const VersionTriplet current = parseVersionTriplet(currentVersion);
    const QJsonDocument doc = QJsonDocument::fromJson(releasesJson);
    if (!doc.isArray()) {
        return best;
    }
    for (const QJsonValue& releaseValue : doc.array()) {
        if (!releaseValue.isObject()) {
            continue;
        }
        const QJsonObject release = releaseValue.toObject();
        if (release.value(QStringLiteral("draft")).toBool(false)) {
            continue;
        }
        const QJsonArray assets = release.value(QStringLiteral("assets")).toArray();
        for (const QJsonValue& assetValue : assets) {
            if (!assetValue.isObject()) {
                continue;
            }
            const QJsonObject asset = assetValue.toObject();
            const QString name = asset.value(QStringLiteral("name")).toString();
            QString version;
            if (!gazerMsiName(name, &version)) {
                continue;
            }
            const VersionTriplet parsed = parseVersionTriplet(version);
            if (!parsed.ok || !current.ok || compareVersions(parsed, current) <= 0) {
                continue;
            }
            const bool newer = !best.found || compareVersions(parsed, parseVersionTriplet(best.version)) > 0;
            const bool sameStable = best.found && compareVersions(parsed, parseVersionTriplet(best.version)) == 0
                                    && best.version.contains(QLatin1String("-beta"))
                                    && !version.contains(QLatin1String("-beta"));
            if (!newer && !sameStable) {
                continue;
            }
            best.found = true;
            best.version = version;
            best.name = name;
            best.url = asset.value(QStringLiteral("browser_download_url")).toString();
        }
    }
    return best;
}

} // namespace gazer
