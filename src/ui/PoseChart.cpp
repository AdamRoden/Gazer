#include "ui/PoseChart.h"
#include "mapping/HeadPoseCurve.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace gazer {
namespace PoseChart {

namespace {

void boundsOf(const QVector<HeadPoseCurvePoint>& points, double* inMin, double* inMax,
              double* outMin, double* outMax)
{
    double imn = -30, imx = 30, omn = -1, omx = 1;
    if (!points.isEmpty()) {
        imn = imx = points.first().in;
        omn = omx = points.first().out;
        for (const HeadPoseCurvePoint& pt : points) {
            imn = qMin(imn, pt.in);
            imx = qMax(imx, pt.in);
            omn = qMin(omn, pt.out);
            omx = qMax(omx, pt.out);
        }
    }
    if (qAbs(imx - imn) < 1e-6) {
        imn -= 1;
        imx += 1;
    }
    if (qAbs(omx - omn) < 1e-6) {
        omn -= 1;
        omx += 1;
    }
    const double ipad = (imx - imn) * 0.08;
    const double opad = (omx - omn) * 0.08;
    *inMin = imn - ipad;
    *inMax = imx + ipad;
    *outMin = omn - opad;
    *outMax = omx + opad;
}

QPointF toPlot(const QRectF& r, double in, double out, double inMin, double inMax, double outMin,
               double outMax)
{
    const double u = (in - inMin) / qMax(1e-9, inMax - inMin);
    const double v = (out - outMin) / qMax(1e-9, outMax - outMin);
    return {r.left() + u * r.width(), r.bottom() - v * r.height()};
}

} // namespace

QVector<HeadPoseCurvePoint> parseCaption(const QString& caption)
{
    QVector<HeadPoseCurvePoint> pts;
    const QStringList pairs = caption.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString& pair : pairs) {
        const QStringList xy = pair.split(QLatin1Char(','));
        if (xy.size() < 2) {
            continue;
        }
        HeadPoseCurvePoint pt;
        pt.in = xy.at(0).toDouble();
        pt.out = xy.at(1).toDouble();
        pts.push_back(pt);
    }
    return pts;
}

void paintCurve(QPainter& p, const QRectF& cell, const ThemeColors& theme,
                const QVector<HeadPoseCurvePoint>& points, int selectedIndex, double liveIn,
                bool showLive)
{
    p.save();
    const QColor bg = theme.bgMain.isValid() ? theme.bgMain : QColor(16, 16, 18);
    const QColor grid = theme.border.isValid() ? theme.border : QColor(80, 80, 86, 160);
    const QColor fg = theme.text.isValid() ? theme.text : QColor(220, 224, 230);
    const QColor accent = theme.accent.isValid() ? theme.accent : QColor(66, 133, 244);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(cell, 12, 12);

    const QRectF r = cell.adjusted(44, 28, -14, -28);
    double inMin, inMax, outMin, outMax;
    boundsOf(points, &inMin, &inMax, &outMin, &outMax);

    const QColor plotBg = theme.bgSurface.isValid() ? theme.bgSurface : bg;
    p.setPen(Qt::NoPen);
    p.setBrush(plotBg);
    p.drawRoundedRect(r, 8, 8);
    p.setPen(QPen(grid, 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r, 8, 8);
    for (int i = 1; i < 4; ++i) {
        const double x = r.left() + r.width() * i / 4.0;
        const double y = r.top() + r.height() * i / 4.0;
        p.setPen(QPen(QColor(grid.red(), grid.green(), grid.blue(), 70), 1, Qt::DotLine));
        p.drawLine(QPointF(x, r.top() + 4), QPointF(x, r.bottom() - 4));
        p.drawLine(QPointF(r.left() + 4, y), QPointF(r.right() - 4, y));
    }
    const QPointF origin = toPlot(r, 0, 0, inMin, inMax, outMin, outMax);
    if (r.contains(origin)) {
        p.setPen(QPen(grid, 1, Qt::DashLine));
        p.drawLine(QPointF(r.left(), origin.y()), QPointF(r.right(), origin.y()));
        p.drawLine(QPointF(origin.x(), r.top()), QPointF(origin.x(), r.bottom()));
    }

    if (points.size() >= 2) {
        QPainterPath path;
        path.moveTo(toPlot(r, points.first().in, points.first().out, inMin, inMax, outMin, outMax));
        for (int i = 1; i < points.size(); ++i) {
            path.lineTo(toPlot(r, points[i].in, points[i].out, inMin, inMax, outMin, outMax));
        }
        p.setPen(QPen(accent, 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }

    for (int i = 0; i < points.size(); ++i) {
        const QPointF pt = toPlot(r, points[i].in, points[i].out, inMin, inMax, outMin, outMax);
        const bool sel = i == selectedIndex;
        if (sel) {
            p.setPen(Qt::NoPen);
            QColor halo = accent;
            halo.setAlpha(70);
            p.setBrush(halo);
            p.drawEllipse(pt, 11.0, 11.0);
        }
        p.setPen(QPen(sel ? fg : accent, sel ? 2.2 : 1.5));
        p.setBrush(sel ? accent : plotBg);
        p.drawEllipse(pt, sel ? 6.5 : 4.5, sel ? 6.5 : 4.5);
    }

    if (showLive && !points.isEmpty()) {
        const double y = evalHeadPoseCurve(points, liveIn);
        const QPointF live = toPlot(r, liveIn, y, inMin, inMax, outMin, outMax);
        p.setPen(QPen(fg, 1.0, Qt::DashLine));
        p.drawLine(QPointF(live.x(), r.top()), QPointF(live.x(), r.bottom()));
        p.drawLine(QPointF(r.left(), live.y()), QPointF(r.right(), live.y()));
        p.setPen(QPen(fg, 1.4));
        p.setBrush(fg);
        p.drawEllipse(live, 4.0, 4.0);
    }

    const QColor muted = theme.textSecondary.isValid() ? theme.textSecondary : fg;
    p.setPen(muted);
    QFont f = p.font();
    f.setPixelSize(11);
    p.setFont(f);
    p.save();
    p.translate(cell.left() + 8, r.center().y());
    p.rotate(-90);
    p.drawText(QRectF(-48, -10, 96, 16), Qt::AlignCenter, QStringLiteral("Output"));
    p.restore();
    p.drawText(QRectF(r.left(), cell.bottom() - 22, r.width(), 18), Qt::AlignCenter,
               QStringLiteral("Input"));
    if (showLive) {
        const double y = evalHeadPoseCurve(points, liveIn);
        p.setPen(fg);
        p.drawText(cell.adjusted(12, 6, -12, 0), Qt::AlignTop | Qt::AlignLeft,
                   QStringLiteral("%1  →  %2").arg(liveIn, 0, 'f', 1).arg(y, 0, 'f', 1));
    }
    p.restore();
}

void valueAt(const QRectF& cell, const QVector<HeadPoseCurvePoint>& points, const QPointF& pos,
             double* in, double* out)
{
    const QRectF r = cell.adjusted(44, 28, -14, -28);
    double inMin, inMax, outMin, outMax;
    boundsOf(points, &inMin, &inMax, &outMin, &outMax);
    const double u = r.width() > 1 ? (pos.x() - r.left()) / r.width() : 0.0;
    const double v = r.height() > 1 ? (r.bottom() - pos.y()) / r.height() : 0.0;
    if (in) {
        *in = inMin + qBound(0.0, u, 1.0) * (inMax - inMin);
    }
    if (out) {
        *out = outMin + qBound(0.0, v, 1.0) * (outMax - outMin);
    }
}

} // namespace PoseChart
} // namespace gazer
