#include "ui/LayoutQuickWindow.h"

#include "layout/LayoutGeometry.h"
#include "layout/LayoutVisibility.h"
#include "ui/LayoutBoardPainter.h"
#include "utils/WinOverlay.h"

#include <QCloseEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QSet>
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
        LayoutBoardPainter(*m_host).paint(*p);
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
    if (item) {
        const QColor fg = resolvedItemStyle(*item).foreground.value_or(m_theme.text);
        return v.withItemFlash(fg);
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

void LayoutQuickWindow::applyInputFocusChrome()
{
#ifdef Q_OS_WIN
    const HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd) {
        return;
    }
    LONG_PTR ex = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    if (m_inputFocus) {
        ex &= ~WS_EX_NOACTIVATE;
    } else {
        ex |= WS_EX_NOACTIVATE;
    }
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, ex);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
#endif
}

void LayoutQuickWindow::setInputFocusEnabled(bool on)
{
    if (m_inputFocus == on) {
        if (on) {
            requestActivate();
        }
        return;
    }
    m_inputFocus = on;
    Qt::WindowFlags f = flags();
    if (on) {
        f &= ~Qt::WindowDoesNotAcceptFocus;
    } else {
        f |= Qt::WindowDoesNotAcceptFocus;
    }
    setFlags(f);
    applyInputFocusChrome();
    if (on) {
        requestActivate();
    }
}

void LayoutQuickWindow::keyPressEvent(QKeyEvent* event)
{
    if (m_inputFocus && event) {
        emit keyPressed(event->key(), event->text());
        event->accept();
        return;
    }
    QQuickWindow::keyPressEvent(event);
}

void LayoutQuickWindow::applyTopmost()
{
    applyOverlayWindowChrome(this, /*excludeFromCapture=*/false);
    applyInputFocusChrome();
    if (m_layout.placement.aboveTaskbar) {
        raiseAboveTaskbar(this);
        applyInputFocusChrome();
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
        ShowWindow(hwnd, m_inputFocus ? SW_SHOW : SW_SHOWNOACTIVATE);
    }
#endif
    keepAboveTaskbar();
    applyInputFocusChrome();
    if (m_inputFocus) {
        requestActivate();
    }
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
        if (item.kind == LayoutItemKind::Toggle) {
            if (LayoutBoardPainter::toggleHitRect(r).contains(local)) {
                hit = item.id;
            }
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

} // namespace gazer
