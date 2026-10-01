#pragma once

#include <QChar>
#include <QHash>
#include <QString>
#include <QVector>

class QDataStream;

namespace gazer {

/// Order-2 next-character counts. A shipped English prior plus personal counts.
/// Gaze samples do not call this; rebuild a distribution when the typed context changes.
class CharPrior {
public:
    static constexpr int kPersonalHalf = 40;
    static constexpr double kBackoff = 0.4;

    struct WordUse {
        QString word;
        int count = 0;
    };

    struct Mass {
        QChar symbol;
        double mass = 0;
    };

    /// Letter, digit, space, or keyboard punctuation. Letters are lowercased.
    /// Null when the character is not part of the alphabet.
    [[nodiscard]] static QChar canonicalize(QChar c);

    /// Sentences (one utterance per line) plus a usage-ranked word list.
    /// Dictionary words are weighted by `count` so frequent words dominate.
    bool build(const QStringList& lines, const QVector<WordUse>& usage, QString* error = nullptr);

    bool saveFile(const QString& path, QString* error = nullptr) const;
    bool loadFile(const QString& path, QString* error = nullptr);
    bool saveUser(const QString& path, QString* error = nullptr) const;
    bool loadUser(const QString& path);

    /// `prev2` / `prev1` null means a boundary (start of stream, or no character yet).
    void observe(QChar prev2, QChar prev1, QChar next);
    /// Reverse one `observe`. False when that count is already zero.
    bool undo(QChar prev2, QChar prev1, QChar next);

    /// Stupid backoff: order 2, else 0.4 × order 1, else 0.16 × unigram.
    /// Personal counts at a context replace the shipped prior by n / (n + 40).
    [[nodiscard]] double probability(QChar prev2, QChar prev1, QChar next) const;

    /// Normalized masses at the longest context that has counts. Empty when nothing was trained.
    [[nodiscard]] QVector<Mass> distribution(QChar prev2, QChar prev1) const;
    /// Normalized unigram, ignoring the boundary context.
    [[nodiscard]] QVector<Mass> unigram() const;

private:
    struct Bucket {
        QHash<ushort, quint32> next;
        quint32 total = 0;
    };

    [[nodiscard]] static ushort code(QChar c);
    [[nodiscard]] static quint64 pack(ushort prev2, ushort prev1, int order);
    /// `unigram` is false for the extra "after space → first letter" edge, which the
    /// word stream has already counted in the unigram.
    void add(QHash<quint64, Bucket>& table, ushort prev2, ushort prev1, ushort next, quint32 weight,
             bool unigram = true);
    bool bumpUser(ushort prev2, ushort prev1, ushort next, int sign);
    [[nodiscard]] bool writeBuckets(QDataStream& out, const QHash<quint64, Bucket>& table) const;
    [[nodiscard]] bool readBuckets(QDataStream& in, QHash<quint64, Bucket>& table);
    [[nodiscard]] bool writeBlob(const QString& path, const char* magic,
                                 const QHash<quint64, Bucket>& table, QString* error) const;
    [[nodiscard]] bool readBlob(const QString& path, const char* magic,
                                QHash<quint64, Bucket>& table, QString* error);
    [[nodiscard]] const Bucket* findBucket(const QHash<quint64, Bucket>& table, quint64 key) const;
    [[nodiscard]] bool hasContext(ushort prev2, ushort prev1, int order) const;
    [[nodiscard]] double probabilityAt(ushort prev2, ushort prev1, int order, ushort next,
                                       bool* seen) const;
    [[nodiscard]] QVector<Mass> massesAt(ushort prev2, ushort prev1, int order) const;

    QHash<quint64, Bucket> m_ship;
    QHash<quint64, Bucket> m_user;
};

} // namespace gazer
