#include "ui/LayoutWindow.h"

#include "layout/LayoutGeometry.h"
#include "ui/MouseIcons.h"
#include "utils/WinOverlay.h"

#include <QCloseEvent>
#include <QFont>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QShowEvent>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

namespace gazer {

LayoutWindow::LayoutWindow(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool
                   | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_QuitOnClose, false);
    setWindowTitle(QStringLiteral("Gazer — Layout"));
    setMinimumSize(320, 160);
    resize(1000, 560);

    m_flashTimer.setSingleShot(true);
    connect(&m_flashTimer, &QTimer::timeout, this, [this]() {
        m_flashId.clear();
        update();
    });
}

void LayoutWindow::setLayout(const LayoutDocument& layout)
{
    m_layout = layout;
    m_hoverId.clear();
    m_hoverProgress = 0.0;
    setWindowTitle(QStringLiteral("Gazer — %1").arg(
        m_layout.isValid() ? m_layout.name : QStringLiteral("Layout")));
    rebuildCellGeometry();
    update();
}

void LayoutWindow::clearLayout()
{
    m_layout = {};
    m_itemLocalRects.clear();
    m_hoverId.clear();
    m_hoverProgress = 0.0;
    update();
}

void LayoutWindow::onActiveLayoutChanged(const gazer::LayoutDocument& layout)
{
    setLayout(layout);
}

void LayoutWindow::setHoverState(const QString& itemId, double progress)
{
    if (m_hoverId == itemId && qFuzzyCompare(m_hoverProgress + 1.0, progress + 1.0)) {
        return;
    }
    m_hoverId = itemId;
    m_hoverProgress = progress;
    update();
}

void LayoutWindow::setProgressVisuals(const ProgressVisuals& visuals)
{
    m_progressVisuals = visuals;
    update();
}

void LayoutWindow::setActiveItemIds(const QSet<QString>& activeIds)
{
    if (m_activeItemIds == activeIds) {
        return;
    }
    m_activeItemIds = activeIds;
    update();
}

void LayoutWindow::flashItem(const QString& itemId)
{
    if (itemId.isEmpty()) {
        return;
    }
    const LayoutItem* item = m_layout.findItem(itemId);
    const ProgressVisuals v = visualsForItem(item);
    if (!v.flashOnComplete) {
        return;
    }
    m_flashId = itemId;
    m_flashTimer.start(qMax(40, v.flashMs));
    update();
}

ProgressVisuals LayoutWindow::visualsForItem(const LayoutItem* item) const
{
    ProgressVisuals v = m_progressVisuals;
    // Layout-level override when dwell section present in JSON.
    if (m_layout.dwell.sectionPresent) {
        v = v.mergedWith(m_layout.dwell);
    }
    // Item-level override wins over layout.
    if (item && item->dwell.sectionPresent) {
        v = v.mergedWith(item->dwell);
    }
    return v;
}

void LayoutWindow::paintProgressChrome(QPainter& p, const QRectF& r, bool hovered,
                                       double progress, const ProgressVisuals& visuals)
{
    const bool flashing = !m_flashId.isEmpty()
                          && m_itemLocalRects.contains(m_flashId)
                          && m_itemLocalRects.value(m_flashId) == r;

    if (flashing) {
        p.setPen(QPen(visuals.flashBorderColor, 3.5));
        p.setBrush(visuals.flashFillColor);
        p.drawRoundedRect(r, 12, 12);
    }

    if (!hovered || progress <= 0.0) {
        return;
    }

    if (visuals.fillBackground) {
        // Grow from center outward.
        QColor fill = visuals.fillColor;
        fill.setAlpha(qBound(0, int(fill.alpha() * progress + 20 * progress), 255));
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        const double cx = r.center().x();
        const double cy = r.center().y();
        const double hw = r.width() * 0.5 * progress;
        const double hh = r.height() * 0.5 * progress;
        p.drawRoundedRect(QRectF(cx - hw, cy - hh, hw * 2.0, hh * 2.0), 12, 12);
    }

    if (visuals.border) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(visuals.borderColor, 2.0 + 2.0 * progress));
        p.drawRoundedRect(r.adjusted(2, 2, -2, -2), 12, 12);
    }

    if (visuals.radial) {
        const double pad = 8.0;
        QRectF arcRect = r.adjusted(pad, pad, -pad, -pad);
        const double side = qMin(arcRect.width(), arcRect.height()) * 0.45;
        arcRect = QRectF(r.center().x() - side / 2.0, r.center().y() - side / 2.0, side, side);
        p.setBrush(Qt::NoBrush);
        QColor ring = visuals.progressColor;
        ring.setAlpha(80);
        p.setPen(QPen(ring, 4.0));
        p.drawEllipse(arcRect);
        p.setPen(QPen(visuals.progressColor, 4.0));
        const int span = static_cast<int>(-360 * 16 * progress);
        p.drawArc(arcRect, 90 * 16, span);
    }
}

void LayoutWindow::applyTopmost()
{
    applyOverlayWindowChrome(this, /*excludeFromCapture=*/false);
    raise();
}

