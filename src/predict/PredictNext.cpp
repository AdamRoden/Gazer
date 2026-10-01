#include "predict/PredictInternal.h"

#include <QHash>

#include <algorithm>

namespace gazer {
namespace {

double massOfWord(const PredictModel& m, const QString& text, float uni)
{
    double mass = double(uni);
    if (m.userTotal > 0) {
        mass += double(m.userUni.value(text)) / double(m.userTotal);
    }
    return mass;
}

void addNext(QHash<QChar, double>& mass, const QString& text, const QString& prefix, double weight)
{
    if (weight <= 0 || text.isEmpty() || !text.startsWith(prefix)) {
        return;
    }
    if (text.size() == prefix.size()) {
        mass[QLatin1Char(' ')] += weight;
        return;
    }
    const QChar next = text.at(prefix.size());
    if (next.isLetter()) {
        mass[next.toLower()] += weight;
    } else if (next.isDigit() || next == QLatin1Char('\'') || next == QLatin1Char('-')) {
        mass[next] += weight;
    }
}

} // namespace

QVector<WordPredictor::NextChar> WordPredictor::nextCharMass(const Query& query) const
{
    QVector<NextChar> out;
    if (!d || (d->words.isEmpty() && d->userUni.isEmpty())) {
        return out;
    }
    const QString prefix = predict_detail::normWord(query.typed);
    QHash<QChar, double> mass;

    if (!prefix.isEmpty()) {
        for (const LexWord& word : d->words) {
            addNext(mass, word.text, prefix, massOfWord(*d, word.text, word.uni));
        }
        for (auto it = d->userUni.cbegin(); it != d->userUni.cend(); ++it) {
            if (it.value() <= 0 || predict_detail::lexiconId(*d, it.key()) >= 0) {
                continue;
            }
            const double weight = double(it.value()) / double(std::max(1, d->userTotal));
            addNext(mass, it.key(), prefix, weight);
        }
    } else if (!query.sentenceWords.isEmpty()) {
        const QString prev = predict_detail::normWord(query.sentenceWords.last());
        const int prevId = predict_detail::lexiconId(*d, prev);
        if (prevId >= 0) {
            const auto bi = d->bigram.constFind(prevId);
            if (bi != d->bigram.cend()) {
                for (const PredictCont& row : bi.value()) {
                    const QString& text = predict_detail::wordText(*d, row.word);
                    if (!text.isEmpty() && text.front().isLetter()) {
                        mass[text.front().toLower()] += double(row.p);
                    }
                }
            }
        }
        const int prevCount = d->userUni.value(prev);
        if (prevCount > 0) {
            const QString stem = prev + QLatin1Char('\t');
            for (auto it = d->userBi.cbegin(); it != d->userBi.cend(); ++it) {
                if (!it.key().startsWith(stem) || it.value() <= 0) {
                    continue;
                }
                const QString word = it.key().mid(stem.size());
                if (!word.isEmpty() && word.front().isLetter()) {
                    mass[word.front().toLower()] += double(it.value()) / double(prevCount);
                }
            }
        }
    }

    if (mass.isEmpty() && prefix.isEmpty()) {
        for (const LexWord& word : d->words) {
            addNext(mass, word.text, QString(), massOfWord(*d, word.text, word.uni));
        }
        for (auto it = d->userUni.cbegin(); it != d->userUni.cend(); ++it) {
            if (it.value() <= 0 || predict_detail::lexiconId(*d, it.key()) >= 0) {
                continue;
            }
            addNext(mass, it.key(), QString(), double(it.value()) / double(std::max(1, d->userTotal)));
        }
    }

    out.reserve(mass.size());
    for (auto it = mass.cbegin(); it != mass.cend(); ++it) {
        if (it.value() <= 0) {
            continue;
        }
        NextChar row;
        row.symbol = it.key();
        row.mass = it.value();
        out.push_back(row);
    }
    return out;
}

} // namespace gazer
