#include "ui/GlassBackdrop.h"

#include "layout/ChromeBlur.h"
#include "layout/RoundBox.h"
#include "utils/ScreenGrab.h"
#include "utils/WinOverlay.h"

#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QWindow>
#include <QtMath>
#include <utility>

namespace gazer {

namespace {

QPixmap downscaleBlur(const QPixmap& src, double radius)
{
    if (src.isNull() || radius < 1.0) {
        return src;
    }
    const int factor = qBound(2, qRound(radius / 2.0), 12);
    const QSize small(qMax(1, src.width() / factor), qMax(1, src.height() / factor));
    return src.scaled(small, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
        .scaled(src.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

} // namespace

GlassBackdrop::GlassBackdrop(QWindow* host)
    : m_host(host)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &GlassBackdrop::rebuild);
    if (!m_host) {
        return;
    }
    connect(m_host, &QWindow::xChanged, this, &GlassBackdrop::invalidate);
    connect(m_host, &QWindow::yChanged, this, &GlassBackdrop::invalidate);
    connect(m_host, &QWindow::widthChanged, this, &GlassBackdrop::invalidate);
    connect(m_host, &QWindow::heightChanged, this, &GlassBackdrop::invalidate);
    connect(m_host, &QWindow::visibleChanged, this, &GlassBackdrop::invalidate);
}

void GlassBackdrop::setActive(double maxBlurRadius)
{
    m_radius = qBound(0.0, maxBlurRadius, kChromeBlurMax);
    if (m_radius <= 0.0) {
        m_timer.stop();
        m_pad = 0;
        if (!m_frosted.isNull()) {
            m_frosted = {};
            m_frostKey = 0;
            emit updated();
        }
        return;
    }
    invalidate();
}

void GlassBackdrop::setCaptureRect(const QRect& globalRect)
{
    if (m_capture == globalRect) {
        return;
    }
    m_capture = globalRect;
    if (m_radius > 0.0) {
        invalidate();
    }
}

void GlassBackdrop::setUnderlay(QPixmap windowLocal, QPoint windowOrigin)
{
    m_underlay = std::move(windowLocal);
    m_underlayOrigin = windowOrigin;
    if (m_radius > 0.0) {
        invalidate();
    }
}

void GlassBackdrop::invalidate()
{
    if (m_radius <= 0.0 || m_capture.isEmpty() || !m_host || !m_host->isVisible()) {
        m_timer.stop();
        return;
    }
    m_timer.start(50);
}

void GlassBackdrop::rebuild()
{
    if (m_radius <= 0.0 || m_capture.isEmpty() || !m_host || !m_host->isVisible()) {
        m_frosted = {};
        m_frostKey = 0;
        return;
    }

    m_pad = qMax(8, qCeil(m_radius * 2.0));
    const QRect grabGlobal = m_capture.adjusted(-m_pad, -m_pad, m_pad, m_pad);

    QScreen* screen = m_host->screen();
    if (!screen) {
        screen = QGuiApplication::screenAt(grabGlobal.center());
    }
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) {
        return;
    }

    QPixmap raw;
    {
        const CaptureExclusion hideHost(m_host);
        raw = grabScreenRect(screen, grabGlobal);
    }
    if (!m_underlay.isNull() && !raw.isNull()) {
        QPainter up(&raw);
        up.setRenderHint(QPainter::SmoothPixmapTransform, true);
        up.drawPixmap(m_underlayOrigin - grabGlobal.topLeft(), m_underlay);
    }
    QPixmap next = downscaleBlur(raw, m_radius);
    const qint64 key = next.cacheKey();
    if (key != 0 && key == m_frostKey) {
        if (m_host->isVisible() && m_radius > 0.0 && !m_capture.isEmpty()) {
            m_timer.start(500);
        }
        return;
    }
    m_frosted = std::move(next);
    m_frostKey = key;

    if (m_host->isVisible() && m_radius > 0.0 && !m_capture.isEmpty()) {
        m_timer.start(500);
    }
    emit updated();
}

void GlassBackdrop::paint(QPainter& p, const QRectF& localRect, const PageBox& radii,
                          const QColor& tint) const
{
    if (localRect.isEmpty()) {
        return;
    }
    const QSize px = localRect.size().toSize().expandedTo(QSize(1, 1));
    QImage tile(px, QImage::Format_ARGB32_Premultiplied);
    tile.fill(Qt::transparent);
    {
        QPainter tp(&tile);
        tp.setRenderHint(QPainter::Antialiasing, true);
        tp.setRenderHint(QPainter::SmoothPixmapTransform, true);
        const QPainterPath path = roundedBoxPath(QRectF(QPointF(), localRect.size()), radii);
        tp.setClipPath(path);
        if (!m_frosted.isNull() && m_host) {
            const QPoint hostPos = m_host->position();
            const QPoint grabOrigin = m_capture.adjusted(-m_pad, -m_pad, m_pad, m_pad).topLeft();
            const QPointF dest = QPointF(grabOrigin - hostPos) - localRect.topLeft();
            tp.drawPixmap(dest, m_frosted);
        }
        if (tint.isValid() && tint.alpha() > 0) {
            tp.fillPath(path, tint);
        }
    }
    p.drawImage(localRect.topLeft(), tile);
}

} // namespace gazer
