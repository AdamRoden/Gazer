#include "predict/CharPrior.h"

#include <QDataStream>
#include <QFile>
#include <QSaveFile>

#include <algorithm>
#include <limits>

namespace gazer {
namespace {

constexpr quint32 kVersion = 1;
const QString kPunct = QStringLiteral(".,?!;:'\"-[]()/=+\\@#$%^&*_<>|~`");

} // namespace

QChar CharPrior::canonicalize(QChar c)
{
    if (c.isNull()) {
        return {};
    }
    if (c.isLetter()) {
        return c.toLower();
    }
    if (c.isDigit() || c == QLatin1Char(' ')) {
        return c;
    }
    if (kPunct.contains(c)) {
        return c;
    }
    return {};
}

ushort CharPrior::code(QChar c)
{
    const QChar n = canonicalize(c);
    return n.isNull() ? ushort(0) : n.unicode();
}

quint64 CharPrior::pack(ushort prev2, ushort prev1, int order)
{
    return (quint64(order) << 32) | (quint64(prev2) << 16) | quint64(prev1);
}

void CharPrior::add(QHash<quint64, Bucket>& table, ushort prev2, ushort prev1, ushort next,
                    quint32 weight, bool unigram)
{
    if (next == 0 || weight == 0) {
        return;
    }
    const auto bump = [&](ushort a, ushort b, int order) {
        Bucket& bucket = table[pack(a, b, order)];
        quint32& count = bucket.next[next];
        const quint32 room = std::numeric_limits<quint32>::max() - count;
        const quint32 addN = std::min(weight, room);
        count += addN;
        const quint32 totalRoom = std::numeric_limits<quint32>::max() - bucket.total;
        bucket.total += std::min(addN, totalRoom);
    };
    bump(prev2, prev1, 2);
    bump(0, prev1, 1);
    if (unigram) {
        bump(0, 0, 0);
    }
}

const CharPrior::Bucket* CharPrior::findBucket(const QHash<quint64, Bucket>& table, quint64 key) const
{
    const auto it = table.constFind(key);
    if (it == table.cend() || it->total == 0) {
        return nullptr;
    }
    return &(*it);
}

bool CharPrior::build(const QStringList& lines, const QVector<WordUse>& usage, QString* error)
{
    m_ship.clear();
    const auto fail = [&](const QString& msg) {
        if (error) {
            *error = msg;
        }
        return false;
    };
    const auto addStream = [&](const QString& text, quint32 weight) {
        ushort prev2 = 0;
        ushort prev1 = 0;
        for (const QChar ch : text) {
            const ushort next = code(ch);
            if (next == 0) {
                continue;
            }
            add(m_ship, prev2, prev1, next, weight);
            prev2 = prev1;
            prev1 = next;
        }
    };
    for (const QString& line : lines) {
        QString stream;
        bool gap = false;
        for (const QChar ch : line) {
            if (ch.isSpace()) {
                gap = !stream.isEmpty();
                continue;
            }
            const QChar n = canonicalize(ch);
            if (n.isNull()) {
                continue;
            }
            if (gap) {
                stream.append(QLatin1Char(' '));
                gap = false;
            }
            stream.append(n);
        }
        if (!stream.isEmpty() && stream.back() != QLatin1Char(' ')) {
            stream.append(QLatin1Char(' '));
        }
        if (!stream.isEmpty()) {
            addStream(stream, 1);
        }
    }

    for (const WordUse& row : usage) {
        if (row.count <= 0 || row.word.isEmpty()) {
            continue;
        }
        QString stream;
        ushort first = 0;
        bool any = false;
        for (const QChar ch : row.word) {
            const ushort next = code(ch);
            if (next == 0 || next == ushort(' ')) {
                continue;
            }
            if (!any) {
                first = next;
            }
            any = true;
            stream.append(QChar(next));
        }
        if (!any) {
            continue;
        }
        stream.append(QLatin1Char(' '));
        const quint32 weight = static_cast<quint32>(row.count);
        addStream(stream, weight);
        add(m_ship, 0, code(QLatin1Char(' ')), first, weight, false);
    }

    if (m_ship.isEmpty()) {
        return fail(QStringLiteral("No character counts"));
    }
    return true;
}

bool CharPrior::writeBuckets(QDataStream& out, const QHash<quint64, Bucket>& table) const
{
    out << quint32(table.size());
    for (auto it = table.cbegin(); it != table.cend(); ++it) {
        const quint64 key = it.key();
        const ushort prev2 = ushort((key >> 16) & 0xffff);
        const ushort prev1 = ushort(key & 0xffff);
        const quint8 order = quint8((key >> 32) & 0xff);
        out << prev2 << prev1 << order << it->total << quint32(it->next.size());
        for (auto n = it->next.cbegin(); n != it->next.cend(); ++n) {
            out << n.key() << n.value();
        }
    }
    return out.status() == QDataStream::Ok;
}

bool CharPrior::readBuckets(QDataStream& in, QHash<quint64, Bucket>& table)
{
    quint32 buckets = 0;
    in >> buckets;
    if (in.status() != QDataStream::Ok || buckets > 2000000u) {
        return false;
    }
    table.clear();
    table.reserve(int(buckets));
    for (quint32 i = 0; i < buckets; ++i) {
        ushort prev2 = 0;
        ushort prev1 = 0;
        quint8 order = 0;
        quint32 total = 0;
        quint32 nNext = 0;
        in >> prev2 >> prev1 >> order >> total >> nNext;
        if (in.status() != QDataStream::Ok || nNext > 10000u) {
            return false;
        }
        Bucket bucket;
        bucket.total = total;
        for (quint32 k = 0; k < nNext; ++k) {
            ushort sym = 0;
            quint32 count = 0;
            in >> sym >> count;
            if (sym != 0 && count != 0) {
                bucket.next.insert(sym, count);
            }
        }
        if (in.status() != QDataStream::Ok) {
            return false;
        }
        if (bucket.total > 0 && !bucket.next.isEmpty()) {
            table.insert(pack(prev2, prev1, int(order)), bucket);
        }
    }
    return in.status() == QDataStream::Ok;
}

bool CharPrior::writeBlob(const QString& path, const char* magic, const QHash<quint64, Bucket>& table,
                          QString* error) const
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = QStringLiteral("Cannot write %1").arg(path);
        }
        return false;
    }
    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    out.setVersion(QDataStream::Qt_6_0);
    out.writeRawData(magic, 4);
    out << kVersion;
    if (!writeBuckets(out, table) || !file.commit()) {
        if (error) {
            *error = QStringLiteral("Failed writing %1").arg(path);
        }
        return false;
    }
    return true;
}

