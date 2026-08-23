#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

namespace gazer {

/// 1, 2, or 4 box values. CSS expansion: all; vert/horiz; T/R/B/L or TL/TR/BR/BL.
struct PageBox {
    QVector<double> v;

    [[nodiscard]] static PageBox all(double n)
    {
        PageBox b;
        b.v = {n};
        return b;
    }

    [[nodiscard]] static PageBox of(double a, double b, double c, double d)
    {
        PageBox box;
        box.v = {a, b, c, d};
        return box;
    }

    [[nodiscard]] bool isSet() const { return !v.isEmpty(); }

    [[nodiscard]] double first() const { return v.isEmpty() ? 0.0 : v[0]; }

    [[nodiscard]] bool uniform() const
    {
        if (v.size() <= 1) {
            return true;
        }
        const double a = v[0];
        for (double x : v) {
            if (!qFuzzyCompare(x + 1.0, a + 1.0)) {
                return false;
            }
        }
        return true;
    }

    /// i = 0 top / TL, 1 right / TR, 2 bottom / BR, 3 left / BL.
    [[nodiscard]] double at(int i) const
    {
        if (v.isEmpty()) {
            return 0.0;
        }
        i = i & 3;
        if (v.size() == 1) {
            return v[0];
        }
        if (v.size() == 2) {
            return (i == 0 || i == 2) ? v[0] : v[1];
        }
        if (v.size() == 3) {
            return i == 0 ? v[0] : (i == 2 ? v[2] : v[1]);
        }
        return v[i];
    }

    [[nodiscard]] static PageBox fromToken(const QString& s)
    {
        PageBox b;
        for (QString p : s.split(QLatin1Char(','))) {
            p = p.trimmed();
            if (p.isEmpty()) {
                continue;
            }
            bool ok = false;
            const double n = p.toDouble(&ok);
            if (ok) {
                b.v.push_back(n);
            }
            if (b.v.size() == 4) {
                break;
            }
        }
        return b;
    }

    [[nodiscard]] QString toToken() const
    {
        if (v.isEmpty()) {
            return {};
        }
        if (uniform()) {
            return QString::number(first(), 'g', 8);
        }
        QStringList parts;
        for (double n : v) {
            parts.push_back(QString::number(n, 'g', 8));
        }
        return parts.join(QLatin1Char(','));
    }
};

} // namespace gazer
