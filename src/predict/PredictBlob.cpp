#include "predict/PredictInternal.h"

#include <QDataStream>
#include <QFile>
#include <QSaveFile>

#include <algorithm>

namespace gazer {
namespace {

constexpr quint32 kVersion = 2;
constexpr quint32 kUserVersion = 1;
constexpr quint8 kFlagProper = 1;
constexpr quint8 kFlagSentence = 2;
constexpr quint8 kFlagEmbedding = 4;

bool writeUtf8(QDataStream& out, const QString& text)
{
    const QByteArray bytes = text.toUtf8();
    if (bytes.size() > 65535) {
        return false;
    }
    out << quint16(bytes.size());
    return out.writeRawData(bytes.constData(), bytes.size()) == bytes.size();
}

QString readUtf8(QDataStream& in, bool* ok)
{
    quint16 n = 0;
    in >> n;
    QByteArray bytes(int(n), Qt::Uninitialized);
    if (n > 0 && in.readRawData(bytes.data(), int(n)) != int(n)) {
        *ok = false;
        return {};
    }
    if (in.status() != QDataStream::Ok) {
        *ok = false;
        return {};
    }
    return QString::fromUtf8(bytes);
}

void writeWeights(QDataStream& out, const WordPredictor::Weights& w)
{
    out << float(w.lambda3) << float(w.lambda2) << float(w.lambda1) << float(w.lambdaAnchor)
        << float(w.lambdaCache) << float(w.lambdaUser) << float(w.beta) << float(w.margin)
        << float(w.costTranspose) << float(w.costSubNeighbor) << float(w.costSubDiag)
        << float(w.costSubOther) << float(w.costInsertDelete) << float(w.costRepeat)
        << float(w.costLimit) << float(w.anchorGain);
}

bool readWeights(QDataStream& in, WordPredictor::Weights& w)
{
    float v[16];
    for (float& x : v) {
        in >> x;
    }
    if (in.status() != QDataStream::Ok) {
        return false;
    }
    w.lambda3 = v[0];
    w.lambda2 = v[1];
    w.lambda1 = v[2];
    w.lambdaAnchor = v[3];
    w.lambdaCache = v[4];
    w.lambdaUser = v[5];
    w.beta = v[6];
    w.margin = v[7];
    w.costTranspose = v[8];
    w.costSubNeighbor = v[9];
    w.costSubDiag = v[10];
    w.costSubOther = v[11];
    w.costInsertDelete = v[12];
    w.costRepeat = v[13];
    w.costLimit = v[14];
    w.anchorGain = v[15];
    return true;
}

void writeConts(QDataStream& out, const QVector<PredictCont>& rows)
{
    out << quint16(std::min(int(rows.size()), 65535));
    const int n = std::min(int(rows.size()), 65535);
    for (int i = 0; i < n; ++i) {
        out << qint32(rows[i].word) << rows[i].p;
    }
}

bool readConts(QDataStream& in, QVector<PredictCont>& rows)
{
    quint16 n = 0;
    in >> n;
    rows.resize(n);
    for (int i = 0; i < n; ++i) {
        qint32 word = 0;
        float p = 0;
        in >> word >> p;
        rows[i].word = word;
        rows[i].p = p;
    }
    return in.status() == QDataStream::Ok;
}

} // namespace

bool WordPredictor::saveFile(const QString& path, QString* error) const
{
    auto fail = [&](const QString& msg) {
        if (error) {
            *error = msg;
        }
        return false;
    };
    if (!d || d->words.isEmpty()) {
        return fail(QStringLiteral("No model to save"));
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return fail(QStringLiteral("Cannot write %1").arg(path));
    }
    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    out.setVersion(QDataStream::Qt_6_0);
    out.writeRawData("GZPR", 4);
    out << kVersion;
    writeWeights(out, d->w);
    out << quint16(d->embDim) << quint32(d->words.size());
    for (const LexWord& w : d->words) {
        if (!writeUtf8(out, w.text)) {
            return fail(QStringLiteral("Word too long"));
        }
        const bool hasEmb = d->embDim > 0 && w.emb.size() == d->embDim;
        quint8 flags = 0;
        if (w.proper) {
            flags |= kFlagProper;
        }
        if (w.inSentences) {
            flags |= kFlagSentence;
        }
        if (hasEmb) {
            flags |= kFlagEmbedding;
        }
        out << w.uni << flags;
        if (hasEmb) {
            for (qint8 v : w.emb) {
                out << quint8(v);
            }
        }
    }
    out << quint32(d->bigram.size());
    for (auto it = d->bigram.cbegin(); it != d->bigram.cend(); ++it) {
        out << qint32(it.key());
        writeConts(out, it.value());
    }
    out << quint32(d->trigram.size());
    for (auto it = d->trigram.cbegin(); it != d->trigram.cend(); ++it) {
        const quint64 key = it.key();
        out << qint32(quint32(key >> 32)) << qint32(quint32(key));
        writeConts(out, it.value());
    }
    out << quint32(d->triggers.size());
    for (auto it = d->triggers.cbegin(); it != d->triggers.cend(); ++it) {
        out << qint32(it.key());
        writeConts(out, it.value());
    }
    if (out.status() != QDataStream::Ok || !file.commit()) {
        return fail(QStringLiteral("Failed writing %1: %2").arg(path, file.errorString()));
    }
    return true;
}

bool WordPredictor::loadFile(const QString& path, QString* error)
{
    auto fail = [&](const QString& msg) {
        if (error) {
            *error = msg;
        }
        return false;
    };
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(QStringLiteral("Cannot open %1").arg(path));
    }
    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);
    in.setVersion(QDataStream::Qt_6_0);
    char magic[4] = {};
    if (in.readRawData(magic, 4) != 4 || QByteArray(magic, 4) != "GZPR") {
        return fail(QStringLiteral("Not a prediction model"));
    }
    quint32 version = 0;
    in >> version;
    if (version != kVersion) {
        return fail(QStringLiteral("Unsupported prediction model version"));
    }
    PredictModel next;
    if (!readWeights(in, next.w)) {
        return fail(QStringLiteral("Truncated weights"));
    }
    quint16 embDim = 0;
    quint32 wordCount = 0;
    in >> embDim >> wordCount;
    next.embDim = embDim;
    if (wordCount > 80000) {
        return fail(QStringLiteral("Vocabulary is too large"));
    }
    next.words.reserve(int(wordCount));
    bool ok = true;
    for (quint32 i = 0; i < wordCount; ++i) {
        LexWord w;
        w.text = readUtf8(in, &ok);
        float uni = 0;
        quint8 flags = 0;
        in >> uni >> flags;
        w.uni = uni;
        w.proper = (flags & kFlagProper) != 0;
        w.inSentences = (flags & kFlagSentence) != 0;
        if ((flags & kFlagEmbedding) != 0) {
            if (embDim == 0) {
                return fail(QStringLiteral("Embedding flag without a width"));
            }
            w.emb.resize(embDim);
            for (int axis = 0; axis < embDim; ++axis) {
                quint8 b = 0;
                in >> b;
                w.emb[axis] = qint8(b);
            }
        }
        if (!ok || w.text.isEmpty() || next.idOf.contains(w.text)) {
            return fail(QStringLiteral("Bad lexicon entry"));
        }
        next.idOf.insert(w.text, next.words.size());
        next.words.push_back(std::move(w));
    }
    quint32 biCount = 0;
    in >> biCount;
    for (quint32 i = 0; i < biCount; ++i) {
        qint32 left = 0;
        in >> left;
        QVector<PredictCont> rows;
        if (!readConts(in, rows)) {
            return fail(QStringLiteral("Bad bigram table"));
        }
        if (left >= 0 && left < next.words.size()) {
            next.bigram.insert(left, rows);
        }
    }
    quint32 triCount = 0;
    in >> triCount;
    for (quint32 i = 0; i < triCount; ++i) {
        qint32 a = 0;
        qint32 b = 0;
        in >> a >> b;
        QVector<PredictCont> rows;
        if (!readConts(in, rows)) {
            return fail(QStringLiteral("Bad trigram table"));
        }
        if (a >= 0 && b >= 0) {
            next.trigram.insert(predict_detail::triKey(a, b), rows);
        }
    }
    quint32 trigCount = 0;
    in >> trigCount;
    for (quint32 i = 0; i < trigCount; ++i) {
        qint32 src = 0;
        in >> src;
        QVector<PredictCont> rows;
        if (!readConts(in, rows)) {
            return fail(QStringLiteral("Bad trigger table"));
        }
        if (src >= 0 && src < next.words.size()) {
            next.triggers.insert(src, rows);
        }
    }
    if (in.status() != QDataStream::Ok || next.words.isEmpty()) {
        return fail(QStringLiteral("Truncated prediction model"));
    }
    predict_detail::rebuildTrie(next);
    predict_detail::rebuildPhonetic(next);
    predict_detail::rebuildTopUni(next);
    *d = std::move(next);
    return true;
}

