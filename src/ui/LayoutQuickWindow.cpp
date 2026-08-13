#include "ui/LayoutQuickWindow.h"

#include "layout/LayoutGeometry.h"
#include "layout/LayoutVisibility.h"
#include "ui/MouseIcons.h"
#include "utils/WinOverlay.h"

#include <QCloseEvent>
#include <QFont>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QQuickItem>
#include <QQuickPaintedItem>
#include <QScreen>
#include <QTimer>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

namespace gazer {

class LayoutBoardItem final : public QQuickPaintedItem {
public:
    explicit LayoutBoardItem(LayoutQuickWindow* host, QQuickItem* parent)
        : QQuickPaintedItem(parent)
        , m_host(host)
    {
        setAntialiasing(true);
        setOpaquePainting(false);
        setFillColor(Qt::transparent);
        setAcceptedMouseButtons(Qt::LeftButton);
        setAcceptHoverEvents(false);
    }

    void paint(QPainter* p) override
    {
        if (!p || !m_host) {
            return;
        }
        m_host->paintBoard(*p);
    }

protected:
    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (!event || !m_host || event->button() != Qt::LeftButton) {
            return;
        }
        const QPointF global = event->globalPosition();
        const QString id = m_host->hitTestGlobal(global);
        if (!id.isEmpty()) {
            emit m_host->itemClicked(id);
        }
    }

private:
    LayoutQuickWindow* m_host = nullptr;
};

LayoutQuickWindow::LayoutQuickWindow(QWindow* parent)
    : QQuickWindow(parent)
{
    setColor(Qt::transparent);
    setFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool
             | Qt::WindowDoesNotAcceptFocus);
    setTitle(QStringLiteral("Gazer — Layout"));
    QQuickWindow::setMinimumSize(QSize(320, 160));
    QQuickWindow::resize(QSize(1000, 560));

    m_board = new LayoutBoardItem(this, contentItem());
    m_board->setSize(QSizeF(width(), height()));

    m_flashTimer.setSingleShot(true);
    connect(&m_flashTimer, &QTimer::timeout, this, [this]() {
        m_flashId.clear();
        if (m_board) {
            m_board->update();
        }
    });
    connect(this, &QQuickWindow::widthChanged, this, &LayoutQuickWindow::syncBoardSize);
    connect(this, &QQuickWindow::heightChanged, this, &LayoutQuickWindow::syncBoardSize);
    connect(this, &QQuickWindow::visibleChanged, this, [this]() {
        if (isVisible()) {
            applyTopmost();
        }
    });
}

void LayoutQuickWindow::closeEvent(QCloseEvent* event)
{
    if (event) {
        event->ignore();
    }
    hide();
    emit closeRequested();
}

void LayoutQuickWindow::syncBoardSize()
{
    if (!m_board) {
        return;
    }
    m_board->setSize(QSizeF(width(), height()));
    rebuildCellGeometry();
    m_board->update();
}

void LayoutQuickWindow::setBoardOpacity(double opacity)
{
    m_boardOpacity = qBound(0.0, opacity, 1.0);
    setOpacity(m_boardOpacity);
}

void LayoutQuickWindow::setLayout(const LayoutDocument& layout)
{
    m_layout = layout;
    m_hoverId.clear();
    m_hoverProgress = 0.0;
    setTitle(QStringLiteral("Gazer — %1").arg(
        m_layout.isValid() ? m_layout.name : QStringLiteral("Layout")));
    rebuildCellGeometry();
    if (m_board) {
        m_board->update();
    }
}

void LayoutQuickWindow::clearLayout()
{
    m_layout = {};
    m_itemLocalRects.clear();
    m_hoverId.clear();
    m_hoverProgress = 0.0;
    if (m_board) {
        m_board->update();
    }
}

void LayoutQuickWindow::setHoverState(const QString& itemId, double progress)
{
    if (m_hoverId == itemId && qFuzzyCompare(m_hoverProgress + 1.0, progress + 1.0)) {
        return;
    }
    m_hoverId = itemId;
    m_hoverProgress = progress;
    if (m_board) {
        m_board->update();
    }
}

void LayoutQuickWindow::setProgressVisuals(const ProgressVisuals& visuals)
{
    m_progressVisuals = visuals;
    if (m_board) {
        m_board->update();
    }
}

void LayoutQuickWindow::setTheme(const ThemeColors& theme)
{
    m_theme = theme;
    if (m_board) {
        m_board->update();
    }
}

void LayoutQuickWindow::setActiveItemIds(const QSet<QString>& activeIds)
{
    if (m_activeItemIds == activeIds) {
        return;
    }
    m_activeItemIds = activeIds;
    if (m_board) {
        m_board->update();
    }
}

void LayoutQuickWindow::setPropertyContext(const QVariantMap& props)
{
    if (m_props == props) {
        return;
    }
    m_props = props;
    rebuildCellGeometry();
    if (m_board) {
        m_board->update();
    }
}

void LayoutQuickWindow::flashItem(const QString& itemId)
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
    if (m_board) {
        m_board->update();
    }
}

