#include "ui/PreviewWindow.h"
#include "ui/HeadPreviewRenderer.h"
#include "ui/PreviewGeometry.h"

#include <QCloseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QtMath>

namespace gazer {

using PreviewGeom::Vec3;
using PreviewGeom::applyLivePose;
using PreviewGeom::kHudH;

PreviewWindow::PreviewWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowTitle(QStringLiteral("Gazer — Head Preview"));
    setMinimumSize(480, 400);
    resize(900, 720);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void PreviewWindow::setTrackerName(const QString& name)
{
    m_trackerName = name;
    if (isVisible()) {
        update();
    }
}

void PreviewWindow::setTheme(const ThemeColors& theme)
{
    m_theme = theme;
    if (m_gl) {
        m_gl->setTheme(theme);
    }
    if (isVisible()) {
        update();
    }
}

void PreviewWindow::onGazeUpdated(const gazer::GazePoint& point)
{
    m_gaze = point;
    if (isVisible()) {
        update();
    }
}

void PreviewWindow::onHeadPoseUpdated(const gazer::HeadPose& pose)
{
    m_head = pose;
    if (isVisible()) {
        update();
    }
}

void PreviewWindow::showAndRaise()
{
    showNormal();
    raise();
    activateWindow();
    update();
}

void PreviewWindow::closeEvent(QCloseEvent* event)
{
    hide();
    event->ignore();
}

void PreviewWindow::paintOverlay(QPainter& p, const QRect& headArea)
{
    const double yaw = m_head.rotationValid ? m_head.yaw : 0.0;
    const double pitch = m_head.rotationValid ? m_head.pitch : 0.0;
    const double roll = m_head.rotationValid ? m_head.roll : 0.0;

    const QPointF g0(headArea.right() - 64, headArea.bottom() - 48);
    auto ax = [&](double x, double y, double z) {
        const Vec3 w = applyLivePose(Vec3{x, y, z}, yaw, pitch, roll, 0, 0, 0);
        const double d = qMax(0.5, (2.2 - w.z) / 2.2);
        return QPointF(g0.x() + w.x * 26 / d, g0.y() - w.y * 26 / d);
    };
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(220, 80, 80), 2));
    p.drawLine(g0, ax(1, 0, 0));
    p.setPen(QPen(QColor(80, 200, 100), 2));
    p.drawLine(g0, ax(0, 1, 0));
    p.setPen(QPen(QColor(80, 140, 255), 2));
    p.drawLine(g0, ax(0, 0, 1));

    p.setPen(m_theme.text.isValid() ? m_theme.text : QColor(220, 224, 230));
    p.setFont(QFont(QStringLiteral("Segoe UI"), 11));
    const int tris = m_gl ? m_gl->triCount() : 0;
    p.drawText(QRect(12, 4, width() - 24, 18), Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("Tracker: %1  ·  mesh %2")
                   .arg(m_trackerName)
                   .arg(tris <= 0 ? QStringLiteral("—")
                                  : QStringLiteral("%1 tris").arg(tris)));

    p.setFont(QFont(QStringLiteral("Consolas"), 10));
    p.setPen(m_theme.textSecondary.isValid() ? m_theme.textSecondary : QColor(150, 156, 165));
    const QString gazeBit = m_gaze.valid ? QStringLiteral("gaze live") : QStringLiteral("gaze lost");
    const QString line2 =
        m_head.valid()
            ? QStringLiteral("Yaw %1°  Pitch %2°  Roll %3°   X %4  Y %5  Z %6 cm  ·  %7")
                  .arg(m_head.yaw, 0, 'f', 1)
                  .arg(m_head.pitch, 0, 'f', 1)
                  .arg(m_head.roll, 0, 'f', 1)
                  .arg(m_head.x, 0, 'f', 1)
                  .arg(m_head.y, 0, 'f', 1)
                  .arg(m_head.z, 0, 'f', 1)
                  .arg(gazeBit)
            : QStringLiteral("Head pose: no sample — mesh still shown at rest  ·  %1").arg(gazeBit);
    p.drawText(QRect(12, 24, width() - 24, 18), Qt::AlignLeft | Qt::AlignVCenter, line2);
}

void PreviewWindow::paintEvent(QPaintEvent*)
{
    const QColor bg = m_theme.bgMain.isValid() ? m_theme.bgMain : QColor(14, 14, 16);
    const QRect headArea = rect().adjusted(0, kHudH, 0, 0);
    QPainter p(this);
    p.fillRect(rect(), bg);
    if (m_gl) {
        m_gl->setTheme(m_theme);
        m_gl->setGaze(m_gaze);
        m_gl->setPose(m_head);
        const QImage img = m_gl->render(headArea.size(), devicePixelRatioF());
        if (!img.isNull()) {
            p.drawImage(headArea, img);
        } else if (!m_gl->meshError().isEmpty()) {
            p.setPen(QColor(220, 100, 100));
            p.setFont(QFont(QStringLiteral("Segoe UI"), 12));
            p.drawText(headArea, Qt::AlignCenter,
                       QStringLiteral("Could not load resources/models/head.obj\n%1")
                           .arg(m_gl->meshError()));
        }
    }
    paintOverlay(p, headArea);
}

} // namespace gazer