void LayoutWindow::showAndRaise()
{
    if (windowState() & Qt::WindowMinimized) {
        setWindowState(windowState() & ~Qt::WindowMinimized);
    }
    setVisible(true);
    show();
    applyTopmost();

#ifdef Q_OS_WIN
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    if (hwnd) {
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
#endif
}

QString LayoutWindow::hitTestGlobal(const QPointF& screenPoint) const
{
    if (!m_layout.isValid() || !isVisible()) {
        return {};
    }
    // Prefer last matching interactive item (paint order ≈ visual stack).
    const QPointF local(screenPoint.x() - boardTopLeftGlobal().x(),
                        screenPoint.y() - boardTopLeftGlobal().y());
    QString hit;
    for (const LayoutItem& item : m_layout.items) {
        if (!item.interactive) {
            continue;
        }
        const QRectF r = m_itemLocalRects.value(item.id);
        if (!r.isEmpty() && r.contains(local)) {
            hit = item.id;
        }
    }
    return hit;
}

void LayoutWindow::rebuildCellGeometry()
{
    m_itemLocalRects = LayoutGeometry::itemRects(m_layout, width(), height());
}

void LayoutWindow::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    rebuildCellGeometry();
}

void LayoutWindow::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    applyTopmost();
}

void LayoutWindow::closeEvent(QCloseEvent* event)
{
    hide();
    event->ignore();
    emit closeRequested();
}

void LayoutWindow::mouseReleaseEvent(QMouseEvent* event)
{
    QWidget::mouseReleaseEvent(event);
    if (!event || event->button() != Qt::LeftButton || !m_layout.isValid()) {
        return;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPointF global = event->globalPosition();
#else
    const QPointF global = event->globalPos();
#endif
    const QString id = hitTestGlobal(global);
    if (!id.isEmpty()) {
        emit itemClicked(id);
    }
}

void LayoutWindow::setTheme(const ThemeColors& theme)
{
    m_theme = theme;
    update();
}

void LayoutWindow::paintDefault(QPainter& p)
{
    p.fillRect(rect(), m_theme.bgMain);
    p.setPen(QPen(m_theme.accent, 2.0));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0), 12, 12);

    if (!m_layout.isValid()) {
        p.setPen(m_theme.danger);
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("No layout loaded"));
        return;
    }

    QFont labelFont(QStringLiteral("Segoe UI"), 14, QFont::DemiBold);
    p.setFont(labelFont);

    for (const LayoutItem& item : m_layout.items) {
        const QRectF r = m_itemLocalRects.value(item.id);
        if (r.isEmpty()) {
            continue;
        }

        const bool hovered = (item.id == m_hoverId);
        const bool active = m_activeItemIds.contains(item.id);
        QColor bg = item.style.background.value_or(m_theme.cellBg);
        QColor fg = item.style.foreground.value_or(m_theme.text);
        if (active) {
            bg = m_theme.accent;
            fg = m_theme.bgMain;
        }
        if (hovered) {
            bg = bg.lighter(120);
        }

        p.setPen(QPen(active ? m_theme.accentHover : m_theme.border, active ? 2.5 : 1.5));
        p.setBrush(bg);
        p.drawRoundedRect(r, 10, 10);
        if (active) {
            // Corner indicator chip
            p.setPen(Qt::NoPen);
            p.setBrush(m_theme.accentHover);
            p.drawEllipse(QRectF(r.right() - 16, r.top() + 6, 10, 10));
        }
        paintProgressChrome(p, r, hovered, m_hoverProgress, visualsForItem(&item));

        p.setPen(fg);
        if (!item.icon.isEmpty()) {
            const QRectF iconR(r.left() + 6, r.top() + 6, r.width() - 12, r.height() * 0.52);
            MouseIcons::paint(p, item.icon, iconR, fg);
            p.setFont(QFont(QStringLiteral("Segoe UI"), 11, QFont::DemiBold));
            p.drawText(r.adjusted(6, r.height() * 0.52, -6, -6),
                       Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, item.label);
        } else {
            const QString text =
                item.caption.isEmpty() ? item.label
                                       : QStringLiteral("%1\n%2").arg(item.label, item.caption);
            p.drawText(r.adjusted(8, 8, -8, -8), Qt::AlignCenter | Qt::TextWordWrap, text);
        }
    }
}

