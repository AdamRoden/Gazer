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
    , m_glass(this)
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
    connect(&m_glass, &GlassBackdrop::updated, this, [this]() {
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
    m_glass.setActive(m_layout.maxChromeBlur());
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
    m_glass.setActive(0.0);
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

void LayoutQuickWindow::setSliderScrub(const QString& itemId, double t, const QString& valueText,
                                       double dwellProgress)
{
    m_sliderScrubId = itemId;
    m_sliderScrubT = qBound(0.0, t, 1.0);
    m_sliderScrubValue = valueText;
    m_sliderScrubProgress = qBound(0.0, dwellProgress, 1.0);
    if (m_board) {
        m_board->update();
    }
}

void LayoutQuickWindow::setSliderReadout(const QString& itemId, double t, const QString& valueText)
{
    if (itemId.isEmpty()) {
        return;
    }
    m_sliderReadoutT.insert(itemId, qBound(0.0, t, 1.0));
    m_sliderReadoutValue.insert(itemId, valueText);
    if (m_board) {
        m_board->update();
    }
}

void LayoutQuickWindow::clearSliderScrub()
{
    if (m_sliderScrubId.isEmpty()) {
        return;
    }
    m_sliderScrubId.clear();
    m_sliderScrubT = 0.0;
    m_sliderScrubValue.clear();
    m_sliderScrubProgress = 0.0;
    if (m_board) {
        m_board->update();
    }
}

LayoutQuickWindow::SliderVisual LayoutQuickWindow::sliderVisual(const QRectF& cell, bool scrubbing)
{
    SliderVisual g;
    const double headerH = qBound(16.0, cell.height() * 0.30, 22.0);
    g.header = QRectF(cell.left() + 8.0, cell.top() + 2.0, qMax(1.0, cell.width() - 16.0), headerH);
    const double trackH = 40.0;
    g.ringDiameter = scrubbing ? 52.0 : 40.0;
    const double inset = g.ringDiameter * 0.5;
    const double bandTop = g.header.bottom();
    const double bandH = qMax(trackH + 10.0, cell.bottom() - bandTop);
    g.trackCy = bandTop + bandH * 0.5;
    g.track = QRectF(cell.left() + 6.0, g.trackCy - trackH * 0.5, qMax(8.0, cell.width() - 12.0),
                     trackH);
    g.valueLeft = g.track.left() + inset;
    g.valueRight = g.track.right() - inset;
    if (g.valueRight <= g.valueLeft + 1.0) {
        const double mid = g.track.center().x();
        g.valueLeft = mid - 1.0;
        g.valueRight = mid + 1.0;
    }
    return g;
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

bool LayoutQuickWindow::fillChrome(QPainter& p, const QRectF& r, double radius, const QColor& bg,
                                   const LayoutChromeStyle& st, const QColor& bgBot)
{
    if (st.hasBlur()) {
        m_glass.paint(p, r, radius, st.background ? bg : QColor());
        return true;
    }
    if (bgBot.isValid() && (bg.alpha() > 0 || bgBot.alpha() > 0)) {
        QLinearGradient g(r.topLeft(), r.bottomLeft());
        g.setColorAt(0.0, bg);
        g.setColorAt(1.0, bgBot);
        p.setPen(Qt::NoPen);
        p.setBrush(g);
        p.drawRoundedRect(r, radius, radius);
        return true;
    }
    if (bg.alpha() <= 0) {
        return false;
    }
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(r, radius, radius);
    return true;
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
        if (r.isEmpty()) {
            continue;
        }
        if (r.contains(local)) {
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

void LayoutQuickWindow::paintRadioButton(QPainter& p, const QRectF& cell, bool on)
{
    const qreal d = 18.0;
    const qreal m = 8.0;
    const QRectF outer(cell.right() - m - d, cell.top() + m, d, d);
    p.setPen(QPen(on ? m_theme.accent : m_theme.border, 2.0));
    p.setBrush(on ? QColor(m_theme.accent.red(), m_theme.accent.green(), m_theme.accent.blue(), 40)
                  : m_theme.bgMain);
    p.drawEllipse(outer);
    if (on) {
        p.setPen(Qt::NoPen);
        p.setBrush(m_theme.accent);
        p.drawEllipse(outer.adjusted(4.5, 4.5, -4.5, -4.5));
    }
}

namespace {

struct SliderChannelInfo {
    QString name;
    QString value;
    double t = 0.0;
};

SliderChannelInfo sliderChannelInfo(const QString& channel, const QColor& color)
{
    QColor c = color.isValid() ? color : ThemeColors::defaultProgressColor();
    int h = 0, s = 0, v = 0, a = 255;
    c.getHsv(&h, &s, &v, &a);
    if (h < 0) {
        h = 0;
    }
    const QString ch = channel.toLower();
    SliderChannelInfo info;
    if (ch == QLatin1String("h") || ch == QLatin1String("hue")) {
        info.name = QStringLiteral("Hue");
        info.value = QString::number(h);
        info.t = h / 359.0;
    } else if (ch == QLatin1String("s") || ch == QLatin1String("sat")) {
        const int pct = qBound(0, qRound(s / 2.55), 100);
        info.name = QStringLiteral("Saturation");
        info.value = QStringLiteral("%1%").arg(pct);
        info.t = s / 255.0;
    } else if (ch == QLatin1String("v") || ch == QLatin1String("val")) {
        const int pct = qBound(0, qRound(v / 2.55), 100);
        info.name = QStringLiteral("Value");
        info.value = QStringLiteral("%1%").arg(pct);
        info.t = v / 255.0;
    } else if (ch == QLatin1String("r") || ch == QLatin1String("red")) {
        info.name = QStringLiteral("Red");
        info.value = QString::number(c.red());
        info.t = c.red() / 255.0;
    } else if (ch == QLatin1String("g") || ch == QLatin1String("green")) {
        info.name = QStringLiteral("Green");
        info.value = QString::number(c.green());
        info.t = c.green() / 255.0;
    } else if (ch == QLatin1String("b") || ch == QLatin1String("blue")) {
        info.name = QStringLiteral("Blue");
        info.value = QString::number(c.blue());
        info.t = c.blue() / 255.0;
    } else {
        const int pct = qBound(0, qRound(c.alpha() / 2.55), 100);
        info.name = QStringLiteral("Alpha");
        info.value = QStringLiteral("%1%").arg(pct);
        info.t = c.alpha() / 255.0;
    }
    info.t = qBound(0.0, info.t, 1.0);
    return info;
}

} // namespace

QRectF LayoutQuickWindow::sliderActivatorRect(const QRectF& cell, const LayoutItem& item,
                                              bool scrubbing) const
{
    const SliderVisual geom = sliderVisual(cell, scrubbing);
    const QString channel = item.caption.isEmpty() ? item.id : item.caption;
    const double t = (scrubbing && item.id == m_sliderScrubId)
                         ? m_sliderScrubT
                         : sliderChannelInfo(channel, m_previewColor).t;
    const QPointF pos = geom.posAt(t);
    const double pad = 8.0;
    const double d = geom.ringDiameter + pad * 2.0;
    return QRectF(pos.x() - d * 0.5, pos.y() - d * 0.5, d, d);
}

void LayoutQuickWindow::paintSliderTrack(QPainter& p, const QRectF& r, const LayoutItem& item,
                                         bool hovered, double progress)
{
    const QString channel = item.caption.isEmpty() ? item.id : item.caption;
    const bool scrubbing = (item.id == m_sliderScrubId);
    const SliderVisual geom = sliderVisual(r, scrubbing);
    QColor base = m_previewColor.isValid() ? m_previewColor : ThemeColors::defaultProgressColor();
    int h = 0, s = 0, v = 0, a = 255;
    base.getHsv(&h, &s, &v, &a);
    if (h < 0) {
        h = 0;
    }
    const int cr = base.red();
    const int cg = base.green();
    const int cb = base.blue();
    const SliderChannelInfo info = sliderChannelInfo(channel, base);
    double t = scrubbing ? m_sliderScrubT : info.t;
    QString valueText = scrubbing && !m_sliderScrubValue.isEmpty() ? m_sliderScrubValue
                                                                  : info.value;
    if (!scrubbing && m_sliderReadoutT.contains(item.id)) {
        t = m_sliderReadoutT.value(item.id);
        valueText = m_sliderReadoutValue.value(item.id, valueText);
    }
    const QString nameText = item.label.isEmpty() ? info.name : item.label;

    const QString ch = channel.toLower();
    const QRectF track = geom.track;
    const double radius = track.height() * 0.5;

    if (ch == QLatin1String("a") || ch == QLatin1String("alpha")) {
        QPainterPath clip;
        clip.addRoundedRect(track, radius, radius);
        p.save();
        p.setClipPath(clip);
        const int cell = 6;
        for (int y = int(track.top()); y < int(track.bottom()); y += cell) {
            for (int x = int(track.left()); x < int(track.right()); x += cell) {
                const bool lite = ((x / cell) + (y / cell)) % 2 == 0;
                p.fillRect(x, y, cell, cell, lite ? QColor(210, 210, 210) : QColor(150, 150, 150));
            }
        }
        p.restore();
    }

    QLinearGradient g(track.left(), track.center().y(), track.right(), track.center().y());
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
    } else if (ch == QLatin1String("contrast")) {
        g.setColorAt(0.0, m_theme.bgSurface);
        g.setColorAt(0.5, m_theme.cellHover);
        g.setColorAt(1.0, m_theme.accent);
    } else if (ch == QLatin1String("brightness")) {
        g.setColorAt(0.0, QColor(20, 20, 22));
        g.setColorAt(1.0, QColor(240, 240, 242));
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
    p.drawRoundedRect(track, radius, radius);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(255, 255, 255, 70), 1.1));
    p.drawRoundedRect(track.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);

    p.setPen(m_theme.text);
    p.setFont(QFont(QStringLiteral("Segoe UI"), 10, QFont::DemiBold));
    p.drawText(geom.header, Qt::AlignLeft | Qt::AlignVCenter, nameText);
    p.setPen(m_theme.textSecondary);
    p.drawText(geom.header, Qt::AlignRight | Qt::AlignVCenter, valueText);

    const QPointF pos = geom.posAt(t);
    const double ringD = geom.ringDiameter;
    const QRectF ring(pos.x() - ringD * 0.5, pos.y() - ringD * 0.5, ringD, ringD);
    p.setBrush(m_theme.bgMain);
    p.setPen(QPen(Qt::white, scrubbing ? 2.4 : 2.0));
    p.drawEllipse(ring);
    p.setPen(QPen(QColor(0, 0, 0, 90), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(ring.adjusted(1.5, 1.5, -1.5, -1.5));

    const double ringProgress = scrubbing && m_sliderScrubProgress > 0.0 ? m_sliderScrubProgress
                                                                        : (hovered ? progress : 0.0);
    if (scrubbing) {
        p.setPen(m_theme.text);
        p.setFont(QFont(QStringLiteral("Segoe UI Semibold"), 10, QFont::DemiBold));
        p.drawText(ring, Qt::AlignCenter, valueText);
    }
    if (ringProgress > 0.01) {
        paintProgress(p, ring.adjusted(-3, -3, 3, 3), ringProgress, visualsForItem(&item),
                      ProgressShape::Ellipse);
    }
}

void LayoutQuickWindow::paintCell(QPainter& p, const LayoutItem& item, const QRectF& r, bool fluent)
{
    const LayoutItemStyle st = resolvedItemStyle(item);
    const QString role = item.role.toLower();
    const double radius = st.radius.value_or(fluent ? 14.0 : 10.0);
    if (role == QLatin1String("slider")) {
        const bool hovered = item.interactive && (item.id == m_hoverId);
        paintSliderTrack(p, r, item, hovered, hovered ? m_hoverProgress : 0.0);
        return;
    }
    if (role == QLatin1String("preview")) {
        paintPreviewSwatch(p, r, radius);
        if (!item.label.isEmpty()) {
            p.setPen(ThemeColors::contrastOn(m_previewColor));
            p.setFont(QFont(QStringLiteral("Segoe UI"), 11, QFont::DemiBold));
            p.drawText(r.adjusted(8, 6, -8, -6), Qt::AlignLeft | Qt::AlignTop, item.label);
            if (!item.caption.isEmpty()) {
                p.setFont(QFont(QStringLiteral("Segoe UI"), 9));
                p.drawText(r.adjusted(8, 24, -8, -6), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                           item.caption);
            }
        }
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
        bg = m_theme.cellActive;
        fg = m_theme.accent;
    }
    if (hovered && bg.alpha() > 0) {
        bg = bg.lighter(118);
    }

    fillChrome(p, r, radius, bg, st);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(border, borderW));
    p.drawRoundedRect(r, radius, radius);

    paintProgressChrome(p, r, hovered, m_hoverProgress, visualsForItem(&item), radius);

    p.setPen(fg);
    if (!item.icon.isEmpty()) {
        const QRectF iconR(r.left() + 6, r.top() + 6, r.width() - 12, r.height() * 0.52);
        MouseIcons::paint(p, item.icon, iconR, fg);
        p.setFont(QFont(QStringLiteral("Segoe UI"), 11, QFont::DemiBold));
        p.drawText(r.adjusted(8, r.height() * 0.52, -8, -6),
                   Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, item.label);
    } else {
        p.setFont(QFont(QStringLiteral("Segoe UI"), fluent ? 13 : 14, QFont::DemiBold));
        const QString text =
            item.caption.isEmpty() ? item.label
                                   : QStringLiteral("%1\n%2").arg(item.label, item.caption);
        p.drawText(r.adjusted(8, 8, -8, -8), Qt::AlignCenter | Qt::TextWordWrap, text);
    }

    if (hasSwitch) {
        paintRadioButton(p, r, active);
    }
}

void LayoutQuickWindow::paintDefault(QPainter& p)
{
    const auto& ws = m_layout.placement.style;
    const double winR = ws.radius.value_or(12.0);
    const double winBw = ws.borderWidth.value_or(0.0);
    QColor bg = ws.background.value_or(m_theme.bgMain);
    QColor border = ws.borderColor.value_or(m_theme.accent);

    fillChrome(p, QRectF(0, 0, width(), height()), winR, bg, ws);
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

    fillChrome(p, QRectF(0, 0, width(), height()), winR, bgTop, ws, bgBot);
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
            if (fillChrome(p, r, radius, st.background.value_or(QColor()), st)) {
                p.setBrush(Qt::NoBrush);
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