bool WordPredictor::loadUser(const QString& path)
{
    if (!d || path.isEmpty()) {
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);
    in.setVersion(QDataStream::Qt_6_0);
    char magic[4] = {};
    if (in.readRawData(magic, 4) != 4 || QByteArray(magic, 4) != "GZPU") {
        return false;
    }
    quint32 version = 0;
    in >> version;
    if (version != kUserVersion) {
        return false;
    }
    predict_detail::clearUser(*d);
    quint32 total = 0;
    quint32 nUni = 0;
    in >> total >> nUni;
    bool ok = true;
    for (quint32 i = 0; i < nUni; ++i) {
        const QString w = readUtf8(in, &ok);
        quint32 count = 0;
        in >> count;
        if (!ok || w.isEmpty() || count == 0) {
            continue;
        }
        d->userUni.insert(w, int(count));
        predict_detail::ensureOov(*d, w);
    }
    quint32 nBi = 0;
    in >> nBi;
    for (quint32 i = 0; i < nBi; ++i) {
        const QString a = readUtf8(in, &ok);
        const QString b = readUtf8(in, &ok);
        quint32 count = 0;
        in >> count;
        if (!ok || a.isEmpty() || b.isEmpty() || count == 0) {
            continue;
        }
        d->userBi.insert(a + QLatin1Char('\t') + b, int(count));
    }
    d->userTotal = int(total);
    return in.status() == QDataStream::Ok;
}

