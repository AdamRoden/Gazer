#include "ui/PreviewWindow.h"

#include <QCloseEvent>
#include <QPaintEvent>
#include <QPainter>

namespace gazer {

PreviewWindow::PreviewWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("Gazer — Gaze Preview"));
    setMinimumSize(640, 480);
    resize(960, 600);
    setAttribute(Qt::WA_OpaquePaintEvent);
    // Qt::WA_QuitOnClose is false globally via setQuitOnLastWindowClosed(false).
}

void PreviewWindow::setTrackerName(const QString& name)
{
    m_trackerName = name;
    update();
}

void PreviewWindow::onGazeUpdated(const gazer::GazePoint& point)
{
    m_gaze = point;
    update();
}

void PreviewWindow::showAndRaise()
{
    showNormal();
    raise();
    activateWindow();
}

void PreviewWindow::closeEvent(QCloseEvent* event)
{
    // Hide instead of destroy — tray "Show preview" reopens this instance.
    hide();
    event->ignore();
}

void PreviewWindow::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(24, 26, 32));

    p.setPen(QColor(180, 190, 200));
    p.setFont(QFont(QStringLiteral("Segoe UI"), 11));
    const QString hud = QStringLiteral("Tracker: %1   Gaze: %2")
                            .arg(m_trackerName,
                                 m_gaze.valid
                                     ? QStringLiteral("(%1, %2)")
                                           .arg(m_gaze.x, 0, 'f', 0)
                                           .arg(m_gaze.y, 0, 'f', 0)
                                     : QStringLiteral("invalid"));
    p.drawText(QRect(12, 10, width() - 24, 24), Qt::AlignLeft | Qt::AlignVCenter,
               hud);
    p.drawText(QRect(12, height() - 32, width() - 24, 24),
               Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("Crosshair follows live gaze (screen coords → window)"));

    if (!m_gaze.valid) {
        p.setPen(QColor(220, 80, 80));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("No valid gaze sample"));
        return;
    }

    // Map virtual-desktop screen coords into this widget.
    const QPoint topLeftGlobal = mapToGlobal(QPoint(0, 0));
    const double localX = m_gaze.x - topLeftGlobal.x();
    const double localY = m_gaze.y - topLeftGlobal.y();

    const QPointF center(localX, localY);
    const bool inside = rect().adjusted(-2, -2, 2, 2).contains(center.toPoint());

    p.setRenderHint(QPainter::Antialiasing, true);

    const QColor cross = inside ? QColor(0, 220, 255) : QColor(255, 180, 40);
    p.setPen(QPen(cross, 2.0));

    constexpr double arm = 22.0;
    constexpr double gap = 6.0;

    p.drawLine(QPointF(center.x() - arm, center.y()),
               QPointF(center.x() - gap, center.y()));
    p.drawLine(QPointF(center.x() + gap, center.y()),
               QPointF(center.x() + arm, center.y()));
    p.drawLine(QPointF(center.x(), center.y() - arm),
               QPointF(center.x(), center.y() - gap));
    p.drawLine(QPointF(center.x(), center.y() + gap),
               QPointF(center.x(), center.y() + arm));

    p.setPen(QPen(cross, 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(center, 14.0, 14.0);

    p.setBrush(cross);
    p.setPen(Qt::NoPen);
    p.drawEllipse(center, 3.0, 3.0);
}

} // namespace gazer