void LayoutWindow::paintFluent(QPainter& p)
{
    // Soft acrylic-like panel from theme
    QLinearGradient bg(0, 0, 0, height());
    bg.setColorAt(0.0, m_theme.bgSurface);
    bg.setColorAt(1.0, m_theme.bgMain);
    p.fillRect(rect(), bg);

    QColor accentBorder = m_theme.accent;
    accentBorder.setAlpha(90);
    p.setPen(QPen(accentBorder, 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5), 18, 18);

    // Title strip
    QFont titleFont(QStringLiteral("Segoe UI"), 16, QFont::DemiBold);
    p.setFont(titleFont);
    p.setPen(m_theme.text);
    p.drawText(QRect(24, 14, width() - 48, 28), Qt::AlignLeft | Qt::AlignVCenter, m_layout.name);
    if (!m_layout.description.isEmpty()) {
        p.setFont(QFont(QStringLiteral("Segoe UI"), 10));
        p.setPen(m_theme.textSecondary);
        p.drawText(QRect(24, 40, width() - 48, 22), Qt::AlignLeft | Qt::AlignVCenter,
                   m_layout.description);
    }

    if (!m_layout.isValid()) {
        p.setPen(m_theme.danger);
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("No layout loaded"));
        return;
    }

    for (const LayoutItem& item : m_layout.items) {
        const QRectF r = m_itemLocalRects.value(item.id);
        if (r.isEmpty()) {
            continue;
        }

        const bool isLabel = !item.interactive;
        const bool hovered = item.interactive && (item.id == m_hoverId);
        QColor fg = item.style.foreground.value_or(QColor(244, 246, 252));

        if (isLabel) {
            // Static text cell — no activator chrome.
            const bool isValue = !item.settingKey.isEmpty()
                                 || item.id.contains(QLatin1String("value"))
                                 || item.id.contains(QLatin1String("display"));
            const bool isInputBox = item.id.contains(QLatin1String("display"))
                                    || item.id.contains(QLatin1String("input"));
            if (isInputBox) {
                p.setPen(QPen(QColor(96, 205, 255, 120), 1.5));
                p.setBrush(QColor(16, 20, 30));
                p.drawRoundedRect(r, 12, 12);
                p.setPen(QColor(120, 230, 255));
                p.setFont(QFont(QStringLiteral("Segoe UI Semibold"), 22, QFont::Bold));
                p.drawText(r.adjusted(12, 8, -12, -8), Qt::AlignCenter | Qt::TextWordWrap,
                           item.label);
            } else if (isValue) {
                p.setPen(QColor(120, 210, 255));
                p.setFont(QFont(QStringLiteral("Segoe UI Semibold"), 16, QFont::Bold));
                p.drawText(r.adjusted(8, 6, -8, -6), Qt::AlignCenter | Qt::TextWordWrap,
                           item.label);
            } else {
                // Title / description labels
                p.setPen(fg);
                p.setFont(QFont(QStringLiteral("Segoe UI"), 12, QFont::DemiBold));
                if (!item.caption.isEmpty()) {
                    const QRectF titleR(r.left() + 10, r.top() + 6, r.width() - 20,
                                        r.height() * 0.42);
                    p.drawText(titleR, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap,
                               item.label);
                    p.setPen(QColor(150, 160, 180));
                    p.setFont(QFont(QStringLiteral("Segoe UI"), 10));
                    const QRectF capR(r.left() + 10, r.center().y(), r.width() - 20,
                                      r.height() * 0.45);
                    p.drawText(capR, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, item.caption);
                } else {
                    p.setPen(QColor(170, 180, 200));
                    p.setFont(QFont(QStringLiteral("Segoe UI"), 11));
                    p.drawText(r.adjusted(10, 6, -10, -6),
                               Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, item.label);
                }
            }
            continue;
        }

        // Interactive card button
        const bool active = m_activeItemIds.contains(item.id);
        QColor bg = item.style.background.value_or(QColor(48, 54, 72));
        if (active) {
            bg = QColor(0, 130, 150);
        }
        if (hovered) {
            bg = bg.lighter(118);
        }

        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, hovered || active ? 80 : 40));
        p.drawRoundedRect(r.translated(0, 2), 14, 14);

        p.setBrush(bg);
        p.setPen(QPen(active ? QColor(0, 240, 255, 255)
                             : (hovered ? QColor(0, 200, 255, 220) : QColor(255, 255, 255, 28)),
                      active ? 2.8 : (hovered ? 2.0 : 1.0)));
        p.drawRoundedRect(r, 14, 14);
        if (active) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 255, 200));
            p.drawEllipse(QRectF(r.right() - 18, r.top() + 8, 11, 11));
            // Soft inner glow
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(0, 255, 220, 90), 4.0));
            p.drawRoundedRect(r.adjusted(3, 3, -3, -3), 12, 12);
        }
        paintProgressChrome(p, r, hovered, m_hoverProgress, visualsForItem(&item));

        p.setPen(active ? QColor(255, 255, 255) : fg);
        if (!item.icon.isEmpty()) {
            const QRectF iconR(r.left() + 8, r.top() + 8, r.width() - 16, r.height() * 0.5);
            MouseIcons::paint(p, item.icon, iconR, active ? QColor(255, 255, 255) : fg);
            p.setFont(QFont(QStringLiteral("Segoe UI"), 11, QFont::DemiBold));
            p.drawText(r.adjusted(8, r.height() * 0.52, -8, -8),
                       Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, item.label);
        } else {
            p.setFont(QFont(QStringLiteral("Segoe UI"), 13, QFont::DemiBold));
            p.drawText(r.adjusted(10, 10, -10, -10), Qt::AlignCenter | Qt::TextWordWrap,
                       item.label);
        }
    }
}

void LayoutWindow::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    if (m_layout.uiStyle == LayoutUiStyle::Fluent) {
        paintFluent(p);
    } else {
        paintDefault(p);
    }
}

} // namespace gazer
