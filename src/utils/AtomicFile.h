#pragma once

#include <QByteArray>
#include <QFileInfo>
#include <QSaveFile>
#include <QString>

namespace gazer {

/// Temp file + commit so a crash cannot leave a truncated target.
inline bool writeFileAtomically(const QString& path, const QByteArray& data,
                                QString* error = nullptr)
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = QStringLiteral("Could not write %1").arg(QFileInfo(path).fileName());
        }
        return false;
    }
    if (f.write(data) != data.size()) {
        f.cancelWriting();
        if (error) {
            *error = QStringLiteral("Could not write %1").arg(QFileInfo(path).fileName());
        }
        return false;
    }
    if (!f.commit()) {
        if (error) {
            *error = QStringLiteral("Could not write %1").arg(QFileInfo(path).fileName());
        }
        return false;
    }
    return true;
}

} // namespace gazer
