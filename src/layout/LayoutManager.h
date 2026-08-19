#pragma once

#include "layout/LayoutTypes.h"

#include <QObject>
#include <QHash>
#include <QString>
#include <QStringList>

namespace gazer {

/// Document catalog: loads/caches layout JSON by id.
class LayoutManager final : public QObject {
    Q_OBJECT

public:
    explicit LayoutManager(QObject* parent = nullptr);

    void setLayoutsDirectory(const QString& dir);
    [[nodiscard]] QString layoutsDirectory() const { return m_dir; }

    [[nodiscard]] int scanDirectory();
    [[nodiscard]] bool loadFile(const QString& path, QString* error = nullptr);
    [[nodiscard]] bool loadLayoutId(const QString& layoutId, QString* error = nullptr);

    [[nodiscard]] const LayoutDocument* document(const QString& layoutId) const;
    [[nodiscard]] bool hasLayout(const QString& layoutId) const;
    /// Insert or replace a parsed document (does not write disk).
    void putDocument(LayoutDocument doc);
    [[nodiscard]] QStringList layoutIds() const;

signals:
    void layoutLoadFailed(const QString& layoutId, const QString& error);

private:
    QString m_dir;
    QHash<QString, LayoutDocument> m_layouts;
};

} // namespace gazer
