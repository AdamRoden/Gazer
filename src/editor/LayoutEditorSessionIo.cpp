#include "editor/LayoutEditorSession.h"

#include "layout/PageLoader.h"
#include "layout/PageWriter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace gazer {

namespace {

bool replaceWithBackup(const QString& tmp, const QString& finalPath, QString* error)
{
    const QString bak = finalPath + QStringLiteral(".bak");
    const bool hadFinal = QFileInfo::exists(finalPath);
    if (hadFinal) {
        QFile::remove(bak);
        if (!QFile::rename(finalPath, bak)) {
            if (error) {
                *error = QStringLiteral("Could not back up %1").arg(finalPath);
            }
            return false;
        }
    }
    if (!QFile::rename(tmp, finalPath)) {
        if (error) {
            *error = QStringLiteral("Could not write %1").arg(finalPath);
        }
        QFile::remove(finalPath);
        if (hadFinal) {
            QFile::rename(bak, finalPath);
        }
        return false;
    }
    if (hadFinal) {
        QFile::remove(bak);
    }
    return true;
}

} // namespace

bool LayoutEditorSession::loadFromFile(const QString& path, QString* error)
{
    PageDocument doc;
    if (!PageLoader::loadFromFile(path, doc, error)) {
        return false;
    }
    replaceDocument(std::move(doc), path, false);
    emit statusMessage(QStringLiteral("Opened %1").arg(path));
    return true;
}

bool LayoutEditorSession::importFromFile(const QString& path, QString* error)
{
    PageDocument doc;
    if (!PageLoader::loadFromFile(path, doc, error)) {
        return false;
    }
    replaceDocument(std::move(doc), {}, true);
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
    if (!QDir().mkpath(dir)) {
        if (error) {
            *error = QStringLiteral("Could not create %1").arg(dir);
        }
        return false;
    }

    PageDocument doc = m_doc;
    doc.id = fi.completeBaseName();
    const QString tmp = path + QStringLiteral(".tmp");
    QFile::remove(tmp);
    if (!PageWriter::saveToFile(doc, tmp, error)) {
        QFile::remove(tmp);
        return false;
    }
    if (!replaceWithBackup(tmp, path, error)) {
        QFile::remove(tmp);
        return false;
    }

    m_doc.id = doc.id;
    m_filePath = path;
    m_undo.setClean();
    emit filePathChanged(m_filePath);
    emit documentChanged();
    emit statusMessage(QStringLiteral("Saved %1").arg(path));
    return true;
}

bool LayoutEditorSession::exportTo(const QString& path, QString* error)
{
    if (!PageWriter::saveToFile(document(), path, error)) {
        return false;
    }
    emit statusMessage(QStringLiteral("Exported %1").arg(path));
    return true;
}

} // namespace gazer
