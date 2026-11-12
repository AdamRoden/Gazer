#include "assist/MouseLeftDrag.h"

#include "assist/MouseAssistState.h"
#include "input/MouseInjector.h"
#include "ui/KeySymbols.h"
#include "ui/OverlaySurface.h"
#include "utils/Log.h"
#include "utils/ScreenGrab.h"

#include <QGuiApplication>
#include <QThread>
#include <QPainter>
#include <QPaintEvent>
#include <QPen>
#include <QPolygonF>
#include <QScreen>

namespace gazer {

namespace {

QRect screenContaining(const QPoint& p)
{
    for (QScreen* s : QGuiApplication::screens()) {
        if (s && s->geometry().contains(p)) {
            return s->geometry();
        }
    }
    if (QScreen* s = QGuiApplication::screenAt(p)) {
        return s->geometry();
    }
    return overlayScreenGeometry();
}

QColor dragInk(QColor color)
{
    if (!color.isValid()) {
        color = QColor(0, 180, 220);
    }
    if (color.alpha() < 200) {
        color.setAlpha(230);
    }
    return color;
}

void paintDotted(QPainter& p, const QPointF& from, const QPointF& to, const QColor& ink)
{
    const QPointF d = to - from;
    const double len = std::hypot(d.x(), d.y());
    if (len < 10.0) {
        return;
    }
    const QPointF u = d / len;
    const QPointF n(-u.y(), u.x());
    const QPointF base = to - u * 16.0;
    auto stroke = [&](const QColor& c, double width) {
        QPen pen(c, width, Qt::CustomDashLine, Qt::RoundCap, Qt::RoundJoin);
        pen.setDashPattern({0.12, 1.7});
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawLine(from, base);
    };
    stroke(QColor(0, 0, 0, 180), 7.0);
    stroke(ink, 3.5);

    QPolygonF head;
    head << to << (base + n * 9.0) << (base - n * 9.0);
    p.setPen(QPen(QColor(0, 0, 0, 180), 1.5));
    p.setBrush(ink);
    p.drawPolygon(head);
}

void paintPressDot(QPainter& p, const QPointF& c, const QColor& ink)
{
    p.setPen(QPen(QColor(0, 0, 0, 200), 2.0));
    p.setBrush(ink);
    p.drawEllipse(c, 9.0, 9.0);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    p.drawEllipse(c, 3.0, 3.0);
}

void paintProgressRing(QPainter& p, const QPointF& c, double radius, double progress, const QColor& ink)
{
    const QRectF arc(c.x() - radius, c.y() - radius, radius * 2.0, radius * 2.0);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 90), 2.0));
    p.drawEllipse(arc);
    if (progress <= 0.001) {
        return;
    }
    p.setPen(QPen(ink, 4.0, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(arc, 90 * 16, int(-progress * 360.0 * 16.0));
}

QColor choiceRim(DragChoice choice, const QColor& ink)
{
    switch (choice) {
    case DragChoice::Release:
        return QColor(52, 168, 92);
    case DragChoice::Cancel:
        return QColor(196, 72, 72);
    case DragChoice::Move:
        return QColor(245, 196, 32);
    }
    return ink;
}

const char* choiceIcon(DragChoice choice)
{
    switch (choice) {
    case DragChoice::Move:
        return "mouseMove";
    case DragChoice::Release:
        return "check";
    case DragChoice::Cancel:
        return "close";
    }
    return "";
}

} // namespace

class MouseLeftDrag::Overlay final : public OverlaySurface {
public:
    struct View {
        QPointF start;
        QPointF tip;
        double aimProgress = 0.0;
        bool aiming = false;
        bool choosing = false;
        DragFan fan;
        int hot = -1;
        double hotProgress = 0.0;
        QColor ink;
    };

    Overlay() { hide(); }

