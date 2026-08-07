#include "ui/MappingPropertiesWindow.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QLabel>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace gazer {

MappingPropertiesWindow::MappingPropertiesWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("Gazer — Mapping Properties"));
    setMinimumSize(640, 480);
    resize(720, 560);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMouseTracking(true);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(12, 12, 12, 12);
    m_combo = new QComboBox(this);
    m_hint = new QLabel(
        QStringLiteral("Drag points · double-click empty to add · double-click point to remove"),
        this);
    m_hint->setStyleSheet(QStringLiteral("color: #aaa;"));
    lay->addWidget(m_combo);
    lay->addWidget(m_hint);
    lay->addStretch(1);

    connect(m_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &MappingPropertiesWindow::onCurvePicked);
    rebuildCombo();
}

void MappingPropertiesWindow::setStore(CurveProfileStore* store)
{
    m_store = store;
    rebuildCombo();
    update();
}

void MappingPropertiesWindow::setLiveInput(double value)
{
    m_liveIn = value;
    m_hasLive = true;
    update();
}

void MappingPropertiesWindow::selectCurve(const QString& id)
{
    m_curveId = id;
    if (m_combo) {
        const int idx = m_combo->findData(id);
        if (idx >= 0) {
            m_combo->setCurrentIndex(idx);
        }
    }
    update();
}

void MappingPropertiesWindow::showAndRaise()
{
    showNormal();
    raise();
    activateWindow();
}

void MappingPropertiesWindow::rebuildCombo()
{
    if (!m_combo) {
        return;
    }
    m_combo->blockSignals(true);
    m_combo->clear();
    for (const QString& id : CurveProfileStore::allIds()) {
        m_combo->addItem(id, id);
    }
    const int idx = m_combo->findData(m_curveId);
    m_combo->setCurrentIndex(idx >= 0 ? idx : 0);
    if (m_combo->currentData().isValid()) {
        m_curveId = m_combo->currentData().toString();
    }
    m_combo->blockSignals(false);
}

void MappingPropertiesWindow::onCurvePicked(int)
{
    if (m_combo) {
        m_curveId = m_combo->currentData().toString();
    }
    m_dragIndex = -1;
    update();
}

QRect MappingPropertiesWindow::graphRect() const
{
    return rect().adjusted(48, 72, -24, -36);
}

QPointF MappingPropertiesWindow::toScreen(double in, double out) const
{
    const QRect g = graphRect();
    // Domain approx from points or default ±40 / ±1
    double inMin = -40, inMax = 40, outMin = -1, outMax = 1;
    if (m_store) {
        if (CurveMapping* c = m_store->curveById(m_curveId)) {
            const auto& pts = c->points();
            if (!pts.isEmpty()) {
                inMin = pts.first().in;
                inMax = pts.last().in;
                outMin = outMax = pts.first().out;
                for (const CurvePoint& p : pts) {
                    outMin = std::min(outMin, p.out);
                    outMax = std::max(outMax, p.out);
                }
                if (std::abs(inMax - inMin) < 1e-6) {
                    inMax = inMin + 1;
                }
                if (std::abs(outMax - outMin) < 1e-6) {
                    outMax = outMin + 1;
                }
                // pad
                const double ip = (inMax - inMin) * 0.08;
                const double op = (outMax - outMin) * 0.08;
                inMin -= ip;
                inMax += ip;
                outMin -= op;
                outMax += op;
            }
        }
    }
    const double nx = (in - inMin) / (inMax - inMin);
    const double ny = (out - outMin) / (outMax - outMin);
    return {g.left() + nx * g.width(), g.bottom() - ny * g.height()};
}

QPointF MappingPropertiesWindow::toData(const QPoint& screen) const
{
    const QRect g = graphRect();
    double inMin = -40, inMax = 40, outMin = -1, outMax = 1;
    if (m_store) {
        if (CurveMapping* c = m_store->curveById(m_curveId)) {
            const auto& pts = c->points();
            if (!pts.isEmpty()) {
                inMin = pts.first().in;
                inMax = pts.last().in;
                outMin = outMax = pts.first().out;
                for (const CurvePoint& p : pts) {
                    outMin = std::min(outMin, p.out);
                    outMax = std::max(outMax, p.out);
                }
                if (std::abs(inMax - inMin) < 1e-6) {
                    inMax = inMin + 1;
                }
                if (std::abs(outMax - outMin) < 1e-6) {
                    outMax = outMin + 1;
                }
                const double ip = (inMax - inMin) * 0.08;
                const double op = (outMax - outMin) * 0.08;
                inMin -= ip;
                inMax += ip;
                outMin -= op;
                outMax += op;
            }
        }
    }
    const double nx = (screen.x() - g.left()) / double(g.width());
    const double ny = (g.bottom() - screen.y()) / double(g.height());
    return {inMin + nx * (inMax - inMin), outMin + ny * (outMax - outMin)};
}

