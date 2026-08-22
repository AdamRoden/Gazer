#include "ui/PageHostWindow.h"

#include "layout/DwellRegionSpace.h"
#include "ui/BoardPaint.h"
#include "utils/WinOverlay.h"

#include <QCloseEvent>
#include <QHash>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QQuickPaintedItem>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QTransform>
#include <utility>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <windowsx.h>
#endif

namespace gazer {


class PageHostItem final : public QQuickPaintedItem {
public:
    explicit PageHostItem(PageHostWindow* host, QQuickItem* parent)
        : QQuickPaintedItem(parent)
        , m_host(host)
    {
        setAntialiasing(true);
        setOpaquePainting(false);
        setFillColor(Qt::transparent);
        setAcceptedMouseButtons(Qt::LeftButton);
    }

    void paint(QPainter* p) override
    {
        if (!p || !m_host) {
            return;
        }
        p->save();
        p->setCompositionMode(QPainter::CompositionMode_Source);
        p->fillRect(boundingRect(), Qt::transparent);
        p->restore();
        p->setRenderHint(QPainter::Antialiasing, true);
        const QPoint origin = m_host->m_origin;
        const QTransform xf = PageHit::drawerTransform(m_host->m_targets, m_host->m_drawerScale,
                                                       m_host->m_gridPaints);
        auto mapRect = [&](const PageTarget& t, const QRectF& r) {
            return PageHit::mapDrawer(t, r, xf, m_host->m_drawerScale).translated(-origin);
        };
        auto paintGrid = [&](const PageGridPaint& g) {
            const QRectF r =
                PageHit::mapDrawer(g.drawerMotion, g.visual, xf, m_host->m_drawerScale)
                    .translated(-origin);
            BoardPaint::paintSurface(*p, r, g.chrome, m_host->m_theme, &m_host->m_glass, true,
                                     false, false, false, false);
        };
        auto paintClusters = [&](const QString& pageId, bool shell) {
            QHash<QString, QRectF> clusters;
            for (const PageTarget& t : m_host->m_targets) {
                if (t.shell != shell || t.pageId != pageId || t.cluster.isEmpty()) {
                    continue;
                }
                const QRectF c = mapRect(t, t.geom.contentOnScreen());
                if (c.isEmpty()) {
                    continue;
                }
                clusters[t.cluster] = clusters.value(t.cluster).isEmpty()
                                          ? c
                                          : clusters[t.cluster].united(c);
            }
            for (auto it = clusters.cbegin(); it != clusters.cend(); ++it) {
                if (it.value().isEmpty()) {
                    continue;
                }
                QColor fill = m_host->m_theme.cellBg;
                if (fill.isValid()) {
                    fill.setAlpha(qBound(80, fill.alpha(), 180));
                    p->setPen(Qt::NoPen);
                    p->setBrush(fill);
                    p->drawRoundedRect(it.value(), 6.0, 6.0);
                }
                QColor stroke = m_host->m_theme.border;
                stroke.setAlpha(qBound(40, stroke.alpha(), 90));
                p->setBrush(Qt::NoBrush);
                p->setPen(QPen(stroke, 1.0));
                p->drawRoundedRect(it.value(), 6.0, 6.0);
            }
        };
        auto paintTarget = [&](const PageTarget& t) {
            const bool hovered = t.id == m_host->m_hoverId;
            const bool flashing = t.id == m_host->m_flashId;
            const bool active = m_host->m_activeIds.contains(t.id);
            const double progress = hovered ? m_host->m_hoverProgress : 0.0;
            const bool showProgress = flashing || (hovered && progress > 0.0);
            if (t.kind == PageTarget::Kind::Zone && t.geom.hidesUntilProgress() && !showProgress) {
                return;
            }
            const QRectF content = mapRect(t, t.geom.contentOnScreen());
            if (!content.isEmpty()) {
                BoardPaint::paintTarget(*p, t, content, m_host->m_theme, &m_host->m_glass, hovered,
                                        progress, flashing, active, m_host->m_progress,
                                        m_host->m_previewColor, m_host->m_sliderScrubId,
                                        m_host->m_sliderScrubT, m_host->m_sliderScrubValue,
                                        m_host->m_sliderScrubProgress);
            } else if (showProgress && !t.geom.progressZone.isEmpty()) {
                const QRectF strip = mapRect(t, t.geom.progressZone);
                BoardPaint::paintTarget(*p, t, strip, m_host->m_theme, &m_host->m_glass, hovered,
                                        progress, flashing, active, m_host->m_progress,
                                        m_host->m_previewColor, m_host->m_sliderScrubId,
                                        m_host->m_sliderScrubT, m_host->m_sliderScrubValue,
                                        m_host->m_sliderScrubProgress);
            }
        };
        auto paintPage = [&](const QString& pageId, bool shell) {
            for (const PageGridPaint& g : m_host->m_gridPaints) {
                if (g.shell == shell && g.pageId == pageId) {
                    paintGrid(g);
                }
            }
            paintClusters(pageId, shell);
            for (const PageTarget& t : m_host->m_targets) {
                if (t.shell == shell && t.pageId == pageId) {
                    paintTarget(t);
                }
            }
        };
        QStringList pageOrder;
        auto notePage = [&](const QString& id, bool shell) {
            if (shell) {
                return;
            }
            if (!pageOrder.contains(id)) {
                pageOrder.push_back(id);
            }
        };
        for (const PageGridPaint& g : m_host->m_gridPaints) {
            notePage(g.pageId, g.shell);
        }
        for (const PageTarget& t : m_host->m_targets) {
            notePage(t.pageId, t.shell);
        }
        for (const QString& pid : pageOrder) {
            paintPage(pid, false);
        }
        QStringList shellOrder;
        auto noteShell = [&](const QString& id) {
            if (!shellOrder.contains(id)) {
                shellOrder.push_back(id);
            }
        };
        for (const PageGridPaint& g : m_host->m_gridPaints) {
            if (g.shell) {
                noteShell(g.pageId);
            }
        }
        for (const PageTarget& t : m_host->m_targets) {
            if (t.shell) {
                noteShell(t.pageId);
            }
        }
        for (const QString& pid : shellOrder) {
            paintPage(pid, true);
        }
        if (!m_host->m_flashRect.isEmpty()) {
            const QRectF fr = m_host->m_flashRect.translated(-origin);
            const QColor fc = m_host->m_progress.resolvedFlashColor(m_host->m_theme.text);
            BoardPaint::fillRound(*p, fr, 8.0, fc);
            BoardPaint::strokeRound(*p, fr, 8.0, fc, 3.5);
        }
    }

protected:
    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (!event || !m_host || event->button() != Qt::LeftButton) {
            return;
        }
        const QString id = m_host->mouseHit(event->globalPosition());
        if (!id.isEmpty()) {
            emit m_host->targetClicked(id);
        }
    }

private:
    PageHostWindow* m_host = nullptr;
};

