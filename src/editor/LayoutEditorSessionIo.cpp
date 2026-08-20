#include "editor/LayoutEditorSession.h"

#include "layout/LayoutLoader.h"
#include "layout/LayoutWriter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace gazer {

namespace {

int suffixIndex(QString* family)
{
    if (family->endsWith(QLatin1String("_shift"))) {
        family->chop(6);
        return 1;
    }
    if (family->endsWith(QLatin1String("_sym"))) {
        family->chop(4);
        return 2;
    }
    return 0;
}

bool actionRefersTo(const LayoutAction& a, const QString& id)
{
    return !id.isEmpty() && a.layoutId == id;
}

bool documentRefersTo(const LayoutDocument& doc, const QString& id)
{
    const auto scan = [&](const QVector<LayoutAction>& acts) {
        for (const LayoutAction& a : acts) {
            if (actionRefersTo(a, id)) {
                return true;
            }
        }
        return false;
    };
    if (scan(doc.onOpen) || scan(doc.onLoad) || scan(doc.onClose)) {
        return true;
    }
    for (const LayoutItem& it : doc.items) {
        if (scan(it.effectiveActions())) {
            return true;
        }
    }
    return false;
}

bool loadLayerFile(const QString& path, LayoutDocument& out)
{
    QString err;
    return QFileInfo::exists(path) && LayoutLoader::loadFromFile(path, out, &err);
}

QVector<EditorLayer> assembleFamily(const QString& dir, const QString& family, int opened,
                                    LayoutDocument openedDoc)
{
    const QString basePath = QDir(dir).filePath(family + QStringLiteral(".json"));
    const QString shiftPath = QDir(dir).filePath(family + QStringLiteral("_shift.json"));
    const QString symPath = QDir(dir).filePath(family + QStringLiteral("_sym.json"));

    LayoutDocument base;
    LayoutDocument shift;
    LayoutDocument sym;
    const bool hasBaseFile = opened == 0 || loadLayerFile(basePath, base);
    const bool hasShift = opened == 1 || loadLayerFile(shiftPath, shift);
    const bool hasSym = opened == 2 || loadLayerFile(symPath, sym);
    if (opened == 0) {
        base = openedDoc;
    } else if (opened == 1) {
        shift = openedDoc;
    } else {
        sym = openedDoc;
    }

    const bool looksFamily =
        opened != 0 || documentRefersTo(openedDoc, family + QStringLiteral("_shift"))
        || documentRefersTo(openedDoc, family + QStringLiteral("_sym"))
        || (hasShift && hasSym);

    if (!looksFamily) {
        return {{QStringLiteral("Base"), {}, std::move(openedDoc)}};
    }

    QVector<EditorLayer> layers;
    if (opened == 0 || hasBaseFile) {
        layers.push_back({QStringLiteral("Base"), {}, std::move(base)});
    }
    if (hasShift) {
        layers.push_back({QStringLiteral("Shift"), QStringLiteral("_shift"), std::move(shift)});
    }
    if (hasSym) {
        layers.push_back({QStringLiteral("Symbols"), QStringLiteral("_sym"), std::move(sym)});
    }
    if (layers.isEmpty()) {
        return {{QStringLiteral("Base"), {}, std::move(openedDoc)}};
    }
    return layers;
}

int indexForOpened(const QVector<EditorLayer>& layers, int opened)
{
    const QString want = opened == 1 ? QStringLiteral("_shift")
                                     : (opened == 2 ? QStringLiteral("_sym") : QString());
    for (int i = 0; i < layers.size(); ++i) {
        if (layers[i].suffix == want) {
            return i;
        }
    }
    return 0;
}

bool replaceWithBackup(const QStringList& temps, const QStringList& finals, QString* error)
{
    QStringList baks;
    baks.reserve(finals.size());
    for (int i = 0; i < finals.size(); ++i) {
        if (!QFileInfo::exists(finals[i])) {
            baks.push_back({});
            continue;
        }
        const QString bak = finals[i] + QStringLiteral(".bak");
        QFile::remove(bak);
        if (!QFile::rename(finals[i], bak)) {
            if (error) {
                *error = QStringLiteral("Could not back up %1").arg(finals[i]);
            }
            for (int j = 0; j < i; ++j) {
                if (!baks[j].isEmpty()) {
                    QFile::remove(finals[j]);
                    QFile::rename(baks[j], finals[j]);
                }
            }
            return false;
        }
        baks.push_back(bak);
    }
    for (int i = 0; i < finals.size(); ++i) {
        if (!QFile::rename(temps[i], finals[i])) {
            if (error) {
                *error = QStringLiteral("Could not write %1").arg(finals[i]);
            }
            for (int j = 0; j <= i; ++j) {
                QFile::remove(finals[j]);
            }
            for (int j = 0; j < finals.size(); ++j) {
                if (!baks[j].isEmpty()) {
                    QFile::rename(baks[j], finals[j]);
                }
            }
            return false;
        }
    }
    for (const QString& bak : baks) {
        if (!bak.isEmpty()) {
            QFile::remove(bak);
        }
    }
    return true;
}

} // namespace

