#include "ui/GlassBackdrop.h"

#include "layout/LayoutTypes.h"
#include "utils/ScreenGrab.h"
#include "utils/WinOverlay.h"

#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QWindow>
#include <QtMath>

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
    m_radius = qBound(0.0, maxBlurRadius, LayoutChromeStyle::kMaxBlur);
    if (m_radius <= 0.0) {
        m_timer.stop();
        m_pad = 0;
        if (!m_frosted.isNull()) {
            m_frosted = {};
            emit updated();
        }
        return;
    }
    invalidate();
}

void GlassBackdrop::invalidate()
{
    if (m_radius <= 0.0 || !m_host || !m_host->isVisible()) {
        m_timer.stop();
        return;
    }
    m_timer.start(50);
}

void GlassBackdrop::rebuild()
{
    if (m_radius <= 0.0 || !m_host || !m_host->isVisible()) {
        m_frosted = {};
        return;
    }

    const QSize winSize = m_host->size();
    if (winSize.width() < 2 || winSize.height() < 2) {
        return;
    }

    m_pad = qMax(8, qCeil(m_radius * 2.0));
    const QRect grabGlobal = QRect(m_host->position(), winSize).adjusted(-m_pad, -m_pad, m_pad, m_pad);

    QScreen* screen = m_host->screen();
    if (!screen) {
        screen = QGuiApplication::screenAt(m_host->position());
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
    m_frosted = downscaleBlur(raw, m_radius);

    if (m_host->isVisible() && m_radius > 0.0) {
        m_timer.start(200);
    }
    emit updated();
}

void GlassBackdrop::paint(QPainter& p, const QRectF& localRect, double cornerRadius,
                          const QColor& tint) const
{
    if (localRect.isEmpty()) {
        return;
    }
    // Rasterize off the QQuickPaintedItem painter. setClipPath + drawText on that
    // painter drops glyphs under RHI; clipping here stays on a QImage.
    const QSize px = localRect.size().toSize().expandedTo(QSize(1, 1));
    QImage tile(px, QImage::Format_ARGB32_Premultiplied);
    tile.fill(Qt::transparent);
    {
        QPainter tp(&tile);
        tp.setRenderHint(QPainter::Antialiasing, true);
        tp.setRenderHint(QPainter::SmoothPixmapTransform, true);
        QPainterPath path;
        path.addRoundedRect(QRectF(QPointF(), localRect.size()), cornerRadius, cornerRadius);
        tp.setClipPath(path);
        if (!m_frosted.isNull()) {
            tp.drawPixmap(QPointF(-localRect.x() - m_pad, -localRect.y() - m_pad), m_frosted);
        }
        if (tint.isValid() && tint.alpha() > 0) {
            tp.fillPath(path, tint);
        }
    }
    p.drawImage(localRect.topLeft(), tile);
}

} // namespace gazer