bool WordPredictor::saveUser(const QString& path) const
{
    if (!d || path.isEmpty()) {
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);
    out.setVersion(QDataStream::Qt_6_0);
    out.writeRawData("GZPU", 4);
    out << kUserVersion << quint32(std::max(0, d->userTotal)) << quint32(d->userUni.size());
    for (auto it = d->userUni.cbegin(); it != d->userUni.cend(); ++it) {
        if (!writeUtf8(out, it.key())) {
            return false;
        }
        out << quint32(std::max(0, it.value()));
    }
    QVector<QPair<QString, int>> pairs;
    pairs.reserve(d->userBi.size());
    for (auto it = d->userBi.cbegin(); it != d->userBi.cend(); ++it) {
        if (it.key().contains(QLatin1Char('\t'))) {
            pairs.push_back({it.key(), it.value()});
        }
    }
    out << quint32(pairs.size());
    for (const auto& pair : pairs) {
        const int tab = pair.first.indexOf(QLatin1Char('\t'));
        if (!writeUtf8(out, pair.first.left(tab)) || !writeUtf8(out, pair.first.mid(tab + 1))) {
            return false;
        }
        out << quint32(std::max(0, pair.second));
    }
    return out.status() == QDataStream::Ok && file.commit();
}

} // namespace gazer
