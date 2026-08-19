#include "layout/LayoutManager.h"

#include "layout/LayoutLoader.h"
#include "utils/Log.h"

#include <QDir>
#include <QFileInfo>
#include <QStringList>

namespace gazer {

LayoutManager::LayoutManager(QObject* parent)
    : QObject(parent)
{
}

void LayoutManager::setLayoutsDirectory(const QString& dir)
{
    m_dir = dir;
}

int LayoutManager::scanDirectory()
{
    if (m_dir.isEmpty()) {
        return 0;
    }
    QDir d(m_dir);
    if (!d.exists()) {
        GAZER_WARN << "Layouts directory missing:" << m_dir;
        return 0;
    }

    int count = 0;
    const QFileInfoList files =
        d.entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    for (const QFileInfo& fi : files) {
        QString err;
        if (loadFile(fi.absoluteFilePath(), &err)) {
            ++count;
        } else {
            GAZER_WARN << "Skip layout" << fi.fileName() << ":" << err;
        }
    }
    GAZER_INFO << "Scanned layouts directory:" << m_dir << "loaded:" << count;
    return count;
}

bool LayoutManager::loadFile(const QString& path, QString* error)
{
    LayoutDocument doc;
    if (!LayoutLoader::loadFromFile(path, doc, error)) {
        return false;
    }
    m_layouts.insert(doc.id, doc);
    return true;
}

bool LayoutManager::hasLayout(const QString& layoutId) const
{
    return m_layouts.contains(layoutId);
}

bool LayoutManager::loadLayoutId(const QString& layoutId, QString* error)
{
    if (m_layouts.contains(layoutId)) {
        return true;
    }
    if (m_dir.isEmpty()) {
        if (error) {
            *error = QStringLiteral("No layouts directory configured");
        }
        return false;
    }
    const QString path = QDir(m_dir).filePath(layoutId + QStringLiteral(".json"));
    if (!QFileInfo::exists(path)) {
        if (error) {
            *error = QStringLiteral("Layout file not found: %1").arg(path);
        }
        emit layoutLoadFailed(layoutId, error ? *error : QString());
        return false;
    }
    if (!loadFile(path, error)) {
        emit layoutLoadFailed(layoutId, error ? *error : QString());
        return false;
    }
    return true;
}

const LayoutDocument* LayoutManager::document(const QString& layoutId) const
{
    auto it = m_layouts.constFind(layoutId);
    if (it == m_layouts.constEnd()) {
        return nullptr;
    }
    return &it.value();
}

void LayoutManager::putDocument(LayoutDocument doc)
{
    if (doc.id.isEmpty()) {
        return;
    }
    m_layouts.insert(doc.id, std::move(doc));
}

QStringList LayoutManager::layoutIds() const
{
    QStringList ids = m_layouts.keys();
    ids.sort(Qt::CaseInsensitive);
    return ids;
}

} // namespace gazer
