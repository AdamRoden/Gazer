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
#include <QPainterPath>
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

void LayoutQuickWindow::setPreviewColor(const QColor& color)
{
    if (m_previewColor == color) {
        return;
    }
    m_previewColor = color.isValid() ? color : ThemeColors::defaultProgressColor();
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

LayoutItemStyle LayoutQuickWindow::resolvedItemStyle(const LayoutItem& item) const
{
    return m_layout.style.withOverrides(item.style);
}

void LayoutQuickWindow::keepAboveTaskbar()
{
    if (!isVisible() || !m_layout.placement.aboveTaskbar) {
        return;
    }
    raiseAboveTaskbar(this);
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
        p.setPen(QPen(visuals.flashColor, 3.5));
        p.setBrush(visuals.flashColor);
        p.drawRoundedRect(r, radius, radius);
    }

    if (!hovered || progress <= 0.0) {
        return;
    }
    paintProgress(p, r, progress, visuals, ProgressShape::RoundedRect, radius);
}

void LayoutQuickWindow::paintPreviewSwatch(QPainter& p, const QRectF& r, double radius)
{
    QPainterPath clip;
    clip.addRoundedRect(r, radius, radius);
    p.save();
    p.setClipPath(clip);
    const int cell = 10;
    for (int y = int(r.top()); y < int(r.bottom()); y += cell) {
        for (int x = int(r.left()); x < int(r.right()); x += cell) {
            const bool lite = ((x / cell) + (y / cell)) % 2 == 0;
            p.fillRect(x, y, cell, cell, lite ? QColor(200, 200, 200) : QColor(140, 140, 140));
        }
    }
    p.setPen(Qt::NoPen);
    p.setBrush(m_previewColor);
    p.drawRoundedRect(r, radius, radius);
    p.restore();
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 80), 1.5));
    p.drawRoundedRect(r.adjusted(1, 1, -1, -1), radius, radius);
}

void LayoutQuickWindow::paintToggleSwitch(QPainter& p, const QRectF& cell, bool on)
{
    const qreal h = 20.0;
    const qreal w = 36.0;
    const qreal m = 8.0;
    const QRectF track(cell.right() - m - w, cell.top() + m, w, h);
    const qreal cr = h * 0.5;

    const QColor trackC = on ? m_theme.accentHover : m_theme.cellActive;
    const QColor thumbC = on ? m_theme.bgMain : m_theme.bgSurface;

    p.setPen(QPen(m_theme.border, 1.0));
    p.setBrush(trackC);
    p.drawRoundedRect(track, cr, cr);

    const qreal inset = 2.0;
    const qreal th = h - inset * 2.0;
    const qreal x = on ? (track.right() - inset - th) : (track.left() + inset);
    p.setPen(QPen(m_theme.border, 1.0));
    p.setBrush(thumbC);
    p.drawEllipse(QRectF(x, track.top() + inset, th, th));
}

void LayoutQuickWindow::paintSliderTrack(QPainter& p, const QRectF& r, const QString& channel,
                                         double radius)
{
    QColor base = m_previewColor.isValid() ? m_previewColor : ThemeColors::defaultProgressColor();
    int h = 0, s = 0, v = 0, a = 255;
    base.getHsv(&h, &s, &v, &a);
    if (h < 0) {
        h = 0;
    }
    const int cr = base.red();
    const int cg = base.green();
    const int cb = base.blue();

    QPainterPath clip;
    clip.addRoundedRect(r, radius, radius);
    p.save();
    p.setClipPath(clip);

    const QString ch = channel.toLower();
    if (ch == QLatin1String("a") || ch == QLatin1String("alpha")) {
        const int cell = 8;
        for (int y = int(r.top()); y < int(r.bottom()); y += cell) {
            for (int x = int(r.left()); x < int(r.right()); x += cell) {
                const bool lite = ((x / cell) + (y / cell)) % 2 == 0;
                p.fillRect(x, y, cell, cell, lite ? QColor(210, 210, 210) : QColor(150, 150, 150));
            }
        }
    }

    QLinearGradient g(r.left(), r.center().y(), r.right(), r.center().y());
    if (ch == QLatin1String("h") || ch == QLatin1String("hue")) {
        for (int i = 0; i <= 6; ++i) {
            g.setColorAt(i / 6.0, QColor::fromHsv(qMin(359, i * 60), 255, 255));
        }
    } else if (ch == QLatin1String("s") || ch == QLatin1String("sat")) {
        g.setColorAt(0.0, QColor::fromHsv(h, 0, v));
        g.setColorAt(1.0, QColor::fromHsv(h, 255, v));
    } else if (ch == QLatin1String("v") || ch == QLatin1String("val")) {
        g.setColorAt(0.0, QColor::fromHsv(h, s, 0));
        g.setColorAt(1.0, QColor::fromHsv(h, s, 255));
    } else if (ch == QLatin1String("r") || ch == QLatin1String("red")) {
        g.setColorAt(0.0, QColor(0, cg, cb));
        g.setColorAt(1.0, QColor(255, cg, cb));
    } else if (ch == QLatin1String("g") || ch == QLatin1String("green")) {
        g.setColorAt(0.0, QColor(cr, 0, cb));
        g.setColorAt(1.0, QColor(cr, 255, cb));
    } else if (ch == QLatin1String("b") || ch == QLatin1String("blue")) {
        g.setColorAt(0.0, QColor(cr, cg, 0));
        g.setColorAt(1.0, QColor(cr, cg, 255));
    } else {
        QColor clear = base;
        clear.setAlpha(0);
        QColor solid = base;
        solid.setAlpha(255);
        g.setColorAt(0.0, clear);
        g.setColorAt(1.0, solid);
    }
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawRoundedRect(r, radius, radius);
    p.restore();
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 70), 1.2));
    p.drawRoundedRect(r.adjusted(1, 1, -1, -1), radius, radius);
}

