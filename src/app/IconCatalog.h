#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace gazer {

struct IconEntry {
    QString name;
    QString hay;
    int popularity = 0;
};

/// Material Symbols catalog used by the Voice icon studio.
/// Tries fonts.google.com, then a name mirror, then a short offline list.
class IconCatalog : public QObject {
    Q_OBJECT

public:
    explicit IconCatalog(QObject* parent = nullptr);
    ~IconCatalog() override;

    /// Start a fetch if one is not already running. Safe to call more than once.
    void ensureLoaded();

    /// Up to 120 names. An empty query is the most popular icons.
    [[nodiscard]] QStringList search(const QString& query) const;

    /// Caption for the search cell.
    [[nodiscard]] QString statusLabel() const;

    /// Fetch SVGs for catalog names that are not already cached.
    void requestIcons(const QStringList& names);

signals:
    void catalogChanged();
    void iconsReady();

private:
    void loadDiskThenNetwork();
    void fetchGoogle();
    void fetchGithub();
    void install(QVector<IconEntry> entries, const QString& source);
    void pump();
    void fetchSvg(const QString& name);

    struct Store;
    Store* m_store = nullptr;
    bool m_alive = true;
};

} // namespace gazer
