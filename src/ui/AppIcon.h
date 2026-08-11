#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QSize>
#include <QString>
#include <QStringList>

namespace gazer {

/// Load the Gazer brand icon from the staged `resources/icons` tree (next to the exe).
/// Prefers the multi-size `.ico`, then individual PNGs. Safe to call after QApplication exists.
inline QIcon loadAppIcon()
{
    const QStringList roots = {
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("resources/icons")),
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../resources/icons")),
        QStringLiteral("resources/icons"),
    };

    QIcon icon;
    for (const QString& root : roots) {
        const QString ico = QDir(root).filePath(QStringLiteral("gazer.ico"));
        if (QFile::exists(ico)) {
            icon.addFile(ico);
        }
        static const int kSizes[] = {16, 24, 32, 48, 64, 128, 256, 512};
        for (int s : kSizes) {
            const QString png =
                QDir(root).filePath(QStringLiteral("gazer-%1.png").arg(s));
            if (QFile::exists(png)) {
                icon.addFile(png, QSize(s, s));
            }
        }
        if (!icon.isNull()) {
            break;
        }
    }
    return icon;
}

} // namespace gazer