bool CharPrior::readBlob(const QString& path, const char* magic, QHash<quint64, Bucket>& table,
                         QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Cannot open %1").arg(path);
        }
        return false;
    }
    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);
    in.setVersion(QDataStream::Qt_6_0);
    char got[4] = {};
    if (in.readRawData(got, 4) != 4 || QByteArray(got, 4) != QByteArray(magic, 4)) {
        if (error) {
            *error = QStringLiteral("Not a character prior");
        }
        return false;
    }
    quint32 version = 0;
    in >> version;
    if (version != kVersion || !readBuckets(in, table)) {
        if (error) {
            *error = QStringLiteral("Bad character prior");
        }
        return false;
    }
    return true;
}

bool CharPrior::saveFile(const QString& path, QString* error) const
{
    return writeBlob(path, "GZCH", m_ship, error);
}

bool CharPrior::loadFile(const QString& path, QString* error)
{
    return readBlob(path, "GZCH", m_ship, error);
}

bool CharPrior::saveUser(const QString& path, QString* error) const
{
    return writeBlob(path, "GZCU", m_user, error);
}

bool CharPrior::loadUser(const QString& path)
{
    QString ignored;
    if (!readBlob(path, "GZCU", m_user, &ignored)) {
        m_user.clear();
        return false;
    }
    return true;
}

bool CharPrior::bumpUser(ushort prev2, ushort prev1, ushort next, int sign)
{
    if (next == 0 || (sign != 1 && sign != -1)) {
        return false;
    }
    if (sign > 0) {
        add(m_user, prev2, prev1, next, 1);
        return true;
    }
    const Bucket* order2 = findBucket(m_user, pack(prev2, prev1, 2));
    if (!order2 || !order2->next.contains(next)) {
        return false;
    }
    const auto step = [&](ushort a, ushort b, int order) {
        const auto it = m_user.find(pack(a, b, order));
        if (it == m_user.end() || it->total == 0) {
            return;
        }
        const auto n = it->next.find(next);
        if (n == it->next.end() || n.value() == 0) {
            return;
        }
        n.value() -= 1;
        it->total -= 1;
        if (n.value() == 0) {
            it->next.erase(n);
        }
        if (it->total == 0 || it->next.isEmpty()) {
            m_user.erase(it);
        }
    };
    step(prev2, prev1, 2);
    step(0, prev1, 1);
    step(0, 0, 0);
    return true;
}