PageHostWindow::PageHostWindow(QWindow* parent)
    : QQuickWindow(parent)
    , m_glass(this)
{
    setColor(Qt::transparent);
    setFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool
             | Qt::WindowDoesNotAcceptFocus);
    setTitle(QStringLiteral("Gazer"));
    QQuickWindow::setMinimumSize(QSize(1, 1));
    m_board = new PageHostItem(this, contentItem());
    connect(this, &QQuickWindow::widthChanged, this, &PageHostWindow::syncBoardSize);
    connect(this, &QQuickWindow::heightChanged, this, &PageHostWindow::syncBoardSize);
    connect(&m_glass, &GlassBackdrop::updated, this, [this]() {
        if (m_board) {
            m_board->update();
        }
    });
    m_theme = ThemeColors::darkPreset();
    m_flashTimer.setSingleShot(true);
    connect(&m_flashTimer, &QTimer::timeout, this, [this]() {
        m_flashId.clear();
        m_flashRect = {};
        if (m_board) {
            m_board->update();
        }
    });
}

void PageHostWindow::syncBoardSize()
{
    if (m_board) {
        m_board->setSize(QSizeF(width(), height()));
        m_board->update();
    }
}

void PageHostWindow::setTheme(const ThemeColors& theme)
{
    m_theme = theme;
    if (m_board) {
        m_board->update();
    }
}

void PageHostWindow::setProgressVisuals(const ProgressVisuals& visuals)
{
    m_progress = visuals;
    if (m_board) {
        m_board->update();
    }
}

void PageHostWindow::setActiveIds(QSet<QString> ids)
{
    if (ids == m_activeIds) {
        return;
    }
    m_activeIds = std::move(ids);
    if (m_board) {
        m_board->update();
    }
}

void PageHostWindow::setGridPaints(QVector<PageGridPaint> grids)
{
    m_gridPaints = std::move(grids);
    if (m_board) {
        m_board->update();
    }
}

void PageHostWindow::setTargets(QVector<PageTarget> targets)
{
    m_targets = std::move(targets);
    if (m_board) {
        m_board->update();
    }
}

void PageHostWindow::setDrawerScale(double scale)
{
    if (qFuzzyCompare(m_drawerScale + 1.0, scale + 1.0)) {
        return;
    }
    m_drawerScale = scale;
    if (m_board) {
        m_board->update();
    }
}

