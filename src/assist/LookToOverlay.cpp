#include "assist/LookToOverlay.h"

#include "ui/KeySymbols.h"
#include "ui/Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPen>
#include <QtMath>

namespace gazer {

namespace {

void paintRoundProgress(QPainter& p, const QPointF& c, double radius, double prog, const QColor& color)
{
    if (prog <= 0.01 || radius < 4.0) {
        return;
    }
    p.setBrush(Qt::NoBrush);
    const qreal w = qBound(2.6, radius * 0.14, 4.8);
    p.setPen(QPen(color, w, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(QRectF(c.x() - radius, c.y() - radius, radius * 2.0, radius * 2.0), 90 * 16,
              int(-360 * 16 * prog));
}

void paintAnnulus(QPainter& p, const QPointF& c, double inner, double outer, const QColor& fill)
{
    if (outer <= inner + 0.5) {
        return;
    }
    QPainterPath path;
    path.addEllipse(c, outer, outer);
    if (inner > 0.5) {
        path.addEllipse(c, inner, inner);
    }
    p.setPen(Qt::NoPen);
    p.setBrush(fill);
    p.drawPath(path);
}

void paintRingStroke(QPainter& p, const QPointF& c, double r, const QColor& color, qreal width)
{
    if (r < 2.0) {
        return;
    }
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(color, width, Qt::SolidLine, Qt::RoundCap));
    p.drawEllipse(c, r, r);
}

} // namespace

LookToOverlay::LookToOverlay(Kind kind)
    : m_kind(kind)
{
    resize(m_box, m_box);
    hide();
}

void LookToOverlay::setAccent(const QColor& c)
{
    if (c.isValid()) {
        m_accent = c;
        update();
    }
}

void LookToOverlay::setHighlight(LookToRing ring)
{
    if (m_highlight == ring) {
        return;
    }
    m_highlight = ring;
    update();
}

void LookToOverlay::setState(const LookToMapSettings& cfg, const QPointF& gaze, double gain,
                             double centerProg, double hubRadius, bool paused)
{
    m_cfg = cfg;
    m_gaze = gaze;
    m_gain = qBound(0.0, gain, 1.0);
    m_centerProg = qBound(0.0, centerProg, 1.0);
    m_hubR = qMax(8.0, hubRadius);
    m_paused = paused;

    const int activator = qMax(8, qRound(m_hubR));
    int side = activator * 2 + 36;
    if (m_kind == Kind::Preview) {
        side = lookToOverlayRadiusPx(m_cfg) * 2 + 48;
        side = qMax(side, activator * 2 + 36);
        side = qMin(side, 1800);
    } else if (!m_paused && m_cfg.showInnerDeadzone) {
        const int falloff = qMax(40, m_cfg.rampEndPx - m_cfg.deadzonePx);
        const int grow = qMax(48, falloff / 3);
        const int halo = qRound(orbThickness(m_cfg.deadzonePx) * 2.2);
        side = (m_cfg.deadzonePx + grow) * 2 + halo * 2 + 16;
        side = qMax(side, activator * 2 + 36);
    }
    if (side != m_box) {
        m_box = side;
        resize(m_box, m_box);
    }
    update();
}

void LookToOverlay::placeCenter(const QPoint& screenCenter)
{
    move(screenCenter.x() - width() / 2, screenCenter.y() - height() / 2);
    showOverlay();
    update();
}

void LookToOverlay::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QPointF c(rect().center());
    const QColor cyan = m_accent.isValid() ? m_accent : ThemeColors::defaultProgressColor();
    if (m_kind == Kind::Preview) {
        paintRings(p, c, cyan);
        paintGaze(p, c, cyan);
        if (m_cfg.showPause) {
            paintActivator(p, c, cyan);
        }
        return;
    }
    if (!m_paused && m_cfg.showInnerDeadzone) {
        paintOrb(p, c, cyan);
        if (m_cfg.showPause) {
            paintActivator(p, c, cyan);
        }
        return;
    }
    if (m_cfg.showPause) {
        paintActivator(p, c, cyan);
    }
}

double LookToOverlay::orbThickness(int deadzonePx)
{
    return qBound(16.0, double(qMax(1, deadzonePx)) * 0.24, 40.0);
}

void LookToOverlay::paintOrb(QPainter& p, const QPointF& c, const QColor& cyan)
{
    const bool hollow = m_cfg.showBorder && !m_cfg.showFill;
    if (!m_cfg.showFill && !m_cfg.showBorder) {
        return;
    }
    double stretch = 0.0;
    double ang = 0.0;
    const double gx = m_gaze.x();
    const double gy = m_gaze.y();
    const double len = qSqrt(gx * gx + gy * gy);
    if (len > 0.05 && m_gain > 0.02) {
        const int falloff = qMax(40, m_cfg.rampEndPx - m_cfg.deadzonePx);
        stretch = qMax(48.0, falloff / 3.0) * m_gain;
        ang = qRadiansToDegrees(qAtan2(gy / len, gx / len));
    }
    const int alpha = hollow ? int(40 + 32 * m_gain) : int(22 + 20 * m_gain);
    const OrbKey key{width(),
                     height(),
                     m_cfg.deadzonePx,
                     alpha,
                     cyan.rgba(),
                     hollow ? 1 : 0,
                     qRound(stretch),
                     stretch > 0.5 ? qRound(ang) : 0};
    if (key != m_orbKey || m_orbBlur.isNull()) {
        m_orbKey = key;
        m_orbBlur = renderOrbBlur(c, stretch, ang, cyan, alpha, hollow);
    }
    p.drawImage(rect().topLeft(), m_orbBlur);
}

QImage LookToOverlay::renderOrbBlur(const QPointF& c, double stretch, double ang, const QColor& cyan,
                                    int alpha, bool hollow) const
{
    const double R = double(qMax(1, m_cfg.deadzonePx));
    const double shift = stretch * 0.5;
    QPainterPath body;
    body.addEllipse(QPointF(shift, 0), R + shift, R);
    if (hollow) {
        QPainterPathStroker s;
        s.setWidth(orbThickness(m_cfg.deadzonePx));
        s.setCapStyle(Qt::RoundCap);
        s.setJoinStyle(Qt::RoundJoin);
        body = s.createStroke(body);
    }

    QImage src(size(), QImage::Format_ARGB32_Premultiplied);
    src.fill(Qt::transparent);
    {
        QPainter ip(&src);
        ip.setRenderHint(QPainter::Antialiasing, true);
        ip.translate(c);
        if (stretch > 0.5) {
            ip.rotate(ang);
        }
        ip.setPen(Qt::NoPen);
        ip.setBrush(QColor(cyan.red(), cyan.green(), cyan.blue(), alpha));
        ip.drawPath(body);
    }
    const int factor = 5;
    const QSize small(qMax(1, src.width() / factor), qMax(1, src.height() / factor));
    return src.scaled(small, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
        .scaled(src.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

void LookToOverlay::paintRings(QPainter& p, const QPointF& c, const QColor& cyan)
{
    const bool fill = m_cfg.showFill;
    const bool border = m_cfg.showBorder;
    const double dz = double(qMax(1, m_cfg.deadzonePx));
    const double rampEnd = double(qMax(m_cfg.deadzonePx + 1, m_cfg.rampEndPx));
    const double full = double(qMax(m_cfg.rampEndPx, m_cfg.fullOuterPx));
    const double outer = double(qMax(m_cfg.fullOuterPx, m_cfg.outerDeadzonePx));
    const int aRamp = int(28 + 36 * m_gain);
    auto stroke = [&](double r, int alpha, qreal w) {
        if (border) {
            paintRingStroke(p, c, r, QColor(cyan.red(), cyan.green(), cyan.blue(), alpha), w);
        }
    };

    if (m_cfg.outerDeadzoneEnabled && m_cfg.showOuterDeadzone && outer > full + 0.5) {
        if (fill) {
            QColor od = cyan;
            od.setAlpha(22);
            paintAnnulus(p, c, full, outer, od);
        }
        stroke(outer, 90, 2.0);
    }

    if (m_cfg.showMax) {
        if (fill) {
            QColor fullC = cyan;
            fullC.setAlpha(42);
            paintAnnulus(p, c, rampEnd, full, fullC);
            QColor rampC = cyan;
            rampC.setAlpha(aRamp);
            paintAnnulus(p, c, dz, rampEnd, rampC);
        }
        stroke(full, 150, 2.4);
        stroke(rampEnd, 170, 2.4);
    }

    if (m_cfg.showInnerDeadzone) {
        if (fill) {
            QColor dzC = cyan;
            dzC.setAlpha(36);
            p.setPen(Qt::NoPen);
            p.setBrush(dzC);
            p.drawEllipse(c, dz, dz);
        }
        stroke(dz, 200, 2.6);
    }

    const double hiR = highlightRadius();
    if (hiR > 1.0) {
        paintRingStroke(p, c, hiR, QColor(255, 255, 255, 220), 3.4);
    }
}

double LookToOverlay::highlightRadius() const
{
    switch (m_highlight) {
    case LookToRing::Deadzone:
        return double(m_cfg.deadzonePx);
    case LookToRing::Ramp:
        return double(m_cfg.rampEndPx);
    case LookToRing::Full:
        return double(m_cfg.fullOuterPx);
    case LookToRing::Outer:
        return m_cfg.outerDeadzoneEnabled ? double(m_cfg.outerDeadzonePx) : 0.0;
    case LookToRing::None:
        return 0.0;
    }
    return 0.0;
}

void LookToOverlay::paintGaze(QPainter& p, const QPointF& c, const QColor& cyan)
{
    const double gx = m_gaze.x();
    const double gy = m_gaze.y();
    const double dist = qSqrt(gx * gx + gy * gy);
    if (dist < 1.0) {
        return;
    }
    const QPointF g = c + m_gaze;
    p.setPen(QPen(QColor(cyan.red(), cyan.green(), cyan.blue(), 140), 1.6, Qt::SolidLine,
                  Qt::RoundCap));
    p.drawLine(c, g);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(cyan.red(), cyan.green(), cyan.blue(), int(80 + 140 * m_gain)));
    p.drawEllipse(g, 6.0, 6.0);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 200), 1.4));
    p.drawEllipse(g, 6.0, 6.0);
}

void LookToOverlay::paintActivator(QPainter& p, const QPointF& c, const QColor& accent)
{
    const double hubR = m_hubR;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 50));
    p.drawEllipse(c, hubR, hubR);
    p.setBrush(QColor(accent.red(), accent.green(), accent.blue(), 40));
    p.drawEllipse(c, hubR, hubR);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(accent.red(), accent.green(), accent.blue(), 150), 2.2, Qt::SolidLine,
                  Qt::RoundCap));
    p.drawEllipse(c, hubR, hubR);
    if (m_paused) {
        const double side = hubR * 1.2;
        const QRectF icon(c.x() - side * 0.5, c.y() - side * 0.5, side, side);
        const QColor fg(255, 255, 255, 230);
        KeySymbols::paint(p, QStringLiteral("Sleep"), icon, fg);
    } else if (m_cfg.hubEnabled) {
        paintRoundProgress(p, c, qMax(4.0, hubR - 1.5), m_centerProg, accent);
    }
}

} // namespace gazer
