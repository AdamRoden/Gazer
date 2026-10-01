#include "layout/KeyGravity.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace gazer {
namespace {

constexpr double kCoreInset = 0.25;
constexpr double kStealWidths = 0.22;
constexpr double kStickWidths = 0.08;
/// ln(100) * 0.0105 == 0.22^2, so a 100:1 pair meets the steal distance at full strength.
constexpr double kTemperature = 0.0105;
constexpr double kFloor = 1e-6;

const QString kPunct = QStringLiteral(".,?!;:'\"-[]()/=+\\@#$%^&*_<>|~`");

double outsideDistance2(const QPointF& p, const QRectF& r)
{
    const double dx = p.x() < r.left() ? r.left() - p.x() : (p.x() > r.right() ? p.x() - r.right() : 0);
    const double dy = p.y() < r.top() ? r.top() - p.y() : (p.y() > r.bottom() ? p.y() - r.bottom() : 0);
    return dx * dx + dy * dy;
}

double medianOf(QVector<double> widths)
{
    if (widths.isEmpty()) {
        return 48;
    }
    std::sort(widths.begin(), widths.end());
    const int n = widths.size();
    if (n % 2 == 1) {
        return widths.at(n / 2);
    }
    return 0.5 * (widths.at(n / 2 - 1) + widths.at(n / 2));
}

double probabilityOf(const KeyGravityQuery& query, QChar symbol)
{
    if (!query.probability) {
        return kFloor;
    }
    const double p = query.probability->value(symbol, 0);
    return p > 0 ? p : kFloor;
}

} // namespace

QChar characterSymbol(const PageTarget& target)
{
    for (const PageAction& action : target.actions) {
        if (action.type == PageActionType::Send) {
            const QString key = action.sendKey.trimmed();
            if (key.size() == 1) {
                const QChar c = key.at(0);
                if (c.isLetter()) {
                    return c.toLower();
                }
                if (c.isDigit() || c == QLatin1Char(' ') || kPunct.contains(c)) {
                    return c == QLatin1Char(' ') ? QLatin1Char(' ') : c;
                }
            } else if (key.compare(QLatin1String("space"), Qt::CaseInsensitive) == 0) {
                return QLatin1Char(' ');
            }
        } else if (action.type == PageActionType::Command) {
            if (action.command.trimmed().compare(QLatin1String("space"), Qt::CaseInsensitive) == 0) {
                return QLatin1Char(' ');
            }
        }
    }
    return {};
}

int keyGravityPick(const KeyGravityQuery& query)
{
    if (query.strength <= 0 || query.geometricIndex < 0
        || query.geometricIndex >= query.cells.size()) {
        return query.geometricIndex;
    }
    const KeyGravityCell& geo = query.cells.at(query.geometricIndex);
    if (geo.symbol.isNull()) {
        return query.geometricIndex;
    }
    const QRectF core = geo.rect.adjusted(geo.rect.width() * kCoreInset, geo.rect.height() * kCoreInset,
                                          -geo.rect.width() * kCoreInset, -geo.rect.height() * kCoreInset);
    if (core.isValid() && core.contains(query.gaze)) {
        return query.geometricIndex;
    }

    QVector<double> letters;
    QVector<double> characters;
    for (const KeyGravityCell& cell : query.cells) {
        if (cell.gridId != geo.gridId || cell.symbol.isNull() || cell.rect.width() <= 0) {
            continue;
        }
        characters.push_back(cell.rect.width());
        if (cell.symbol.isLetter()) {
            letters.push_back(cell.rect.width());
        }
    }
    const double width = medianOf(!letters.isEmpty() ? letters : characters);
    const double strength = std::clamp(query.strength, 0, 100) / 100.0;
    const double steal = kStealWidths * width * strength;
    const double temperature = kTemperature * strength * width * width;
    const double stick = (kStickWidths * width) * (kStickWidths * width);

    int best = -1;
    double bestScore = 0;
    for (int i = 0; i < query.cells.size(); ++i) {
        const KeyGravityCell& cell = query.cells.at(i);
        if (cell.symbol.isNull() || cell.gridId != geo.gridId) {
            continue;
        }
        const bool reached = cell.rect.adjusted(-steal, -steal, steal, steal).contains(query.gaze);
        if (i != query.geometricIndex && !reached) {
            continue;
        }
        double score = outsideDistance2(query.gaze, cell.rect)
                       - temperature * std::log(probabilityOf(query, cell.symbol));
        if (!query.stickyId.isEmpty() && cell.id == query.stickyId) {
            score -= stick;
        }
        if (best < 0 || score < bestScore - 1e-6
            || (i == query.geometricIndex && score <= bestScore + 1e-6)) {
            best = i;
            bestScore = score;
        }
    }
    return best < 0 ? query.geometricIndex : best;
}

double rapidDwellScale(int strength, double surprise)
{
    const double s = std::clamp(strength, 0, 100) / 100.0;
    const double z = std::clamp(surprise, -1.0, 1.0);
    return std::clamp(1.0 - 0.25 * s * z, 0.75, 1.20);
}

QVector<int> chooseRapidSteps(const QVector<int>& settingsRapid, const QVector<int>* authored,
                              int strength, double surprise)
{
    if (authored && !authored->isEmpty()) {
        return *authored;
    }
    const double scale = rapidDwellScale(strength, surprise);
    QVector<int> out;
    out.reserve(settingsRapid.size());
    for (int step : settingsRapid) {
        if (step == 0) {
            out.push_back(0);
            continue;
        }
        out.push_back(std::max(1, qRound(step * scale)));
    }
    return out;
}

} // namespace gazer