void PageHostWindow::setHover(const QString& id, double progress)
{
    if (m_hoverId == id && qFuzzyCompare(m_hoverProgress + 1.0, progress + 1.0)) {
        return;
    }
    m_hoverId = id;
    m_hoverProgress = progress;
    if (m_board) {
        m_board->update();
    }
}

void PageHostWindow::flash(const QString& id)
{
    if (id.isEmpty()) {
        return;
    }
    m_flashId = id;
    m_flashRect = {};
    const QTransform xf = PageHit::drawerTransform(m_targets, m_drawerScale, m_gridPaints);
    for (const PageTarget& t : m_targets) {
        if (t.id != id) {
            continue;
        }
        QRectF r = t.geom.contentOnScreen();
        if (r.isEmpty()) {
            r = t.geom.progressZone;
        }
        m_flashRect = PageHit::mapDrawer(t, r, xf, m_drawerScale);
        break;
    }
    m_flashTimer.start(qMax(40, m_progress.flashMs));
    if (m_board) {
        m_board->update();
    }
}

void PageHostWindow::setPreviewColor(const QColor& color)
{
    const QColor next = color.isValid() ? color : ThemeColors::defaultProgressColor();
    if (m_previewColor == next) {
        return;
    }
    m_previewColor = next;
    if (m_board) {
        m_board->update();
    }
}

void PageHostWindow::setSliderScrub(const QString& itemId, double t, const QString& valueText,
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

void PageHostWindow::clearSliderScrub()
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

void PageHostWindow::applyInputFocusChrome()
{
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId());
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

void PageHostWindow::setInputFocusEnabled(bool on)
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
    applyChrome();
    if (on) {
        requestActivate();
    }
}

void PageHostWindow::keyPressEvent(QKeyEvent* event)
{
    if (m_inputFocus && event) {
        emit keyPressed(event->key(), event->text());
        event->accept();
        return;
    }
    QQuickWindow::keyPressEvent(event);
}

void PageHostWindow::coverVirtualDesktop()
{
    const QRect desk = DwellRegionSpace::virtualDesktop();
    m_origin = desk.topLeft();
    setGeometry(desk);
    syncBoardSize();
}

void PageHostWindow::applyChrome()
{
    applyOverlayWindowChrome(this, /*excludeFromCapture=*/false);
    applyInputFocusChrome();
    raiseAboveTaskbar(this);
}

void PageHostWindow::showHost()
{
    coverVirtualDesktop();
    show();
    applyChrome();
}

void PageHostWindow::raiseHost()
{
    if (!isVisible()) {
        showHost();
        return;
    }
    applyChrome();
}

QString PageHostWindow::mouseHit(const QPointF& global) const
{
    const QTransform xf = PageHit::drawerTransform(m_targets, m_drawerScale, m_gridPaints);
    const QString cover =
        PageHit::coveringPageId(m_gridPaints, global, m_drawerScale, m_targets);
    for (int i = m_targets.size() - 1; i >= 0; --i) {
        const PageTarget& t = m_targets.at(i);
        if (!t.interactive) {
            continue;
        }
        if (!cover.isEmpty() && !t.shell && t.pageId != cover) {
            continue;
        }
        const bool showProgress = (t.id == m_flashId)
                                  || (t.id == m_hoverId && m_hoverProgress > 0.0);
        if (t.kind == PageTarget::Kind::Zone && !showProgress) {
            QRectF dwell = PageHit::mapDrawer(t, t.geom.dwellZone, xf, m_drawerScale);
            if (dwell.contains(global)) {
                return t.id;
            }
            continue;
        }
        const QRectF content =
            PageHit::mapDrawer(t, t.geom.contentOnScreen(), xf, m_drawerScale);
        if (!content.isEmpty() && content.contains(global)) {
            return t.id;
        }
        if (showProgress
            && PageHit::mapDrawer(t, t.geom.progressZone, xf, m_drawerScale).contains(global)) {
            return t.id;
        }
    }
    return {};
}

void PageHostWindow::closeEvent(QCloseEvent* event)
{
    if (event) {
        event->ignore();
    }
}

bool PageHostWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef Q_OS_WIN
    if (eventType == QByteArrayLiteral("windows_generic_MSG") && message && result) {
        MSG* msg = static_cast<MSG*>(message);
        if (msg->message == WM_NCHITTEST) {
            const QPoint gp(GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam));
            if (mouseHit(gp).isEmpty()) {
                *result = HTTRANSPARENT;
                return true;
            }
            *result = HTCLIENT;
            return true;
        }
        if (msg->message == WM_CLOSE) {
            *result = 0;
            return true;
        }
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
#endif
    return QQuickWindow::nativeEvent(eventType, message, result);
}

} // namespace gazer
