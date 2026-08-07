#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>
#include <algorithm>
#include <cmath>

namespace gazer {

struct CurvePoint {
    double in = 0.0;
    double out = 0.0;
};

/// Piecewise-linear response curve (OpenTrack-style input → output).
class CurveMapping {
public:
    void setPoints(QVector<CurvePoint> pts)
    {
        m_points = std::move(pts);
        sortPoints();
    }

    [[nodiscard]] const QVector<CurvePoint>& points() const { return m_points; }

    void setLinear(double inMin, double inMax, double outMin, double outMax)
    {
        m_points = {{inMin, outMin}, {inMax, outMax}};
    }

    void setIdentity(double range = 1.0) { setLinear(-range, range, -range, range); }

    /// Map input through the curve. Clamps to end segments outside domain.
    [[nodiscard]] double map(double input) const
    {
        if (m_points.isEmpty()) {
            return input;
        }
        if (m_points.size() == 1) {
            return m_points.first().out;
        }
        if (input <= m_points.first().in) {
            return m_points.first().out;
        }
        if (input >= m_points.last().in) {
            return m_points.last().out;
        }
        for (int i = 1; i < m_points.size(); ++i) {
            const CurvePoint& a = m_points[i - 1];
            const CurvePoint& b = m_points[i];
            if (input <= b.in) {
                const double den = b.in - a.in;
                if (std::abs(den) < 1e-12) {
                    return b.out;
                }
                const double t = (input - a.in) / den;
                return a.out + t * (b.out - a.out);
            }
        }
        return m_points.last().out;
    }

    [[nodiscard]] QJsonObject toJson() const
    {
        QJsonArray arr;
        for (const CurvePoint& p : m_points) {
            QJsonArray pair;
            pair.append(p.in);
            pair.append(p.out);
            arr.append(pair);
        }
        QJsonObject o;
        o.insert(QStringLiteral("points"), arr);
        return o;
    }

    static CurveMapping fromJson(const QJsonObject& o)
    {
        CurveMapping c;
        QVector<CurvePoint> pts;
        for (const QJsonValue& v : o.value(QStringLiteral("points")).toArray()) {
            if (!v.isArray()) {
                continue;
            }
            const QJsonArray pair = v.toArray();
            if (pair.size() < 2) {
                continue;
            }
            pts.push_back({pair.at(0).toDouble(), pair.at(1).toDouble()});
        }
        if (pts.isEmpty()) {
            c.setIdentity(40.0);
        } else {
            c.setPoints(std::move(pts));
        }
        return c;
    }

    [[nodiscard]] static CurveMapping softDeadzone(double dead, double inMax, double outMax)
    {
        CurveMapping c;
        c.setPoints({{-inMax, -outMax},
                     {-dead, 0.0},
                     {dead, 0.0},
                     {inMax, outMax}});
        return c;
    }

private:
    void sortPoints()
    {
        std::sort(m_points.begin(), m_points.end(),
                  [](const CurvePoint& a, const CurvePoint& b) { return a.in < b.in; });
    }

    QVector<CurvePoint> m_points;
};

/// Named curves used across head pose, LTS, magnifier, gaze stickiness.
struct CurveProfileStore {
    CurveMapping headYaw = CurveMapping::softDeadzone(2.0, 40.0, 1.0);
    CurveMapping headPitch = CurveMapping::softDeadzone(2.0, 30.0, 1.0);
    CurveMapping headRoll = CurveMapping::softDeadzone(2.0, 30.0, 1.0);
    CurveMapping headX = CurveMapping::softDeadzone(0.5, 10.0, 1.0);
    CurveMapping headY = CurveMapping::softDeadzone(0.5, 10.0, 1.0);
    CurveMapping headZ = CurveMapping::softDeadzone(0.5, 20.0, 1.0);
    CurveMapping lookToScroll = CurveMapping::softDeadzone(0.0, 1.0, 1.0);
    CurveMapping magnifierStickiness = CurveMapping::softDeadzone(0.0, 1.0, 1.0);
    CurveMapping gazeIndicatorStickiness = CurveMapping::softDeadzone(0.0, 1.0, 1.0);
    CurveMapping gazeMouseStickiness = CurveMapping::softDeadzone(0.0, 1.0, 1.0);

    [[nodiscard]] CurveMapping* curveById(const QString& id)
    {
        if (id == QLatin1String("head.yaw")) {
            return &headYaw;
        }
        if (id == QLatin1String("head.pitch")) {
            return &headPitch;
        }
        if (id == QLatin1String("head.roll")) {
            return &headRoll;
        }
        if (id == QLatin1String("head.x")) {
            return &headX;
        }
        if (id == QLatin1String("head.y")) {
            return &headY;
        }
        if (id == QLatin1String("head.z")) {
            return &headZ;
        }
        if (id == QLatin1String("lookToScroll.response")) {
            return &lookToScroll;
        }
        if (id == QLatin1String("magnifier.stickiness")) {
            return &magnifierStickiness;
        }
        if (id == QLatin1String("gazeIndicator.stickiness")) {
            return &gazeIndicatorStickiness;
        }
        if (id == QLatin1String("gazeMouse.stickiness")) {
            return &gazeMouseStickiness;
        }
        return nullptr;
    }

    [[nodiscard]] static QStringList allIds()
    {
        return {QStringLiteral("head.yaw"),
                QStringLiteral("head.pitch"),
                QStringLiteral("head.roll"),
                QStringLiteral("head.x"),
                QStringLiteral("head.y"),
                QStringLiteral("head.z"),
                QStringLiteral("lookToScroll.response"),
                QStringLiteral("magnifier.stickiness"),
                QStringLiteral("gazeIndicator.stickiness"),
                QStringLiteral("gazeMouse.stickiness")};
    }
};

} // namespace gazer