bool LayoutEditorSession::loadFromFile(const QString& path, QString* error)
{
    LayoutDocument doc;
    if (!LayoutLoader::loadFromFile(path, doc, error)) {
        return false;
    }
    const QFileInfo fi(path);
    QString family = fi.completeBaseName();
    const int opened = suffixIndex(&family);
    auto layers = assembleFamily(fi.absolutePath(), family, opened, std::move(doc));
    const int index = indexForOpened(layers, opened);
    replaceProject(std::move(layers), index, path, false);
    emit statusMessage(QStringLiteral("Opened %1").arg(path));
    return true;
}

bool LayoutEditorSession::loadFromJson(const QByteArray& json, QString* error)
{
    LayoutDocument doc;
    if (!LayoutLoader::loadFromJson(json, doc, error)) {
        return false;
    }
    replaceProject({{QStringLiteral("Base"), {}, std::move(doc)}}, 0, m_filePath, true);
    return true;
}

bool LayoutEditorSession::importFromFile(const QString& path, QString* error)
{
    LayoutDocument doc;
    if (!LayoutLoader::loadFromFile(path, doc, error)) {
        return false;
    }
    replaceProject({{QStringLiteral("Base"), {}, std::move(doc)}}, 0, {}, true);
    emit statusMessage(QStringLiteral("Imported %1").arg(path));
    return true;
}

bool LayoutEditorSession::save(QString* error)
{
    if (m_filePath.isEmpty()) {
        if (error) {
            *error = QStringLiteral("No file path");
        }
        return false;
    }
    return saveTo(m_filePath, error);
}

bool LayoutEditorSession::saveTo(const QString& path, QString* error)
{
    const QFileInfo fi(path);
    const QString dir = fi.absolutePath();
    QString family = fi.completeBaseName();
    suffixIndex(&family);
    if (!QDir().mkpath(dir)) {
        if (error) {
            *error = QStringLiteral("Could not create %1").arg(dir);
        }
        return false;
    }

    QVector<LayoutDocument> written;
    written.reserve(m_layers.size());
    QStringList temps;
    QStringList finals;
    for (const EditorLayer& layer : m_layers) {
        LayoutDocument doc = layer.doc;
        doc.id = family + layer.suffix;
        const QString out = QDir(dir).filePath(doc.id + QStringLiteral(".json"));
        const QString tmp = out + QStringLiteral(".tmp");
        QFile::remove(tmp);
        if (!LayoutWriter::saveToFile(doc, tmp, error)) {
            for (const QString& t : temps) {
                QFile::remove(t);
            }
            return false;
        }
        written.push_back(std::move(doc));
        temps.push_back(tmp);
        finals.push_back(out);
    }

    if (!replaceWithBackup(temps, finals, error)) {
        for (const QString& t : temps) {
            QFile::remove(t);
        }
        return false;
    }

    for (int i = 0; i < m_layers.size(); ++i) {
        m_layers[i].doc.id = written[i].id;
    }
    m_filePath = path;
    m_undo.setClean();
    emit filePathChanged(m_filePath);
    emit documentChanged();
    emit statusMessage(QStringLiteral("Saved %1").arg(path));
    return true;
}

bool LayoutEditorSession::exportTo(const QString& path, QString* error)
{
    if (!LayoutWriter::saveToFile(document(), path, error)) {
        return false;
    }
    emit statusMessage(QStringLiteral("Exported %1").arg(path));
    return true;
}

} // namespace gazer
