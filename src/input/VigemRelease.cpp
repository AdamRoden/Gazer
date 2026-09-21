#include "input/VigemRelease.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace gazer {

QString vigemBusSetupUrlFromReleaseJson(const QByteArray& json, QString* error)
{
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        if (error) {
            *error = QStringLiteral("ViGEm release JSON parse error: %1").arg(pe.errorString());
        }
        return {};
    }
    const QJsonArray assets = doc.object().value(QStringLiteral("assets")).toArray();
    QString any;
    QString preferred;
    for (const QJsonValue& v : assets) {
        if (!v.isObject()) {
            continue;
        }
        const QJsonObject o = v.toObject();
        const QString name = o.value(QStringLiteral("name")).toString();
        const QString url = o.value(QStringLiteral("browser_download_url")).toString();
        if (url.isEmpty()) {
            continue;
        }
        const QString n = name.toLower();
        if (!n.contains(QLatin1String("vigembus"))) {
            continue;
        }
        if (!n.endsWith(QLatin1String(".exe")) && !n.endsWith(QLatin1String(".msi"))) {
            continue;
        }
        if (any.isEmpty()) {
            any = url;
        }
        if (n.contains(QLatin1String("x64")) || n.contains(QLatin1String("x86_64"))
            || n.contains(QLatin1String("arm64"))) {
            preferred = url;
            break;
        }
    }
    const QString url = preferred.isEmpty() ? any : preferred;
    if (url.isEmpty() && error) {
        *error = QStringLiteral("ViGEm release has no setup asset");
    }
    return url;
}

} // namespace gazer
