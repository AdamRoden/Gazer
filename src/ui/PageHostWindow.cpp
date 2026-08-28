#include "ui/PageHostWindow.h"

#include "input/KeyGlyphs.h"
#include "input/KeyNames.h"

#include "ui/BoardPaint.h"
#include "utils/WinOverlay.h"

#include <QCloseEvent>
#include <QHash>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QQuickPaintedItem>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QTransform>
#include <QtMath>
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
        p->setRenderHint(QPainter::Antialiasing, true);
        p->save();
        p->setCompositionMode(QPainter::CompositionMode_Source);
        p->fillRect(boundingRect(), Qt::transparent);
        p->restore();
        m_host->paintScene(*p, PageHostWindow::ChromePass::Live);
        if (!m_host->m_flashRect.isEmpty()) {
            const QRectF fr = m_host->m_flashRect.translated(-m_host->paintOrigin());
            const QColor fc = m_host->m_progress.resolvedFlashColor(m_host->m_theme.text);
            const PageBox radii =
                m_host->m_flashRadii.isSet() ? m_host->m_flashRadii
                                             : PageBox::all(PageChrome::kDefaultRadius);
            BoardPaint::fillRound(*p, fr, radii, fc);
            BoardPaint::strokeRound(*p, fr, radii, fc, PageBox::all(3.5));
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
    m_raiseTimer.setSingleShot(true);
    connect(&m_raiseTimer, &QTimer::timeout, this, [this]() {
        if (isVisible()) {
            raiseInTopmostBand(this);
        }
    });
    connect(&m_flashTimer, &QTimer::timeout, this, [this]() {
        m_flashId.clear();
        m_flashRect = {};
        m_flashRadii = {};
        if (m_board) {
            m_board->update();
        }
    });
}

void PageHostWindow::syncBoardSize()
{
    if (m_board) {
        m_board->setSize(QSizeF(width(), height()));
    }
}

void PageHostWindow::paintScene(QPainter& p, ChromePass pass)
{
    const bool live = pass == ChromePass::Live;
    GlassBackdrop* glass = live ? &m_glass : nullptr;
    const QPoint origin = paintOrigin();
    const QTransform& xf = m_drawerXf;
    auto mapRect = [&](const PageTarget& t, const QRectF& r) {
        return PageHit::mapDrawer(t, r, xf, m_drawerScale).translated(-origin);
    };
    auto paintGrid = [&](const PageGridPaint& g) {
        if (!live && g.chrome.hasBlur()) {
            return;
        }
        const QRectF r =
            PageHit::mapDrawer(g.drawerMotion, g.visual, xf, m_drawerScale).translated(-origin);
        BoardPaint::paintSurface(p, r, g.chrome, m_theme, glass, true, false, false, false, false);
    };
    auto paintClusters = [&](const QString& pageId, bool shell) {
        QHash<QString, QRectF> clusters;
        for (const PageTarget& t : m_targets) {
            if (t.shell != shell || t.pageId != pageId || t.cluster.isEmpty()) {
                continue;
            }
            if (!live && t.chrome.hasBlur()) {
                continue;
            }
            const QRectF c = mapRect(t, t.geom.contentOnScreen());
            if (c.isEmpty()) {
                continue;
            }
            clusters[t.cluster] =
                clusters.value(t.cluster).isEmpty() ? c : clusters[t.cluster].united(c);
        }
        for (auto it = clusters.cbegin(); it != clusters.cend(); ++it) {
            if (it.value().isEmpty()) {
                continue;
            }
            QColor fill = m_theme.cellBg;
            if (fill.isValid()) {
                fill.setAlpha(qBound(80, fill.alpha(), 180));
                p.setPen(Qt::NoPen);
                p.setBrush(fill);
                p.drawRoundedRect(it.value(), 6.0, 6.0);
            }
            QColor stroke = m_theme.border;
            stroke.setAlpha(qBound(40, stroke.alpha(), 90));
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(stroke, 1.0));
            p.drawRoundedRect(it.value(), 6.0, 6.0);
        }
    };
    auto paintTarget = [&](const PageTarget& t) {
        if (!live && t.chrome.hasBlur()) {
            return;
        }
        PageTarget vis = t;
        QString sendKey;
        for (const PageAction& a : t.actions) {
            if (a.type != PageActionType::Send || a.sendKey.isEmpty()
                || !a.sendEdge.trimmed().isEmpty() || KeyNames::isModifier(a.sendKey)) {
                continue;
            }
            sendKey = a.sendKey;
            break;
        }
        vis.label = KeyGlyphs::displayLabel(t.label, sendKey, m_shiftHeld);
        const bool hovered = live && sessionKey(t) == m_hoverId;
        const bool flashing = live && sessionKey(t) == m_flashId;
        const bool active = live && m_activeIds.contains(sessionKey(t));
        const bool locked = live && m_lockedIds.contains(sessionKey(t));
        const double progress = hovered ? m_hoverProgress : 0.0;
        const bool showProgress = flashing || (hovered && (progress > 0.0 || m_revealProgress));
        if (t.kind == PageTarget::Kind::Zone && t.geom.hidesUntilProgress() && !showProgress) {
            return;
        }
        const QRectF content = mapRect(t, t.kind == PageTarget::Kind::Zone ? t.geom.visual
                                                                          : t.geom.contentOnScreen());
        if (!content.isEmpty()) {
            BoardPaint::paintTarget(p, vis, content, m_theme, glass, hovered, progress, flashing,
                                    active, m_progress, m_previewColor,
                                    live ? m_sliderScrubId : QString(),
                                    live ? m_sliderScrubT : 0.0,
                                    live ? m_sliderScrubValue : QString(),
                                    live ? m_sliderScrubProgress : 0.0, locked);
        } else if (showProgress && !t.geom.progressZone.isEmpty()) {
            const QRectF strip = mapRect(t, t.geom.progressZone);
            BoardPaint::paintTarget(p, vis, strip, m_theme, glass, hovered, progress, flashing,
                                    active, m_progress, m_previewColor, m_sliderScrubId,
                                    m_sliderScrubT, m_sliderScrubValue, m_sliderScrubProgress,
                                    locked);
        }
    };
    auto paintPage = [&](const QString& pageId, bool shell) {
        for (const PageGridPaint& g : m_gridPaints) {
            if (g.shell == shell && g.pageId == pageId) {
                paintGrid(g);
            }
        }
        paintClusters(pageId, shell);
        for (const PageTarget& t : m_targets) {
            if (t.shell == shell && t.pageId == pageId) {
                paintTarget(t);
            }
        }
    };
    auto note = [](QStringList& order, const QString& id, bool wantShell, bool isShell) {
        if (wantShell != isShell || id.isEmpty() || order.contains(id)) {
            return;
        }
        order.push_back(id);
    };
    QStringList pageOrder;
    for (const PageGridPaint& g : m_gridPaints) {
        note(pageOrder, g.pageId, false, g.shell);
    }
    for (const PageTarget& t : m_targets) {
        note(pageOrder, t.pageId, false, t.shell);
    }
    for (const QString& pid : pageOrder) {
        paintPage(pid, false);
    }
    QStringList shellOrder;
    for (const PageGridPaint& g : m_gridPaints) {
        note(shellOrder, g.pageId, true, g.shell);
    }
    for (const PageTarget& t : m_targets) {
        note(shellOrder, t.pageId, true, t.shell);
    }
    for (const QString& pid : shellOrder) {
        paintPage(pid, true);
    }
}

