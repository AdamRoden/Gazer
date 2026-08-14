#pragma once

#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QRectF>
#include <QString>
#include <QtMath>

namespace gazer {
namespace MouseIcons {

/// Paint a simple OptiKey-style mouse action glyph centered in @p r.
inline void paint(QPainter& p, const QString& icon, const QRectF& r, const QColor& color)
{
    if (icon.isEmpty() || r.isEmpty()) {
        return;
    }
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(color, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);

    const QPointF c = r.center();
    const qreal s = qMin(r.width(), r.height()) * 0.38;

    auto mouseBody = [&]() {
        const QRectF body(c.x() - s * 0.55, c.y() - s * 0.7, s * 1.1, s * 1.4);
        p.drawRoundedRect(body, s * 0.35, s * 0.35);
        p.drawLine(QPointF(c.x(), body.top() + s * 0.15), QPointF(c.x(), body.center().y()));
        return body;
    };

    if (icon == QLatin1String("leftClick") || icon == QLatin1String("leftDbl")
        || icon == QLatin1String("leftHold")) {
        const QRectF body = mouseBody();
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(QRectF(body.left(), body.top(), body.width() * 0.5, body.height() * 0.45),
                          s * 0.2, s * 0.2);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(color, 2.2));
        if (icon == QLatin1String("leftDbl")) {
            p.drawText(QRectF(c.x() - s, c.y() + s * 0.15, s * 2, s * 0.6), Qt::AlignCenter,
                       QStringLiteral("2×"));
        } else if (icon == QLatin1String("leftHold")) {
            p.drawText(QRectF(c.x() - s, c.y() + s * 0.15, s * 2, s * 0.6), Qt::AlignCenter,
                       QStringLiteral("↓↑"));
        }
    } else if (icon == QLatin1String("rightClick") || icon == QLatin1String("rightDbl")
               || icon == QLatin1String("rightHold")) {
        const QRectF body = mouseBody();
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(
            QRectF(body.center().x(), body.top(), body.width() * 0.5, body.height() * 0.45),
            s * 0.2, s * 0.2);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(color, 2.2));
        if (icon == QLatin1String("rightDbl")) {
            p.drawText(QRectF(c.x() - s, c.y() + s * 0.15, s * 2, s * 0.6), Qt::AlignCenter,
                       QStringLiteral("2×"));
        } else if (icon == QLatin1String("rightHold")) {
            p.drawText(QRectF(c.x() - s, c.y() + s * 0.15, s * 2, s * 0.6), Qt::AlignCenter,
                       QStringLiteral("↓↑"));
        }
    } else if (icon == QLatin1String("middleClick") || icon == QLatin1String("middleHold")) {
        const QRectF body = mouseBody();
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(QRectF(c.x() - s * 0.12, body.top() + s * 0.2, s * 0.24, s * 0.35),
                          s * 0.08, s * 0.08);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(color, 2.2));
        if (icon == QLatin1String("middleHold")) {
            p.drawText(QRectF(c.x() - s, c.y() + s * 0.2, s * 2, s * 0.5), Qt::AlignCenter,
                       QStringLiteral("↓↑"));
        }
    } else if (icon == QLatin1String("moveTo") || icon == QLatin1String("moveLeftClick")
               || icon == QLatin1String("moveRightClick")) {
        // Cursor arrow
        QPainterPath path;
        path.moveTo(c.x() - s * 0.5, c.y() - s * 0.7);
        path.lineTo(c.x() - s * 0.5, c.y() + s * 0.5);
        path.lineTo(c.x() - s * 0.15, c.y() + s * 0.2);
        path.lineTo(c.x() + s * 0.1, c.y() + s * 0.65);
        path.lineTo(c.x() + s * 0.35, c.y() + s * 0.55);
        path.lineTo(c.x() + s * 0.1, c.y() + s * 0.1);
        path.lineTo(c.x() + s * 0.55, c.y() + s * 0.1);
        path.closeSubpath();
        p.setBrush(color);
        p.setPen(QPen(color.darker(140), 1.0));
        p.drawPath(path);
        if (icon == QLatin1String("moveLeftClick") || icon == QLatin1String("moveRightClick")) {
            p.setPen(QPen(color, 1.6));
            p.setFont(QFont(QStringLiteral("Segoe UI"), qMax(8, int(s * 0.55)), QFont::DemiBold));
            p.drawText(QRectF(c.x() - s * 0.15, c.y() + s * 0.35, s * 1.1, s * 0.55),
                       Qt::AlignCenter,
                       icon == QLatin1String("moveLeftClick") ? QStringLiteral("L")
                                                              : QStringLiteral("R"));
        }
    } else if (icon == QLatin1String("magPick") || icon == QLatin1String("magPickCenter")) {
        p.drawEllipse(c, s * 0.55, s * 0.55);
        p.drawLine(c.x() + s * 0.4, c.y() + s * 0.4, c.x() + s * 0.75, c.y() + s * 0.75);
        if (icon == QLatin1String("magPickCenter")) {
            // Crosshair: magnifier window follows the first dwell point.
            p.drawLine(QPointF(c.x() - s * 0.28, c.y()), QPointF(c.x() + s * 0.28, c.y()));
            p.drawLine(QPointF(c.x(), c.y() - s * 0.28), QPointF(c.x(), c.y() + s * 0.28));
            p.drawEllipse(c, s * 0.1, s * 0.1);
        } else {
            p.drawRect(QRectF(c.x() - s * 0.25, c.y() - s * 0.15, s * 0.35, s * 0.3));
        }
    } else if (icon == QLatin1String("scrollUp") || icon == QLatin1String("scrollDown")
               || icon == QLatin1String("scrollLeft") || icon == QLatin1String("scrollRight")) {
        mouseBody();
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        QPolygonF tri;
        if (icon == QLatin1String("scrollUp")) {
            tri << QPointF(c.x(), c.y() - s * 0.15) << QPointF(c.x() - s * 0.28, c.y() + s * 0.25)
                << QPointF(c.x() + s * 0.28, c.y() + s * 0.25);
        } else if (icon == QLatin1String("scrollDown")) {
            tri << QPointF(c.x(), c.y() + s * 0.35) << QPointF(c.x() - s * 0.28, c.y() - s * 0.05)
                << QPointF(c.x() + s * 0.28, c.y() - s * 0.05);
        } else if (icon == QLatin1String("scrollLeft")) {
            tri << QPointF(c.x() - s * 0.35, c.y() + s * 0.1) << QPointF(c.x() + s * 0.15, c.y() - s * 0.2)
                << QPointF(c.x() + s * 0.15, c.y() + s * 0.4);
        } else {
            tri << QPointF(c.x() + s * 0.35, c.y() + s * 0.1) << QPointF(c.x() - s * 0.15, c.y() - s * 0.2)
                << QPointF(c.x() - s * 0.15, c.y() + s * 0.4);
        }
        p.drawPolygon(tri);
    } else if (icon == QLatin1String("nudgeUp") || icon == QLatin1String("nudgeDown")
               || icon == QLatin1String("nudgeLeft") || icon == QLatin1String("nudgeRight")) {
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        QPolygonF tri;
        if (icon == QLatin1String("nudgeUp")) {
            tri << QPointF(c.x(), c.y() - s * 0.55) << QPointF(c.x() - s * 0.45, c.y() + s * 0.35)
                << QPointF(c.x() + s * 0.45, c.y() + s * 0.35);
        } else if (icon == QLatin1String("nudgeDown")) {
            tri << QPointF(c.x(), c.y() + s * 0.55) << QPointF(c.x() - s * 0.45, c.y() - s * 0.35)
                << QPointF(c.x() + s * 0.45, c.y() - s * 0.35);
        } else if (icon == QLatin1String("nudgeLeft")) {
            tri << QPointF(c.x() - s * 0.55, c.y()) << QPointF(c.x() + s * 0.35, c.y() - s * 0.45)
                << QPointF(c.x() + s * 0.35, c.y() + s * 0.45);
        } else {
            tri << QPointF(c.x() + s * 0.55, c.y()) << QPointF(c.x() - s * 0.35, c.y() - s * 0.45)
                << QPointF(c.x() - s * 0.35, c.y() + s * 0.45);
        }
        p.drawPolygon(tri);
    } else if (icon == QLatin1String("edgeTop") || icon == QLatin1String("edgeBottom")
               || icon == QLatin1String("edgeLeft") || icon == QLatin1String("edgeRight")) {
        const QRectF screen(c.x() - s * 0.7, c.y() - s * 0.5, s * 1.4, s * 1.0);
        p.drawRect(screen);
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        if (icon == QLatin1String("edgeTop")) {
            p.drawRect(QRectF(screen.left(), screen.top(), screen.width(), s * 0.22));
        } else if (icon == QLatin1String("edgeBottom")) {
            p.drawRect(QRectF(screen.left(), screen.bottom() - s * 0.22, screen.width(), s * 0.22));
        } else if (icon == QLatin1String("edgeLeft")) {
            p.drawRect(QRectF(screen.left(), screen.top(), s * 0.22, screen.height()));
        } else {
            p.drawRect(QRectF(screen.right() - s * 0.22, screen.top(), s * 0.22, screen.height()));
        }
    } else if (icon == QLatin1String("amount")) {
        p.drawText(r, Qt::AlignCenter, QStringLiteral("±"));
    } else if (icon == QLatin1String("magnifier")) {
        p.drawEllipse(c, s * 0.5, s * 0.5);
        p.drawLine(c.x() + s * 0.35, c.y() + s * 0.35, c.x() + s * 0.7, c.y() + s * 0.7);
    } else if (icon == QLatin1String("lookToScroll")) {
        p.drawEllipse(c, s * 0.55, s * 0.55);
        p.drawLine(c.x(), c.y() - s * 0.75, c.x(), c.y() - s * 0.35);
        p.drawLine(c.x(), c.y() + s * 0.35, c.x(), c.y() + s * 0.75);
        p.setBrush(color);
        p.drawPolygon(QPolygonF() << QPointF(c.x(), c.y() - s * 0.85)
                                  << QPointF(c.x() - s * 0.18, c.y() - s * 0.5)
                                  << QPointF(c.x() + s * 0.18, c.y() - s * 0.5));
        p.drawPolygon(QPolygonF() << QPointF(c.x(), c.y() + s * 0.85)
                                  << QPointF(c.x() - s * 0.18, c.y() + s * 0.5)
                                  << QPointF(c.x() + s * 0.18, c.y() + s * 0.5));
    } else if (icon == QLatin1String("main") || icon == QLatin1String("back")) {
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        p.drawPolygon(QPolygonF() << QPointF(c.x() - s * 0.15, c.y())
                                  << QPointF(c.x() + s * 0.45, c.y() - s * 0.45)
                                  << QPointF(c.x() + s * 0.45, c.y() + s * 0.45));
        p.drawRect(QRectF(c.x() + s * 0.35, c.y() - s * 0.18, s * 0.35, s * 0.36));
    } else if (icon == QLatin1String("edit")) {
        const QRectF body(c.x() - s * 0.15, c.y() - s * 0.55, s * 0.55, s * 0.85);
        p.drawRoundedRect(body, 2.0, 2.0);
        p.setBrush(color);
        QPolygonF tip;
        tip << QPointF(c.x() - s * 0.45, c.y() + s * 0.55) << QPointF(c.x() - s * 0.05, c.y() + s * 0.55)
            << QPointF(c.x() + s * 0.45, c.y() - s * 0.15) << QPointF(c.x() + s * 0.15, c.y() - s * 0.45);
        p.drawPolygon(tip);
    } else if (icon == QLatin1String("close")) {
        p.drawLine(c.x() - s * 0.45, c.y() - s * 0.45, c.x() + s * 0.45, c.y() + s * 0.45);
        p.drawLine(c.x() + s * 0.45, c.y() - s * 0.45, c.x() - s * 0.45, c.y() + s * 0.45);
    } else {
        // Fallback: first letter
        p.setFont(QFont(QStringLiteral("Segoe UI"), int(s * 0.9), QFont::Bold));
        p.drawText(r, Qt::AlignCenter, icon.left(1).toUpper());
    }
    p.restore();
}

} // namespace MouseIcons
} // namespace gazer