void LayoutQuickWindow::paintCell(QPainter& p, const LayoutItem& item, const QRectF& r, bool fluent)
{
    const LayoutItemStyle st = resolvedItemStyle(item);
    const QString role = item.role.toLower();
    const double radius = st.radius.value_or(fluent ? 14.0 : 10.0);
    if (role == QLatin1String("slider")) {
        paintSliderTrack(p, r, item.caption.isEmpty() ? item.id : item.caption, radius);
        if (!item.label.isEmpty()) {
            p.setPen(QColor(255, 255, 255, 230));
            p.setFont(QFont(QStringLiteral("Segoe UI"), 11, QFont::DemiBold));
            p.drawText(r.adjusted(8, 4, -8, -4), Qt::AlignLeft | Qt::AlignVCenter, item.label);
        }
        return;
    }
    if (role == QLatin1String("preview")) {
        paintPreviewSwatch(p, r, radius);
        return;
    }

    const bool hovered = item.interactive && (item.id == m_hoverId);
    const bool active = m_activeItemIds.contains(item.id);
    const double borderW = st.borderWidth.value_or(active ? 2.5 : 1.5);

    const bool hasSwitch = !item.activeState.isEmpty();
    QColor bg = st.background.value_or(m_theme.cellBg);
    QColor fg = st.foreground.value_or(m_theme.text);
    QColor border = st.borderColor.value_or((active && !hasSwitch) ? m_theme.accentHover
                                                                   : m_theme.border);

    if (active && !hasSwitch) {
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

    if (hasSwitch) {
        paintToggleSwitch(p, r, active);
    }

    paintProgressChrome(p, r, hovered, m_hoverProgress, visualsForItem(&item), radius);

    const qreal textRight = hasSwitch ? 48.0 : 8.0;
    p.setPen(fg);
    if (!item.icon.isEmpty()) {
        const QRectF iconR(r.left() + 6, r.top() + 6, r.width() - 12 - (hasSwitch ? 40.0 : 0.0),
                           r.height() * 0.52);
        MouseIcons::paint(p, item.icon, iconR, fg);
        p.setFont(QFont(QStringLiteral("Segoe UI"), 11, QFont::DemiBold));
        p.drawText(r.adjusted(6, r.height() * 0.52, -textRight, -6),
                   Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, item.label);
    } else {
        p.setFont(QFont(QStringLiteral("Segoe UI"), fluent ? 13 : 14, QFont::DemiBold));
        const QString text =
            item.caption.isEmpty() ? item.label
                                   : QStringLiteral("%1\n%2").arg(item.label, item.caption);
        p.drawText(r.adjusted(8, 8, -textRight, -8), Qt::AlignCenter | Qt::TextWordWrap, text);
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

        if (item.role.compare(QLatin1String("slider"), Qt::CaseInsensitive) == 0
            || item.role.compare(QLatin1String("preview"), Qt::CaseInsensitive) == 0) {
            paintCell(p, item, r, true);
            continue;
        }

        if (!item.interactive) {
            const LayoutItemStyle st = resolvedItemStyle(item);
            const QString role = item.role.toLower();
            const bool isInput = role == QLatin1String("display") || role == QLatin1String("input");
            const bool isValue = role == QLatin1String("value") || !item.settingKey.isEmpty();
            QColor fg = st.foreground.value_or(isInput || isValue ? m_theme.accent : m_theme.text);
            const double radius = st.radius.value_or(12.0);
            if (st.background && st.background->alpha() > 0) {
                p.setBrush(*st.background);
                p.setPen(QPen(st.borderColor.value_or(m_theme.accent),
                              st.borderWidth.value_or(1.5)));
                p.drawRoundedRect(r, radius, radius);
            } else if (isInput) {
                p.setPen(QPen(m_theme.accent, 1.5));
                p.setBrush(m_theme.bgMain);
                p.drawRoundedRect(r, radius, radius);
            }
            if (isInput) {
                p.setPen(fg);
                p.setFont(QFont(QStringLiteral("Segoe UI Semibold"), 22, QFont::Bold));
                p.drawText(r.adjusted(12, 8, -12, -8), Qt::AlignCenter | Qt::TextWordWrap,
                           item.label);
            } else if (isValue) {
                p.setPen(fg);
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
                    p.setPen(m_theme.textSecondary);
                    p.setFont(QFont(QStringLiteral("Segoe UI"), 10));
                    const QRectF capR(r.left() + 10, r.center().y(), r.width() - 20,
                                      r.height() * 0.45);
                    p.drawText(capR, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, item.caption);
                } else {
                    p.setPen(m_theme.textSecondary);
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
