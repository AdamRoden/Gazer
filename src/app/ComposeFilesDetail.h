#pragma once

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace gazer {
namespace files_detail {

/// Rows on screen in the File Explorer list. An offset is the first visible item.
constexpr int kFileRows = 5;
/// Tile view is this many columns by rows. One arrow step moves one row of tiles.
constexpr int kFileTileCols = 8;
constexpr int kFileTileRows = 4;

[[nodiscard]] inline int filePageSize(bool tiles)
{
    return tiles ? kFileTileCols * kFileTileRows : kFileRows;
}

[[nodiscard]] inline int fileRowStep(bool tiles)
{
    return tiles ? kFileTileCols : 1;
}

[[nodiscard]] inline int maxFileOffset(int count, bool tiles)
{
    return qMax(0, count - filePageSize(tiles));
}

[[nodiscard]] inline int clampFileOffset(int offset, int count, bool tiles)
{
    return qBound(0, offset, maxFileOffset(count, tiles));
}

struct PathChip {
    QString label;
    QString path;
};

struct Place {
    QString label;
    QString path;
    QString icon;
};

[[nodiscard]] inline QString browserEntryLabel(const QString& path)
{
    const QFileInfo info(path);
    if (info.fileName().isEmpty() || info.isRoot()) {
        QString root = info.absoluteFilePath();
        while (root.endsWith(QLatin1Char('/')) || root.endsWith(QLatin1Char('\\'))) {
            root.chop(1);
        }
        return root.isEmpty() ? path : root;
    }
    return info.fileName();
}

[[nodiscard]] inline bool samePath(const QString& a, const QString& b)
{
    if (a.isEmpty() || b.isEmpty()) {
        return a.isEmpty() && b.isEmpty();
    }
    return QDir::cleanPath(a).compare(QDir::cleanPath(b), Qt::CaseInsensitive) == 0;
}

[[nodiscard]] inline QVector<PathChip> pathChips(const QString& dir)
{
    QVector<PathChip> chips;
    if (dir.isEmpty()) {
        chips.push_back({QStringLiteral("This PC"), QString()});
        return chips;
    }
    const QString clean = QDir::cleanPath(dir);
    if (clean.startsWith(QLatin1String("//"))) {
        const QStringList parts = clean.mid(2).split(QLatin1Char('/'), Qt::SkipEmptyParts);
        QString acc = QStringLiteral("//");
        for (int i = 0; i < parts.size(); ++i) {
            if (i == 0) {
                acc += parts.at(i);
            } else {
                acc += QLatin1Char('/') + parts.at(i);
            }
            chips.push_back({parts.at(i), acc});
        }
        if (chips.isEmpty()) {
            chips.push_back({QStringLiteral("This PC"), QString()});
        }
        return chips;
    }
    const QStringList parts = clean.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    QString acc;
    for (int i = 0; i < parts.size(); ++i) {
        if (i == 0) {
            acc = parts.at(i);
        } else {
            acc += QLatin1Char('/') + parts.at(i);
        }
        QString path = acc;
        if (i == 0 && path.size() == 2 && path.at(1) == QLatin1Char(':')) {
            path += QLatin1Char('/');
        }
        chips.push_back({parts.at(i), QDir::cleanPath(path)});
    }
    if (chips.isEmpty()) {
        chips.push_back({clean, clean});
    }
    return chips;
}

[[nodiscard]] inline QVector<Place> filePlaces()
{
    QVector<Place> out;
    auto add = [&](const QString& label, const QString& path, const QString& icon) {
        if (path.isEmpty()) {
            return;
        }
        const QString clean = QDir::cleanPath(path);
        if (!QFileInfo(clean).exists()) {
            return;
        }
        out.push_back({label, clean, icon});
    };
    add(QStringLiteral("Desktop"),
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
        QStringLiteral("home"));
    add(QStringLiteral("Documents"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        QStringLiteral("folder"));
    add(QStringLiteral("Downloads"),
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
        QStringLiteral("ArrowDown"));
    add(QStringLiteral("Pictures"),
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
        QStringLiteral("photoCamera"));
    add(QStringLiteral("Music"),
        QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
        QStringLiteral("musicNote"));
    out.push_back({QStringLiteral("This PC"), QString(), QStringLiteral("hardDrive")});
    const QFileInfoList drives = QDir::drives();
    for (const QFileInfo& drive : drives) {
        const QString root = QDir::cleanPath(drive.absoluteFilePath());
        out.push_back({browserEntryLabel(root), root, QStringLiteral("hardDrive")});
    }
    return out;
}

} // namespace files_detail
} // namespace gazer