void MappingPropertiesWindow::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(24, 26, 32));

    const QRect g = graphRect();
    p.setPen(QColor(60, 64, 72));
    p.setBrush(QColor(18, 20, 26));
    p.drawRect(g);

    // Axes at 0
    const QPointF o = toScreen(0, 0);
    p.setPen(QPen(QColor(80, 86, 96), 1, Qt::DashLine));
    if (g.contains(o.toPoint())) {
        p.drawLine(QPointF(g.left(), o.y()), QPointF(g.right(), o.y()));
        p.drawLine(QPointF(o.x(), g.top()), QPointF(o.x(), g.bottom()));
    }

    if (!m_store) {
        p.setPen(QColor(200, 80, 80));
        p.drawText(g, Qt::AlignCenter, QStringLiteral("No curve store"));
        return;
    }
    CurveMapping* curve = m_store->curveById(m_curveId);
    if (!curve || curve->points().isEmpty()) {
        p.setPen(QColor(200, 80, 80));
        p.drawText(g, Qt::AlignCenter, QStringLiteral("Empty curve"));
        return;
    }

    // Polyline
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(0, 220, 255), 2.0));
    QPolygonF poly;
    for (const CurvePoint& pt : curve->points()) {
        poly << toScreen(pt.in, pt.out);
    }
    p.drawPolyline(poly);

    // Control points
    for (int i = 0; i < curve->points().size(); ++i) {
        const QPointF s = toScreen(curve->points()[i].in, curve->points()[i].out);
        p.setBrush(i == m_dragIndex ? QColor(255, 200, 40) : QColor(0, 220, 255));
        p.setPen(Qt::NoPen);
        p.drawEllipse(s, 6, 6);
    }

    // Live sample
    if (m_hasLive) {
        const double out = curve->map(m_liveIn);
        const QPointF s = toScreen(m_liveIn, out);
        p.setPen(QPen(QColor(255, 100, 100), 2));
        p.setBrush(QColor(255, 80, 80));
        p.drawEllipse(s, 5, 5);
        p.setPen(QColor(220, 220, 220));
        p.drawText(12, height() - 12,
                   QStringLiteral("live in=%1 → out=%2").arg(m_liveIn, 0, 'f', 2).arg(out, 0, 'f', 3));
    }

    p.setPen(QColor(160, 170, 180));
    p.drawText(12, 56, QStringLiteral("Input →  |  ↑ Output    %1").arg(m_curveId));
}

void MappingPropertiesWindow::mousePressEvent(QMouseEvent* event)
{
    if (!m_store || event->button() != Qt::LeftButton) {
        return;
    }
    CurveMapping* curve = m_store->curveById(m_curveId);
    if (!curve) {
        return;
    }
    const QPoint pos = event->pos();
    for (int i = 0; i < curve->points().size(); ++i) {
        const QPointF s = toScreen(curve->points()[i].in, curve->points()[i].out);
        if (QLineF(s, pos).length() <= 10.0) {
            m_dragIndex = i;
            update();
            return;
        }
    }
}

void MappingPropertiesWindow::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragIndex < 0 || !m_store) {
        return;
    }
    CurveMapping* curve = m_store->curveById(m_curveId);
    if (!curve) {
        return;
    }
    auto pts = curve->points();
    if (m_dragIndex >= pts.size()) {
        return;
    }
    const QPointF d = toData(event->pos());
    pts[m_dragIndex].in = d.x();
    pts[m_dragIndex].out = d.y();
    curve->setPoints(pts);
    emit curvesChanged();
    update();
}

void MappingPropertiesWindow::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragIndex = -1;
        update();
    }
}

void MappingPropertiesWindow::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (!m_store || event->button() != Qt::LeftButton) {
        return;
    }
    CurveMapping* curve = m_store->curveById(m_curveId);
    if (!curve) {
        return;
    }
    auto pts = curve->points();
    const QPoint pos = event->pos();
    for (int i = 0; i < pts.size(); ++i) {
        const QPointF s = toScreen(pts[i].in, pts[i].out);
        if (QLineF(s, pos).length() <= 10.0) {
            if (pts.size() > 2) {
                pts.removeAt(i);
                curve->setPoints(pts);
                emit curvesChanged();
                update();
            }
            return;
        }
    }
    if (graphRect().contains(pos)) {
        const QPointF d = toData(pos);
        pts.push_back({d.x(), d.y()});
        curve->setPoints(pts);
        emit curvesChanged();
        update();
    }
}

void MappingPropertiesWindow::closeEvent(QCloseEvent* event)
{
    hide();
    event->ignore();
}

} // namespace gazer
