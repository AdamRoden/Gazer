#pragma once

#include "assist/MouseDwellMove.h"
#include "ui/OverlaySurface.h"
#include "ui/PickStyle.h"

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPen>
#include <QColor>
#include <QPixmap>
#include <QtGlobal>

namespace gazer {

class MouseDwellMove::CursorOverlay final : public OverlaySurface {
public:
    CursorOverlay()
    {
        resize(200, 200);
        hide();
    }

    void setProgress(double p)
    {
        m_progress = qBound(0.0, p, 1.0);
        update();
    }

    void setVisuals(const ProgressVisuals& v)
    {
        m_visuals = v;
        update();
    }

    void setStyle(int flags)
    {
        m_style = flags;
        update();
    }

    void placeCenter(const QPoint& c)
    {
        move(c.x() - width() / 2, c.y() - height() / 2);
        showOverlay();
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        PickStyle::paint(p, QRectF(rect()).center(), m_style, m_progress, m_visuals);
    }

private:
    double m_progress = 0.0;
    int m_style = PickStyle::kDefaultMousePick;
    ProgressVisuals m_visuals;
};

class MouseDwellMove::MagPickOverlay final : public OverlaySurface {
public:
    MagPickOverlay()
    {
        setOverlayLayer(OverlayLayer::MagPick);
        hide();
    }

    void showCapture(const QPixmap& pm, const QRect& destGlobal, const QString& hint, bool round)
    {
        m_pm = pm;
        m_hasPick = false;
        m_pick = {};
        m_progress = 0.0;
        m_hint = hint;
        m_round = round;
        setGeometry(destGlobal);
        showOverlay();
        update();
    }

    void setRound(bool on)
    {
        if (m_round == on) {
            return;
        }
        m_round = on;
        update();
    }

    void clearPick()
    {
        if (!m_hasPick) {
            return;
        }
        m_hasPick = false;
        m_pick = {};
        update();
    }

    void setProgress(double p)
    {
        m_progress = qBound(0.0, p, 1.0);
        update();
    }

    void setVisuals(const ProgressVisuals& v)
    {
        m_visuals = v;
        update();
    }

    void setStyle(int flags)
    {
        m_style = flags;
        update();
    }

    void setPickLocal(const QPoint& local)
    {
        m_pick = local;
        m_hasPick = true;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.fillRect(rect(), Qt::transparent);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);

        QPainterPath clip;
        if (m_round) {
            clip.addEllipse(QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5));
        } else {
            clip.addRect(QRectF(rect()));
        }
        p.save();
        p.setClipPath(clip);
        p.fillRect(rect(), QColor(0, 0, 0, 180));
        if (!m_pm.isNull()) {
            p.drawPixmap(rect(), m_pm);
        }
        p.restore();

        const QColor ring = m_visuals.progressColor;
        p.setPen(QPen(m_visuals.borderColor.isValid() ? m_visuals.borderColor : ring, 3.0));
        p.setBrush(Qt::NoBrush);
        const QRectF frame = QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5);
        if (m_round) {
            p.drawEllipse(frame);
        } else {
            p.drawRect(frame);
        }
        if (m_hasPick) {
            PickStyle::paint(p, QPointF(m_pick), m_style, m_progress, m_visuals);
        }
        p.setPen(Qt::white);
        p.setFont(QFont(QStringLiteral("Segoe UI"), 11, QFont::DemiBold));
        const QString hint = m_hint.isEmpty()
                                 ? QStringLiteral("Dwell to pick point (static zoom)")
                                 : m_hint;
        p.drawText(rect().adjusted(12, 10, -12, -10), Qt::AlignTop | Qt::AlignHCenter, hint);
    }

private:
    QPixmap m_pm;
    QPoint m_pick;
    bool m_hasPick = false;
    double m_progress = 0.0;
    int m_style = PickStyle::kDefaultMousePick;
    ProgressVisuals m_visuals;
    QString m_hint;
    bool m_round = false;
};

} // namespace gazer