    void setView(const View& view)
    {
        m_view = view;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QPointF origin(pos());
        const auto local = [&](QPointF g) { return g - origin; };
        const QColor ink = m_view.ink;
        paintDotted(p, local(m_view.start), local(m_view.tip), ink);
        paintPressDot(p, local(m_view.start), ink);
        if (m_view.aiming) {
            paintProgressRing(p, local(m_view.tip), 24.0, m_view.aimProgress, ink);
        }
        if (!m_view.choosing) {
            return;
        }
        for (int i = 0; i < 3; ++i) {
            const DragFanButton& b = m_view.fan.buttons[i];
            const QPointF c = local(b.center);
            const double r = m_view.fan.buttonRadius;
            const bool hot = i == m_view.hot;
            const QColor rim = choiceRim(b.choice, ink);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, 80));
            p.drawEllipse(c + QPointF(0, 2), r, r);
            p.setPen(QPen(rim, hot ? 4.0 : 3.0));
            p.setBrush(hot ? QColor(48, 54, 64, 242) : QColor(28, 32, 40, 235));
            p.drawEllipse(c, r, r);
            if (hot) {
                paintProgressRing(p, c, r + 8.0, m_view.hotProgress, rim);
            }
            const double side = r * 1.35;
            const QRectF icon(c.x() - side * 0.5, c.y() - side * 0.5, side, side);
            KeySymbols::paint(p, QLatin1String(choiceIcon(b.choice)), icon,
                              QColor(255, 255, 255, 235));
        }
    }

private:
    View m_view;
};

MouseLeftDrag::MouseLeftDrag(QObject* parent)
    : QObject(parent)
{
    m_clock.start();
    m_invalidGrace.graceMs = 220;
    m_dwell.setDwellMs(m_dwellMs * kDragFinishDwellScale);
    m_overlay = std::make_unique<Overlay>();
}

MouseLeftDrag::~MouseLeftDrag()
{
    blockSignals(true);
    if (m_phase != Phase::Idle) {
        finish(true);
    }
}

void MouseLeftDrag::setDwellMs(int ms)
{
    m_dwellMs = qMax(50, ms);
    m_dwell.setDwellMs(m_dwellMs * kDragFinishDwellScale);
}

void MouseLeftDrag::setProgressVisuals(const ProgressVisuals& visuals)
{
    m_visuals = visuals;
    syncOverlay();
}

void MouseLeftDrag::setPaused(bool paused)
{
    if (m_paused == paused) {
        return;
    }
    m_paused = paused;
    if (!m_paused) {
        return;
    }
    m_dwell.reset();
    m_hotProg = 0.0;
    m_lastSampleMs = -1;
    syncOverlay();
}

bool MouseLeftDrag::containsChoice(const GazePoint& point) const
{
    if (m_phase != Phase::Choose || !point.valid) {
        return false;
    }
    return hitDragFan(m_fan, QPointF(point.x, point.y)) >= 0;
}

bool MouseLeftDrag::begin(const QPoint& press, QString* error)
{
    if (m_phase != Phase::Idle) {
        return true;
    }
    if (!m_assist) {
        if (error) {
            *error = QStringLiteral("Left drag unavailable");
        }
        return false;
    }
    if (!m_assist->setHeld(QStringLiteral("left"), true, error)) {
        return false;
    }
    m_start = press;
    m_end = press;
    m_tip = QPointF(press);
    m_phase = Phase::Aim;
    m_paused = false;
    m_gateButtons = false;
    m_hot = -1;
    m_hotProg = 0.0;
    m_lastSampleMs = -1;
    m_invalidGrace.reset();
    m_dwell.reset();
    fitOverlay();
    syncOverlay();
    GAZER_INFO << "MouseLeftDrag press" << press;
    emit activeChanged(true);
    return true;
}

void MouseLeftDrag::cancel()
{
    finish(true);
}

void MouseLeftDrag::finish(bool moveBack)
{
    if (m_phase == Phase::Idle) {
        return;
    }
    const QPoint target = moveBack ? m_start : m_end;
    m_phase = Phase::Idle;
    m_gateButtons = false;
    m_hot = -1;
    m_hotProg = 0.0;
    m_dwell.reset();
    if (m_overlay) {
        m_overlay->hide();
    }
    QString err;
    if (!MouseInjector::moveTo(target.x(), target.y(), &err)) {
        GAZER_WARN << "MouseLeftDrag move before release failed:" << err;
    }
    if (moveBack) {
        QThread::msleep(50);
    }
    if (m_assist && !m_assist->setHeld(QStringLiteral("left"), false, &err)) {
        GAZER_WARN << "MouseLeftDrag release failed:" << err;
    }
    GAZER_INFO << (moveBack ? "MouseLeftDrag released at start" : "MouseLeftDrag released at end")
               << target;
    emit activeChanged(false);
}