ProgressVisuals LayoutQuickWindow::visualsForItem(const LayoutItem* item) const
{
    ProgressVisuals v = m_progressVisuals;
    if (m_layout.dwell.sectionPresent) {
        v = v.mergedWith(m_layout.dwell);
    }
    if (item && item->dwell.sectionPresent) {
        v = v.mergedWith(item->dwell);
    }
    return v;
}

bool LayoutQuickWindow::itemShown(const LayoutItem& item) const
{
    return itemIsShown(item, m_props);
}

void LayoutQuickWindow::keepAboveTaskbar()
{
    if (!isVisible() || !m_layout.placement.aboveTaskbar) {
        return;
    }
    raiseAboveTaskbar(this);
}

void LayoutQuickWindow::assertAboveTaskbar()
{
    keepAboveTaskbar();
}

void LayoutQuickWindow::applyTopmost()
{
    applyOverlayWindowChrome(this, /*excludeFromCapture=*/false);
    if (m_layout.placement.aboveTaskbar) {
        raiseAboveTaskbar(this);
        return;
    }
    raise();
}

void LayoutQuickWindow::showAndRaise()
{
    setVisibility(QWindow::AutomaticVisibility);
    show();
    applyTopmost();

#ifdef Q_OS_WIN
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    if (hwnd) {
        ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    }
#endif
    keepAboveTaskbar();
}

QString LayoutQuickWindow::hitTestGlobal(const QPointF& screenPoint) const
{
    if (!m_layout.isValid() || !isVisible()) {
        return {};
    }
    const QPointF local(screenPoint.x() - boardTopLeftGlobal().x(),
                        screenPoint.y() - boardTopLeftGlobal().y());
    QString hit;
    for (const LayoutItem& item : m_layout.items) {
        if (!itemShown(item) || !item.interactive) {
            continue;
        }
        const QRectF r = m_itemLocalRects.value(item.id);
        if (!r.isEmpty() && r.contains(local)) {
            hit = item.id;
        }
    }
    return hit;
}

void LayoutQuickWindow::rebuildCellGeometry()
{
    m_itemLocalRects = LayoutGeometry::itemRects(m_layout, width(), height());
}

void LayoutQuickWindow::paintProgressChrome(QPainter& p, const QRectF& r, bool hovered,
                                            double progress, const ProgressVisuals& visuals,
                                            double radius)
{
    const bool flashing = !m_flashId.isEmpty() && m_itemLocalRects.contains(m_flashId)
                          && m_itemLocalRects.value(m_flashId) == r;

    if (flashing) {
        p.setPen(QPen(visuals.flashBorderColor, 3.5));
        p.setBrush(visuals.flashFillColor);
        p.drawRoundedRect(r, radius, radius);
    }

    if (!hovered || progress <= 0.0) {
        return;
    }

    if (visuals.fillBackground) {
        QColor fill = visuals.fillColor;
        fill.setAlpha(qBound(0, int(fill.alpha() * progress + 20 * progress), 255));
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        const double cx = r.center().x();
        const double cy = r.center().y();
        const double hw = r.width() * 0.5 * progress;
        const double hh = r.height() * 0.5 * progress;
        p.drawRoundedRect(QRectF(cx - hw, cy - hh, hw * 2.0, hh * 2.0), radius, radius);
    }

    if (visuals.border) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(visuals.borderColor, 2.0 + 2.0 * progress));
        p.drawRoundedRect(r.adjusted(2, 2, -2, -2), radius, radius);
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

void LayoutQuickWindow::paintCell(QPainter& p, const LayoutItem& item, const QRectF& r, bool fluent)
{
    const bool hovered = item.interactive && (item.id == m_hoverId);
    const bool active = m_activeItemIds.contains(item.id);
    const double radius = item.style.radius.value_or(fluent ? 14.0 : 10.0);
    const double borderW = item.style.borderWidth.value_or(active ? 2.5 : 1.5);

    QColor bg = item.style.background.value_or(fluent ? QColor(48, 54, 72) : m_theme.cellBg);
    QColor fg = item.style.foreground.value_or(m_theme.text);
    QColor border = item.style.borderColor.value_or(active ? m_theme.accentHover : m_theme.border);

    if (active) {
        bg = m_theme.accent;
        fg = m_theme.bgMain;
    }
    if (hovered && bg.alpha() > 0) {
        bg = bg.lighter(118);
    }

    if (bg.alpha() > 0) {
        p.setBrush(bg);
    } else {
        p.setBrush(Qt::NoBrush);
    }
    p.setPen(QPen(border, borderW));
    p.drawRoundedRect(r, radius, radius);

    if (active) {
        p.setPen(Qt::NoPen);
        p.setBrush(m_theme.accentHover);
        p.drawEllipse(QRectF(r.right() - 16, r.top() + 6, 10, 10));
    }

    paintProgressChrome(p, r, hovered, m_hoverProgress, visualsForItem(&item), radius);

    p.setPen(fg);
    if (!item.icon.isEmpty()) {
        const QRectF iconR(r.left() + 6, r.top() + 6, r.width() - 12, r.height() * 0.52);
        MouseIcons::paint(p, item.icon, iconR, fg);
        p.setFont(QFont(QStringLiteral("Segoe UI"), 11, QFont::DemiBold));
        p.drawText(r.adjusted(6, r.height() * 0.52, -6, -6),
                   Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, item.label);
    } else {
        p.setFont(QFont(QStringLiteral("Segoe UI"), fluent ? 13 : 14, QFont::DemiBold));
        const QString text =
            item.caption.isEmpty() ? item.label
                                   : QStringLiteral("%1\n%2").arg(item.label, item.caption);
        p.drawText(r.adjusted(8, 8, -8, -8), Qt::AlignCenter | Qt::TextWordWrap, text);
    }
}

