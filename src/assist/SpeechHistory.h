#pragma once

#include <QString>
#include <QVector>
#include <cstdint>

namespace gazer {

struct SpeechHistoryItem {
    QString id;
    QString phrase;
    QString modelId;
    QString voiceId;
    QString backend;
    QString atIso;
};

/// Last 50 composed utterances. Metadata in history.json; optional MPEG in history/.
class SpeechHistory {
public:
    static constexpr int kMaxItems = 50;
    static constexpr qint64 kMaxTotalBytes = 200ll * 1024 * 1024;

    explicit SpeechHistory(QString rootDir = {});

    [[nodiscard]] QString rootDir() const { return m_root; }
    [[nodiscard]] QString jsonPath() const;
    [[nodiscard]] QString filesDir() const;

    bool load(QString* error = nullptr);
    bool save(QString* error = nullptr) const;

    [[nodiscard]] const QVector<SpeechHistoryItem>& items() const { return m_items; }
    [[nodiscard]] const SpeechHistoryItem* find(const QString& id) const;
    [[nodiscard]] QString clipPath(const QString& id) const;
    [[nodiscard]] static bool validId(const QString& id);

    /// Newest first. Copies `mpegPath` into history/ when it exists.
    /// `copiedMpeg` receives the history/ destination when the copy succeeds.
    bool record(const QString& phrase, const QString& backend, const QString& modelId,
                const QString& voiceId, const QString& mpegPath, QString* error = nullptr,
                QString* copiedMpeg = nullptr);

    /// Drop the row and its MPEG. Returns false if `id` is unknown.
    bool remove(const QString& id, QString* error = nullptr);

    void evictOldestHistory();
    /// Drop oldest items until `usedBytes() + extraBytes` fits the 200 MB cap.
    void makeRoom(qint64 extraBytes);
    void setMaxTotalBytes(qint64 n);
    [[nodiscard]] qint64 usedBytes() const;

private:
    static QString makeId();
    void trimToCap();

    QString m_root;
    QVector<SpeechHistoryItem> m_items;
    qint64 m_maxBytes = kMaxTotalBytes;
};

} // namespace gazer
