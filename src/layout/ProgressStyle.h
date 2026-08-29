#pragma once

#include <QString>
#include <QStringList>

namespace gazer {

enum class ProgressFillDir { None, Center, Up, Down, Right, Left };

/// Dwell-progress shape tokens. CSV is the XML/JSON boundary only.
struct ProgressStyle {
    bool radial = true;
    bool pie = false;
    bool fillBackground = false;
    ProgressFillDir fillDir = ProgressFillDir::Center;
    bool border = false;

    [[nodiscard]] bool any() const
    {
        return radial || pie || fillBackground || border;
    }

    void ensureDefault()
    {
        if (!any()) {
            radial = true;
        }
    }

    /// Pointer dwell: ring + border (boards default to ring only).
    [[nodiscard]] static ProgressStyle pointerDefaults()
    {
        ProgressStyle s;
        s.border = true;
        return s;
    }

    [[nodiscard]] static ProgressStyle fromCsv(const QString& csv)
    {
        ProgressStyle s;
        s.radial = false;
        s.pie = false;
        s.fillBackground = false;
        s.fillDir = ProgressFillDir::Center;
        s.border = false;
        const QStringList parts = csv.toLower().split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (QString p : parts) {
            p = p.trimmed();
            if (p == QLatin1String("radial") || p == QLatin1String("ring")) {
                s.radial = true;
            } else if (p == QLatin1String("pie")) {
                s.pie = true;
            } else if (p == QLatin1String("fill") || p == QLatin1String("background")) {
                s.fillBackground = true;
                s.fillDir = ProgressFillDir::Center;
            } else if (p == QLatin1String("fillup") || p == QLatin1String("up")) {
                s.fillBackground = true;
                s.fillDir = ProgressFillDir::Up;
            } else if (p == QLatin1String("filldown") || p == QLatin1String("down")) {
                s.fillBackground = true;
                s.fillDir = ProgressFillDir::Down;
            } else if (p == QLatin1String("fillright") || p == QLatin1String("right")) {
                s.fillBackground = true;
                s.fillDir = ProgressFillDir::Right;
            } else if (p == QLatin1String("fillleft") || p == QLatin1String("left")) {
                s.fillBackground = true;
                s.fillDir = ProgressFillDir::Left;
            } else if (p == QLatin1String("border") || p == QLatin1String("outline")) {
                s.border = true;
            }
        }
        s.ensureDefault();
        return s;
    }

    [[nodiscard]] QString toCsv() const
    {
        QStringList p;
        if (radial) {
            p << QStringLiteral("radial");
        }
        if (pie) {
            p << QStringLiteral("pie");
        }
        if (fillBackground) {
            switch (fillDir) {
            case ProgressFillDir::Up:
                p << QStringLiteral("fillup");
                break;
            case ProgressFillDir::Down:
                p << QStringLiteral("filldown");
                break;
            case ProgressFillDir::Right:
                p << QStringLiteral("fillright");
                break;
            case ProgressFillDir::Left:
                p << QStringLiteral("fillleft");
                break;
            case ProgressFillDir::Center:
            case ProgressFillDir::None:
                p << QStringLiteral("fill");
                break;
            }
        }
        if (border) {
            p << QStringLiteral("border");
        }
        return p.join(QLatin1Char(','));
    }
};

} // namespace gazer