void CharPrior::observe(QChar prev2, QChar prev1, QChar next)
{
    const ushort n = code(next);
    if (n == 0) {
        return;
    }
    bumpUser(code(prev2), code(prev1), n, +1);
}

bool CharPrior::undo(QChar prev2, QChar prev1, QChar next)
{
    const ushort n = code(next);
    if (n == 0) {
        return false;
    }
    return bumpUser(code(prev2), code(prev1), n, -1);
}

bool CharPrior::hasContext(ushort prev2, ushort prev1, int order) const
{
    const quint64 key = pack(prev2, prev1, order);
    return findBucket(m_ship, key) != nullptr || findBucket(m_user, key) != nullptr;
}

double CharPrior::probabilityAt(ushort prev2, ushort prev1, int order, ushort next, bool* seen) const
{
    const Bucket* ship = findBucket(m_ship, pack(prev2, prev1, order));
    const Bucket* user = findBucket(m_user, pack(prev2, prev1, order));
    const quint32 ns = ship ? ship->total : 0;
    const quint32 nu = user ? user->total : 0;
    if (ns + nu == 0) {
        *seen = false;
        return 0;
    }
    *seen = true;
    const double ps = ns ? double(ship->next.value(next)) / double(ns) : 0;
    const double pu = nu ? double(user->next.value(next)) / double(nu) : 0;
    if (ns == 0) {
        return pu;
    }
    if (nu == 0) {
        return ps;
    }
    const double lambda = double(nu) / double(nu + kPersonalHalf);
    return (1.0 - lambda) * ps + lambda * pu;
}

double CharPrior::probability(QChar prev2, QChar prev1, QChar next) const
{
    const ushort n = code(next);
    if (n == 0) {
        return 0;
    }
    const ushort a = code(prev2);
    const ushort b = code(prev1);
    bool seen = false;
    double p = probabilityAt(a, b, 2, n, &seen);
    if (seen) {
        return p;
    }
    p = probabilityAt(0, b, 1, n, &seen);
    if (seen) {
        return kBackoff * p;
    }
    p = probabilityAt(0, 0, 0, n, &seen);
    if (seen) {
        return kBackoff * kBackoff * p;
    }
    return 0;
}

QVector<CharPrior::Mass> CharPrior::massesAt(ushort prev2, ushort prev1, int order) const
{
    QHash<ushort, double> mass;
    const auto take = [&](const Bucket* bucket) {
        if (!bucket) {
            return;
        }
        for (auto it = bucket->next.cbegin(); it != bucket->next.cend(); ++it) {
            mass.insert(it.key(), 0);
        }
    };
    take(findBucket(m_ship, pack(prev2, prev1, order)));
    take(findBucket(m_user, pack(prev2, prev1, order)));
    double sum = 0;
    for (auto it = mass.begin(); it != mass.end(); ++it) {
        bool rowSeen = false;
        const double p = probabilityAt(prev2, prev1, order, it.key(), &rowSeen);
        it.value() = p;
        sum += p;
    }
    QVector<Mass> out;
    if (sum <= 0) {
        return out;
    }
    out.reserve(mass.size());
    for (auto it = mass.cbegin(); it != mass.cend(); ++it) {
        if (it.value() <= 0) {
            continue;
        }
        Mass row;
        row.symbol = QChar(it.key());
        row.mass = it.value() / sum;
        out.push_back(row);
    }
    return out;
}

QVector<CharPrior::Mass> CharPrior::distribution(QChar prev2, QChar prev1) const
{
    const ushort a = code(prev2);
    const ushort b = code(prev1);
    if (hasContext(a, b, 2)) {
        return massesAt(a, b, 2);
    }
    if (hasContext(0, b, 1)) {
        return massesAt(0, b, 1);
    }
    if (!hasContext(0, 0, 0)) {
        return {};
    }
    return massesAt(0, 0, 0);
}

QVector<CharPrior::Mass> CharPrior::unigram() const
{
    if (!hasContext(0, 0, 0)) {
        return {};
    }
    return massesAt(0, 0, 0);
}

} // namespace gazer
