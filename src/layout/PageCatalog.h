#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>

namespace gazer {

struct PageCatalogEntry {
    QString id;
    QString name;
    QString path;
};

/// On-disk XML pages. User copies under AppData win over shipped files with the same id.
class PageCatalog final : public QObject {
    Q_OBJECT

public:
    explicit PageCatalog(QObject* parent = nullptr);

    void setDirectory(const QString& dir);
    void setUserDirectory(const QString& dir);
    [[nodiscard]] QString directory() const { return m_dir; }
    [[nodiscard]] QString userDirectory() const { return m_userDir; }

    [[nodiscard]] int scan();
    [[nodiscard]] bool has(const QString& id) const;
    [[nodiscard]] QStringList ids() const;
    [[nodiscard]] QString nameFor(const QString& id) const;
    /// User file if it exists, else shipped. Does not require scan().
    [[nodiscard]] QString pathFor(const QString& id) const;

    [[nodiscard]] static QString resolvePath(const QString& id, const QString& userDir,
                                             const QString& shippedDir);

private:
    QString m_dir;
    QString m_userDir;
    QHash<QString, PageCatalogEntry> m_pages;
};

} // namespace gazer