void MouseLeftDrag::enterChoose(const QPoint& end)
{
    m_end = end;
    m_tip = QPointF(end);
    m_phase = Phase::Choose;
    m_gateButtons = false;
    m_hot = -1;
    m_hotProg = 0.0;
    m_dwell.reset();
    m_lastSampleMs = -1;
    const QRect screen = screenContaining(end);
    m_fan = layoutDragFan(QPointF(end), QRectF(screen));
    fitOverlay();
    syncOverlay();
    GAZER_INFO << "MouseLeftDrag choose" << end;
}

void MouseLeftDrag::fitOverlay()
{
    if (!m_overlay) {
        return;
    }
    const QRect desk = virtualDesktop();
    if (desk.isEmpty()) {
        return;
    }
    if (m_overlay->geometry() != desk) {
        m_overlay->setGeometry(desk);
    }
    m_overlay->showOverlay();
}

void MouseLeftDrag::syncOverlay()
{
    if (!m_overlay) {
        return;
    }
    if (m_phase == Phase::Idle) {
        m_overlay->hide();
        return;
    }
    Overlay::View view;
    view.start = QPointF(m_start);
    view.tip = m_tip;
    view.aimProgress = m_phase == Phase::Aim ? m_dwell.progress() : 0.0;
    view.aiming = m_phase == Phase::Aim && !m_paused;
    view.choosing = m_phase == Phase::Choose;
    view.fan = m_fan;
    view.hot = m_hot;
    view.hotProgress = m_hotProg;
    view.ink = dragInk(m_visuals.progressColor);
    m_overlay->setView(view);
}

void MouseLeftDrag::onGaze(const GazePoint& point)
{
    if (m_phase == Phase::Idle || m_paused) {
        return;
    }
    const qint64 now = m_clock.elapsed();
    if (!point.valid) {
        if (m_invalidGrace.onInvalid(now) == InvalidGazeGrace::Result::Holding) {
            return;
        }
        m_dwell.reset();
        m_hotProg = 0.0;
        m_lastSampleMs = -1;
        syncOverlay();
        return;
    }
    m_invalidGrace.onValid();

    const QPointF g(point.x, point.y);
    const double dtSec = m_lastSampleMs < 0
                             ? 0.016
                             : qBound(0.004, (now - m_lastSampleMs) / 1000.0, 0.08);
    m_lastSampleMs = now;

    if (m_phase == Phase::Choose) {
        const int hit = hitDragFan(m_fan, g);
        if (hit != m_hot) {
            m_hot = hit;
            m_hotProg = 0.0;
        }
        const double choiceDwell = double(m_dwellMs);
        if (hit < 0) {
            m_hotProg = qMax(0.0, m_hotProg - dtSec * 1000.0 / choiceDwell * 1.6);
        } else {
            m_hotProg = qMin(1.0, m_hotProg + dtSec * 1000.0 / choiceDwell);
        }
        syncOverlay();
        if (m_hotProg < 1.0 || hit < 0) {
            return;
        }
        const DragChoice choice = m_fan.buttons[hit].choice;
        m_hotProg = 0.0;
        m_hot = -1;
        if (choice == DragChoice::Move) {
            m_phase = Phase::Aim;
            m_gateButtons = true;
            m_dwell.reset();
            m_lastSampleMs = -1;
            m_tip = g;
            syncOverlay();
            GAZER_INFO << "MouseLeftDrag aim again";
            return;
        }
        // Check lets go at the fixated end. X moves back to the press point, then lets go.
        finish(choice == DragChoice::Cancel);
        return;
    }

    m_tip = m_tip * 0.4 + g * 0.6;
    if (m_gateButtons && hitDragFan(m_fan, g) >= 0) {
        m_dwell.reset();
        syncOverlay();
        return;
    }
    if (m_gateButtons) {
        m_gateButtons = false;
        m_dwell.reset();
    }
    const bool done = m_dwell.sample(g, dtSec);
    syncOverlay();
    if (!done) {
        return;
    }
    const QPoint end(qRound(m_dwell.commitPos().x()), qRound(m_dwell.commitPos().y()));
    QString err;
    if (!MouseInjector::moveTo(end.x(), end.y(), &err)) {
        GAZER_WARN << "MouseLeftDrag move failed:" << err;
        m_dwell.reset();
        syncOverlay();
        return;
    }
    enterChoose(end);
}

} // namespace gazer
