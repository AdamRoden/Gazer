#include "layout/PageCatalog.h"

#include "layout/PageCompose.h"
#include "layout/PageLoader.h"
#include "utils/Log.h"

#include <QDir>
#include <QFileInfo>
#include <utility>

namespace gazer {

PageCatalog::PageCatalog(QObject* parent)
    : QObject(parent)
{
}

void PageCatalog::setDirectory(const QString& dir)
{
    m_dir = dir;
}

void PageCatalog::setUserDirectory(const QString& dir)
{
    m_userDir = dir;
}

QString PageCatalog::resolvePath(const QString& id, const QString& userDir, const QString& shippedDir)
{
    if (id.isEmpty()) {
        return {};
    }
    if (!userDir.isEmpty()) {
        const QString user = QDir(userDir).filePath(id + QStringLiteral(".xml"));
        if (QFileInfo::exists(user)) {
            return user;
        }
    }
    if (!shippedDir.isEmpty()) {
        const QString shipped = QDir(shippedDir).filePath(id + QStringLiteral(".xml"));
        if (QFileInfo::exists(shipped)) {
            return shipped;
        }
    }
    return {};
}

int PageCatalog::scan()
{
    m_pages.clear();
    m_inlined.clear();
    QHash<QString, PageDocument> docs;
    auto ingest = [&](const QString& dir) {
        if (dir.isEmpty()) {
            return;
        }
        QDir d(dir);
        if (!d.exists()) {
            return;
        }
        const QFileInfoList files = d.entryInfoList({QStringLiteral("*.xml")}, QDir::Files, QDir::Name);
        for (const QFileInfo& fi : files) {
            PageDocument doc;
            QString err;
            if (!PageLoader::loadFromFile(fi.absoluteFilePath(), doc, &err) || !doc.isValid()) {
                GAZER_WARN << "Skip page" << fi.fileName() << ":" << err;
                continue;
            }
            PageCatalogEntry e;
            e.id = doc.id;
            e.name = doc.name;
            e.path = fi.absoluteFilePath();
            m_pages.insert(e.id, e);
            docs.insert(doc.id, std::move(doc));
        }
    };
    ingest(m_dir);
    ingest(m_userDir);
    for (auto it = docs.cbegin(); it != docs.cend(); ++it) {
        PageCompose::collectSlotTargets(it.value(), m_inlined);
    }
    GAZER_INFO << "Scanned pages directory:" << m_dir << "user:" << m_userDir
               << "loaded:" << m_pages.size();
    return m_pages.size();
}

bool PageCatalog::has(const QString& id) const
{
    return !pathFor(id).isEmpty();
}

QStringList PageCatalog::ids() const
{
    QStringList out = m_pages.keys();
    out.sort(Qt::CaseInsensitive);
    return out;
}

QString PageCatalog::nameFor(const QString& id) const
{
    return m_pages.value(id).name;
}

QString PageCatalog::pathFor(const QString& id) const
{
    const QString live = resolvePath(id, m_userDir, m_dir);
    if (!live.isEmpty()) {
        return live;
    }
    return m_pages.value(id).path;
}

} // namespace gazer