void PageHostWindow::refreshUnderlay()
{
    const QSize sz = size();
    if (sz.width() < 1 || sz.height() < 1) {
        m_glass.setUnderlay({}, {});
        return;
    }
    QPixmap pm(sz);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    paintScene(p, ChromePass::Underlay);
    m_glass.setUnderlay(pm, paintOrigin());
}

void PageHostWindow::syncFrost(bool grabNow)
{
    const bool rest = m_drawerScale >= 0.999;
    if (grabNow) {
        refreshUnderlay();
        m_glass.captureNow();
        if (!rest) {
            m_glass.stopRefresh();
        }
        return;
    }
    if (rest) {
        refreshUnderlay();
        return;
    }
    m_glass.stopRefresh();
}

void PageHostWindow::syncGlass()
{
    double blur = 0.0;
    for (const PageGridPaint& g : m_gridPaints) {
        blur = qMax(blur, g.chrome.blur.value_or(0.0));
    }
    for (const PageTarget& t : m_targets) {
        blur = qMax(blur, t.chrome.blur.value_or(0.0));
    }
    const QRect cap = PageHit::frostedBounds(m_targets, m_gridPaints).toAlignedRect();
    m_glass.setCaptureRect(cap);
    if (!qFuzzyCompare(blur + 1.0, m_blurMax + 1.0)) {
        m_blurMax = blur;
        m_glass.setActive(blur);
    }
}

void PageHostWindow::cacheDrawerXf()
{
    m_drawerXf = PageHit::drawerTransform(m_targets, m_drawerScale, m_gridPaints);
}

QPoint PageHostWindow::paintOrigin() const
{
    return geometry().topLeft();
}

void PageHostWindow::fitToChrome()
{
    QRectF u = PageHit::hostBounds(m_targets, m_gridPaints);
    if (!m_reserved.isEmpty()) {
        u = u.isEmpty() ? m_reserved : u.united(m_reserved);
    }
    const int pad = qMax(8, qCeil(m_blurMax * 2.0));
    QRect geo(0, 0, 1, 1);
    if (!u.isEmpty()) {
        geo = u.toAlignedRect().adjusted(-pad, -pad, pad, pad);
        if (geo.width() < 1) {
            geo.setWidth(1);
        }
        if (geo.height() < 1) {
            geo.setHeight(1);
        }
    }
    if (geo.topLeft() == m_origin && geo.size() == size()) {
        return;
    }
    m_origin = geo.topLeft();
    setGeometry(geo);
    syncBoardSize();
}