void LayoutQuickWindow::paintDefault(QPainter& p)
{
    const auto& ws = m_layout.placement.style;
    const double winR = ws.radius.value_or(12.0);
    const double winBw = ws.borderWidth.value_or(0.0);
    QColor bg = ws.background.value_or(m_theme.bgMain);
    QColor border = ws.borderColor.value_or(m_theme.accent);

    if (bg.alpha() > 0) {
        p.setBrush(bg);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(QRectF(0, 0, width(), height()), winR, winR);
    }
    if (border.alpha() > 0 && winBw > 0) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(border, winBw));
        p.drawRoundedRect(QRectF(0, 0, width(), height()).adjusted(1.0, 1.0, -1.0, -1.0), winR,
                          winR);
    }

    if (!m_layout.isValid()) {
        p.setPen(m_theme.danger);
        p.drawText(QRect(0, 0, width(), height()), Qt::AlignCenter,
                   QStringLiteral("No layout loaded"));
        return;
    }

    for (const LayoutItem& item : m_layout.items) {
        if (!itemShown(item)) {
            continue;
        }
        const QRectF r = m_itemLocalRects.value(item.id);
        if (r.isEmpty()) {
            continue;
        }
        paintCell(p, item, r, false);
    }
}

void LayoutQuickWindow::paintFluent(QPainter& p)
{
    const auto& ws = m_layout.placement.style;
    const double winR = ws.radius.value_or(18.0);
    const double winBw = ws.borderWidth.value_or(0.0);
    QColor bgTop = ws.background.value_or(m_theme.bgSurface);
    QColor bgBot = ws.background.value_or(m_theme.bgMain);
    QColor border = ws.borderColor.value_or(m_theme.accent);

    if (bgTop.alpha() > 0 || bgBot.alpha() > 0) {
        QLinearGradient bg(0, 0, 0, height());
        bg.setColorAt(0.0, bgTop);
        bg.setColorAt(1.0, bgBot);
        p.setBrush(bg);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(QRectF(0, 0, width(), height()), winR, winR);
    }
    if (border.alpha() > 0 && winBw > 0) {
        QColor accentBorder = border;
        if (!ws.borderColor) {
            accentBorder.setAlpha(90);
        }
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(accentBorder, winBw));
        p.drawRoundedRect(QRectF(0, 0, width(), height()).adjusted(1.5, 1.5, -1.5, -1.5), winR,
                          winR);
    }

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
        p.drawText(QRect(0, 0, width(), height()), Qt::AlignCenter,
                   QStringLiteral("No layout loaded"));
        return;
    }

    for (const LayoutItem& item : m_layout.items) {
        if (!itemShown(item)) {
            continue;
        }
        const QRectF r = m_itemLocalRects.value(item.id);
        if (r.isEmpty()) {
            continue;
        }

        if (!item.interactive) {
            QColor fg = item.style.foreground.value_or(QColor(244, 246, 252));
            const double radius = item.style.radius.value_or(12.0);
            if (item.style.background && item.style.background->alpha() > 0) {
                p.setBrush(*item.style.background);
                p.setPen(QPen(item.style.borderColor.value_or(QColor(96, 205, 255, 120)),
                              item.style.borderWidth.value_or(1.5)));
                p.drawRoundedRect(r, radius, radius);
            }
            const bool isValue = !item.settingKey.isEmpty()
                                 || item.id.contains(QLatin1String("value"))
                                 || item.id.contains(QLatin1String("display"));
            const bool isInputBox = item.id.contains(QLatin1String("display"))
                                    || item.id.contains(QLatin1String("input"));
            if (isInputBox) {
                if (!item.style.background) {
                    p.setPen(QPen(QColor(96, 205, 255, 120), 1.5));
                    p.setBrush(QColor(16, 20, 30));
                    p.drawRoundedRect(r, radius, radius);
                }
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

        paintCell(p, item, r, true);
    }
}

void LayoutQuickWindow::paintBoard(QPainter& p)
{
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    p.setOpacity(m_boardOpacity);

    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(QRect(0, 0, width(), height()), Qt::transparent);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);

    if (m_layout.uiStyle == LayoutUiStyle::Fluent) {
        paintFluent(p);
    } else {
        paintDefault(p);
    }
}

} // namespace gazer
