#pragma once

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
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

enum class BrowserJob { Speak, Settings, Audio, HistoryClip };
enum class BrowserMode { Import, Export };

/// What one file-browser job shows and which writer confirm uses.
struct BrowserFace {
    QStringList patterns;
    QString documentName;
    QString caption;
    QString emptyFolder;
    QString exportSuffix;
    QString dwellHint;
    enum class Selection { Typed, Audio };
    Selection selection = Selection::Typed;
    enum class Writer { Workspace, Settings, Clip };
    Writer writer = Writer::Workspace;
};

[[nodiscard]] inline BrowserFace browserFace(BrowserJob job, BrowserMode mode,
                                              const QStringList& audioPatterns)
{
    const bool exporting = mode == BrowserMode::Export;
    BrowserFace face;
    face.patterns = QStringList{QStringLiteral("*.json")};
    face.exportSuffix = QStringLiteral(".json");
    face.emptyFolder = QStringLiteral("No folders or JSON files");
    face.dwellHint = QStringLiteral("Dwell a folder or a JSON file");
    face.documentName = exporting ? QStringLiteral("Export") : QStringLiteral("Import");
    switch (job) {
    case BrowserJob::Audio:
        face.patterns = audioPatterns;
        face.documentName = QStringLiteral("Audio");
        face.caption = QStringLiteral("Choose audio");
        face.emptyFolder = QStringLiteral("No folders or audio files");
        face.dwellHint = QStringLiteral("Dwell a folder or an audio file");
        face.selection = BrowserFace::Selection::Audio;
        break;
    case BrowserJob::HistoryClip:
        face.patterns = QStringList{QStringLiteral("*.mp3")};
        face.documentName = QStringLiteral("Save MP3");
        face.caption = QStringLiteral("Save MP3");
        face.emptyFolder = QStringLiteral("No folders or MP3 files");
        face.exportSuffix = QStringLiteral(".mp3");
        face.dwellHint = QStringLiteral("Dwell a folder or an MP3 file");
        face.writer = BrowserFace::Writer::Clip;
        break;
    case BrowserJob::Settings:
        face.documentName = exporting ? QStringLiteral("Export settings")
                                      : QStringLiteral("Import settings");
        face.caption = face.documentName;
        face.writer = BrowserFace::Writer::Settings;
        break;
    case BrowserJob::Speak:
        break;
    }
    return face;
}

[[nodiscard]] inline bool reservedWindowsStem(const QString& stem)
{
    const QString s = stem.toUpper();
    if (s == QLatin1String("CON") || s == QLatin1String("PRN") || s == QLatin1String("AUX")
        || s == QLatin1String("NUL")) {
        return true;
    }
    if (s.startsWith(QLatin1String("COM")) || s.startsWith(QLatin1String("LPT"))) {
        bool digits = s.size() > 3;
        for (int i = 3; digits && i < s.size(); ++i) {
            digits = s.at(i).isDigit();
        }
        return digits;
    }
    return false;
}

[[nodiscard]] inline bool fileNameCharOk(QChar c)
{
    if (c.unicode() < 32) {
        return false;
    }
    return QStringLiteral("\\/:*?\"<>|").indexOf(c) < 0;
}

/// Existing file in `dir` for a typed leaf. `suffix` is appended when it is missing.
[[nodiscard]] inline QString fileInDir(const QString& dir, const QString& typed, const QString& suffix)
{
    if (dir.isEmpty()) {
        return {};
    }
    QString name = typed.trimmed();
    if (name.isEmpty() || name.endsWith(QLatin1Char('.'))) {
        return {};
    }
    const QString ext = suffix.startsWith(QLatin1Char('.')) ? suffix : QStringLiteral(".") + suffix;
    if (!name.endsWith(ext, Qt::CaseInsensitive)) {
        name += ext;
    }
    for (const QChar c : name) {
        if (!fileNameCharOk(c)) {
            return {};
        }
    }
    const QFileInfo info(QDir(dir).filePath(name));
    const QString want = ext.mid(1);
    if (!info.isFile() || info.suffix().compare(want, Qt::CaseInsensitive) != 0) {
        return {};
    }
    return info.absoluteFilePath();
}

/// Phrase turned into an MP3 leaf. Empty and reserved stems get a timestamped name.
[[nodiscard]] inline QString historyClipLeaf(const QString& phrase)
{
    QString stem;
    bool gap = false;
    for (const QChar c : phrase.trimmed()) {
        if (c.isLetterOrNumber()) {
            if (gap && !stem.isEmpty()) {
                stem += QLatin1Char('-');
            }
            gap = false;
            stem += c;
            if (stem.size() >= 48) {
                break;
            }
        } else if (!stem.isEmpty()) {
            gap = true;
        }
    }
    while (stem.endsWith(QLatin1Char('-'))) {
        stem.chop(1);
    }
    if (stem.isEmpty() || reservedWindowsStem(stem)) {
        stem = QStringLiteral("gazer-clip-%1")
                   .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd-HHmmss")));
    }
    return stem + QStringLiteral(".mp3");
}

} // namespace files_detail
} // namespace gazer