void PageHostWindow::commit(QVector<PageTarget> targets, QVector<PageGridPaint> grids,
                            double drawerScale, QRectF reserved)
{
    m_targets = std::move(targets);
    m_gridPaints = std::move(grids);
    m_reserved = reserved;
    m_drawerScale = drawerScale;
    cacheDrawerXf();
    syncGlass();
    fitToChrome();
    syncFrost(true);
    if (m_board) {
        m_board->update();
    }
}

void PageHostWindow::setTheme(const ThemeColors& theme)
{
    m_theme = theme;
    refreshUnderlay();
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

void PageHostWindow::setLockedIds(QSet<QString> ids)
{
    if (ids == m_lockedIds) {
        return;
    }
    m_lockedIds = std::move(ids);
    if (m_board) {
        m_board->update();
    }
}

void PageHostWindow::setShiftHeld(bool on)
{
    if (m_shiftHeld == on) {
        return;
    }
    m_shiftHeld = on;
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
    cacheDrawerXf();
    syncFrost(false);
    if (m_board) {
        m_board->update();
    }
}

void PageHostWindow::setHover(const QString& id, double progress, bool revealProgress)
{
    if (m_hoverId == id && qFuzzyCompare(m_hoverProgress + 1.0, progress + 1.0)
        && m_revealProgress == revealProgress) {
        return;
    }
    m_hoverId = id;
    m_hoverProgress = progress;
    m_revealProgress = revealProgress && !id.isEmpty();
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
    m_flashRadii = {};
    const QTransform& xf = m_drawerXf;
    for (const PageTarget& t : m_targets) {
        if (sessionKey(t) != id) {
            continue;
        }
        QRectF r = t.geom.contentOnScreen();
        if (r.isEmpty()) {
            r = t.geom.progressZone;
        }
        m_flashRect = PageHit::mapDrawer(t, r, xf, m_drawerScale);
        const bool clustered = !t.cluster.isEmpty();
        m_flashRadii = t.chrome.resolvedRadius(clustered);
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

void PageHostWindow::applyChrome()
{
    applyOverlayWindowChrome(this, /*excludeFromCapture=*/false);
    applyInputFocusChrome();
    setOverlayStackHost(this);
    raiseInTopmostBand(this);
    // Explorer restacks Shell_TrayWnd after a show; HWND_TOP again if it landed on us.
    // Owned overlays stay above this window.
    m_raiseTimer.start(180);
}

void PageHostWindow::showHost()
{
    fitToChrome();
    show();
    applyChrome();
}

void PageHostWindow::raiseHost()
{
    if (!isVisible()) {
        showHost();
        return;
    }
    raiseInTopmostBand(this);
    m_raiseTimer.start(180);
}

QString PageHostWindow::mouseHit(const QPointF& global) const
{
    const QTransform& xf = m_drawerXf;
    const QString cover =
        PageHit::coveringPageId(m_gridPaints, global, m_drawerScale, m_targets, &xf);
    for (int i = m_targets.size() - 1; i >= 0; --i) {
        const PageTarget& t = m_targets.at(i);
        if (!t.interactive) {
            continue;
        }
        if (!cover.isEmpty() && !t.shell && t.pageId != cover) {
            continue;
        }
        const QString key = sessionKey(t);
        const bool showProgress = (key == m_flashId)
                                  || (key == m_hoverId && m_hoverProgress > 0.0);
        const bool clustered = !t.cluster.isEmpty();
        if (t.kind == PageTarget::Kind::Zone && !showProgress) {
            const QRectF dwell = PageHit::mapDrawer(t, t.geom.dwellZone, xf, m_drawerScale);
            if (dwell.contains(global)) {
                return key;
            }
            continue;
        }
        const QRectF content =
            PageHit::mapDrawer(t, t.geom.contentOnScreen(), xf, m_drawerScale);
        if (!content.isEmpty()
            && PageHit::shapeContains(content, t.chrome, clustered, global)) {
            return key;
        }
        if (showProgress) {
            const QRectF strip = PageHit::mapDrawer(t, t.geom.progressZone, xf, m_drawerScale);
            if (PageHit::shapeContains(strip, t.chrome, clustered, global)) {
                return key;
            }
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
            if (OverlayInputPassThrough::active()) {
                *result = HTTRANSPARENT;
                return true;
            }
            const QPoint native(GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam));
            const QPoint gp = logicalGlobalFromNative(this, native);
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
