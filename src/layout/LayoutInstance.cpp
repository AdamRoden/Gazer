#include "layout/LayoutInstance.h"

#include "ui/ProgressVisuals.h"

#include <QGuiApplication>
#include <QScreen>

namespace gazer {

LayoutInstance::LayoutInstance(QString instanceId, LayoutDocument document, QObject* parent)
    : QObject(parent)
    , m_instanceId(std::move(instanceId))
    , m_document(std::move(document))
{
    m_window = std::make_unique<LayoutWindow>();
    m_window->setLayout(m_document);
    m_window->setWindowTitle(
        QStringLiteral("Gazer — %1 [%2]").arg(m_document.name, m_instanceId));

    connect(m_window.get(), &LayoutWindow::closeRequested, this,
            [this]() { emit windowCloseRequested(m_instanceId); });
    connect(m_window.get(), &LayoutWindow::itemClicked, this,
            [this](const QString& itemId) {
                if (m_dwellSuspended) {
                    const LayoutItem* item = m_document.findItem(itemId);
                    if (!item || !item->isDwellExempt()) {
                        return;
                    }
                }
                emit itemActivated(m_instanceId, itemId);
            });

    m_dwell = std::make_unique<DwellStateMachine>();
    applyDwellConfig();

    connect(m_dwell.get(), &DwellStateMachine::dwellProgress, this,
            [this](const QString& itemId, double progress) {
                if (m_window) {
                    const LayoutItem* item =
                        itemId.isEmpty() ? nullptr : m_document.findItem(itemId);
                    const bool boardCell =
                        item && item->participatesInBoardGrid()
                        && !DwellRegionSpace::mostlyOffDisplay(
                               gridItemScreenRect(itemId), DwellRegionSpace::virtualDesktop());
                    if (boardCell) {
                        m_window->setHoverState(itemId, progress);
                    } else {
                        m_window->setHoverState(QString(), 0.0);
                    }
                }
                syncEdgeBubble(itemId, progress);
            });
    connect(m_dwell.get(), &DwellStateMachine::itemActivated, this,
            [this](const QString& itemId) {
                const LayoutItem* item = m_document.findItem(itemId);
                const bool unbounded =
                    item && (!item->participatesInBoardGrid() || item->hasDwellRegion);
                if (unbounded && m_edgeBubbles) {
                    ProgressVisuals v = m_progressVisuals;
                    if (m_document.dwell.sectionPresent) {
                        v = v.mergedWith(m_document.dwell);
                    }
                    if (item && item->dwell.sectionPresent) {
                        v = v.mergedWith(item->dwell);
                    }
                    m_edgeBubbles->flashThenClear(m_instanceId, v, v.flashMs);
                } else {
                    if (m_window) {
                        m_window->flashItem(itemId);
                    }
                    clearEdgeBubble();
                }
                emit itemActivated(m_instanceId, itemId);
            });

    applyPlacement(0);
}

void LayoutInstance::applyDwellConfig()
{
    m_dwell->reset();
    m_dwell->setEnabled(m_document.dwell.enabled);
    m_activeDwellItemId.clear();
    applyDwellForItem(QString());
}

void LayoutInstance::applyDwellForItem(const QString& itemId)
{
    QVector<int> seq;
    int grace = m_globalGraceMs > 0 ? m_globalGraceMs : 180;

    const LayoutItem* item = itemId.isEmpty() ? nullptr : m_document.findItem(itemId);

    if (item && item->dwell.hasTiming) {
        seq = item->dwell.effectiveSequence();
    } else if (m_document.dwell.hasTiming) {
        seq = m_document.dwell.effectiveSequence();
    } else if (!m_globalDwellSequence.isEmpty()) {
        seq = m_globalDwellSequence;
    } else {
        seq = m_document.dwell.effectiveSequence();
    }

    if (item && item->dwell.hasGrace && item->dwell.graceMs >= 0) {
        grace = item->dwell.graceMs;
    } else if (m_document.dwell.hasGrace && m_document.dwell.graceMs >= 0) {
        grace = m_document.dwell.graceMs;
    }

    m_dwell->setDwellSequence(seq);
    m_dwell->setInvalidGraceMs(qMax(0, grace));
}

void LayoutInstance::setGlobalDwellOverride(const QVector<int>& dwellSequence, int graceMs)
{
    m_globalDwellSequence = dwellSequence;
    m_globalGraceMs = graceMs;
    applyDwellConfig();
}

void LayoutInstance::setProgressVisuals(const ProgressVisuals& visuals)
{
    m_progressVisuals = visuals;
    if (m_window) {
        m_window->setProgressVisuals(visuals);
    }
}

void LayoutInstance::setTheme(const ThemeColors& theme)
{
    if (m_window) {
        m_window->setTheme(theme);
    }
}

void LayoutInstance::setActiveItemIds(const QSet<QString>& activeIds)
{
    if (m_window) {
        m_window->setActiveItemIds(activeIds);
    }
}

void LayoutInstance::setItemText(const QString& itemId, const QString& label,
                                 const QString& caption)
{
    for (LayoutItem& it : m_document.items) {
        if (it.id == itemId) {
            it.label = label;
            if (!caption.isNull()) {
                it.caption = caption;
            }
            if (m_window) {
                m_window->setLayout(m_document);
            }
            return;
        }
    }
}

void LayoutInstance::setDocument(LayoutDocument document)
{
    m_document = std::move(document);
    m_window->setLayout(m_document);
    m_window->setWindowTitle(
        QStringLiteral("Gazer — %1 [%2]").arg(m_document.name, m_instanceId));
    applyDwellConfig();
    applyPlacement(0);
    if (m_window) {
        m_window->update();
    }
}

void LayoutInstance::raise()
{
    if (!m_window) {
        return;
    }
    if (!m_document.showsBoardWindow()) {
        m_window->hide();
        return;
    }
    m_window->showAndRaise();
}

void LayoutInstance::hide()
{
    if (m_window) {
        m_window->hide();
    }
    leaveGaze();
}

void LayoutInstance::applyPlacement(int cascadeOffset)
{
    if (!m_window) {
        return;
    }

    const LayoutWindowPlacement& p = m_document.placement;

    // Headless / items-only: no board chrome. Keep a 1×1 HWND for mapToGlobal
    // when board-local dwell regions are used; screenAnchor does not need it.
    if (!m_document.showsBoardWindow()) {
        m_window->setMinimumSize(1, 1);
        m_window->setMaximumSize(1, 1);
        m_window->resize(1, 1);
        if (QScreen* screen = QGuiApplication::primaryScreen()) {
            m_window->move(screen->geometry().topLeft());
        }
        m_window->hide();
        return;
    }

    m_window->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

    QScreen* screen = QGuiApplication::primaryScreen();
    const QRect avail = screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);

    int winW = 1000;
    int winH = 560;
    if (p.width.isSet()) {
        winW = qMax(40, p.width.resolveInt(avail.width(), p.widthPx > 0 ? p.widthPx : 1000));
    } else if (p.widthPx > 0) {
        winW = p.widthPx;
    }
    if (p.height.isSet()) {
        winH = qMax(40, p.height.resolveInt(avail.height(), p.heightPx > 0 ? p.heightPx : 560));
    } else if (p.heightPx > 0) {
        winH = p.heightPx;
    }
    m_window->setMinimumSize(qMax(40, qMin(winW, 80)), qMax(40, qMin(winH, 80)));
    m_window->resize(winW, winH);

    if (p.anchor == LayoutWindowPlacement::Anchor::Default && !p.x.isSet() && !p.y.isSet()) {
        placeRelative(cascadeOffset, cascadeOffset);
        return;
    }

    if (!screen) {
        placeRelative(cascadeOffset, cascadeOffset);
        return;
    }

    const int margin = qMax(0, p.marginPx);
    const int w = m_window->width();
    const int h = m_window->height();

    int x = avail.left() + margin;
    int y = avail.top() + margin;

    switch (p.anchor) {
    case LayoutWindowPlacement::Anchor::TopLeft:
        break;
    case LayoutWindowPlacement::Anchor::TopCenter:
        x = avail.left() + (avail.width() - w) / 2;
        break;
    case LayoutWindowPlacement::Anchor::TopRight:
        x = avail.right() - w - margin + 1;
        break;
    case LayoutWindowPlacement::Anchor::Center:
        x = avail.left() + (avail.width() - w) / 2;
        y = avail.top() + (avail.height() - h) / 2;
        break;
    case LayoutWindowPlacement::Anchor::LeftCenter:
        x = avail.left() + margin;
        y = avail.top() + (avail.height() - h) / 2;
        break;
    case LayoutWindowPlacement::Anchor::RightCenter:
        x = avail.right() - w - margin + 1;
        y = avail.top() + (avail.height() - h) / 2;
        break;
    case LayoutWindowPlacement::Anchor::BottomLeft:
        y = avail.bottom() - h - margin + 1;
        break;
    case LayoutWindowPlacement::Anchor::BottomCenter:
        x = avail.left() + (avail.width() - w) / 2;
        y = avail.bottom() - h - margin + 1;
        break;
    case LayoutWindowPlacement::Anchor::BottomRight:
        x = avail.right() - w - margin + 1;
        y = avail.bottom() - h - margin + 1;
        break;
    case LayoutWindowPlacement::Anchor::Default:
        break;
    }

    // Explicit x/y replace or offset from anchor-computed position.
    if (p.x.isSet()) {
        x = avail.left() + p.x.resolveInt(avail.width(), 0);
    }
    if (p.y.isSet()) {
        y = avail.top() + p.y.resolveInt(avail.height(), 0);
    }

    m_window->move(x + cascadeOffset, y + cascadeOffset);
}

void LayoutInstance::placeRelative(int offsetX, int offsetY)
{
    QPoint origin(80, 80);
    if (QScreen* screen = QGuiApplication::primaryScreen()) {
        origin = screen->availableGeometry().topLeft() + QPoint(80, 80);
    }
    m_window->move(origin + QPoint(offsetX, offsetY));
}

QPoint LayoutInstance::boardOrigin() const
{
    return m_window ? m_window->mapToGlobal(QPoint(0, 0)) : QPoint();
}

QScreen* LayoutInstance::boardScreen() const
{
    if (m_window) {
        if (QScreen* s = m_window->screen()) {
            return s;
        }
        if (QScreen* s = QGuiApplication::screenAt(screenRect().center())) {
            return s;
        }
    }
    return QGuiApplication::primaryScreen();
}

QRect LayoutInstance::gridItemScreenRect(const QString& itemId) const
{
    if (!m_window) {
        return {};
    }
    const QRectF local = m_window->itemLocalRects().value(itemId);
    if (local.isEmpty()) {
        return {};
    }
    return QRect(boardOrigin() + local.topLeft().toPoint(), local.size().toSize());
}

DwellRegionSpace::Resolved LayoutInstance::resolveItem(const LayoutItem& item) const
{
    return DwellRegionSpace::resolveItem(item, boardOrigin(), screenRect(), boardScreen());
}

QRect LayoutInstance::itemHitRect(const LayoutItem& item) const
{
    const auto r = resolveItem(item);
    const bool engaged = !m_dwellLipItemId.isEmpty() && m_dwellLipItemId == item.id;
    return r.hitWithDriftLip(engaged);
}

QRect LayoutInstance::unpauseGapScreenRect(const LayoutItem& item) const
{
    if (item.participatesInBoardGrid()) {
        return gridItemScreenRect(item.id);
    }
    const auto r = resolveItem(item);
    // Prefer visible edge band (coerced progress position); else on-screen part of hit.
    if (!r.band.onScreen.isEmpty()) {
        return r.band.onScreen.toRect();
    }
    const QRect desktop = DwellRegionSpace::virtualDesktop();
    const QRect hit = r.hit;
    if (!hit.isEmpty() && desktop.intersects(hit)) {
        return desktop.intersected(hit);
    }
    // Fallback: small chip at coerced center.
    if (!hit.isEmpty() && desktop.isValid()) {
        const QPoint c = DwellRegionSpace::coercePointOntoScreen(hit.center(), desktop);
        return QRect(c.x() - 40, c.y() - 40, 80, 80).intersected(desktop);
    }
    return {};
}

QRect LayoutInstance::screenRect() const
{
    if (!m_window) {
        return {};
    }
    return QRect(boardOrigin(), m_window->size());
}

QPointF LayoutInstance::centerScreen() const
{
    return screenRect().center();
}

QString LayoutInstance::hitTest(const QPointF& screenPoint) const
{
    const QPoint pt = screenPoint.toPoint();
    auto allow = [this](const LayoutItem& item) {
        if (!item.interactive) {
            return false;
        }
        if (m_dwellSuspended && !item.isDwellExempt()) {
            return false;
        }
        return true;
    };

    QString hit;
    for (const LayoutItem& item : m_document.items) {
        if (!allow(item) || item.participatesInBoardGrid()) {
            continue;
        }
        const QRect rect = itemHitRect(item);
        if (!rect.isEmpty() && rect.contains(pt)) {
            hit = item.id; // last match wins
        }
    }
    if (!hit.isEmpty()) {
        return hit;
    }
    if (!m_window) {
        return {};
    }
    const QString boardHit = m_window->hitTestGlobal(screenPoint);
    if (boardHit.isEmpty()) {
        return {};
    }
    if (m_dwellSuspended) {
        const LayoutItem* item = m_document.findItem(boardHit);
        if (!item || !item->isDwellExempt()) {
            return {};
        }
    }
    return boardHit;
}

bool LayoutInstance::containsScreenPoint(const QPointF& screenPoint) const
{
    if (!m_window) {
        return false;
    }
    const QPoint pt = screenPoint.toPoint();
    // Board chrome: still "over" the window for routing, but hitTest filters cells.
    if (m_window->isVisible() && screenRect().contains(pt)) {
        return true;
    }
    for (const LayoutItem& item : m_document.items) {
        if (!item.interactive || item.participatesInBoardGrid()) {
            continue;
        }
        if (m_dwellSuspended && !item.isDwellExempt()) {
            continue;
        }
        const QRect rect = itemHitRect(item);
        if (!rect.isEmpty() && rect.contains(pt)) {
            return true;
        }
    }
    return false;
}

void LayoutInstance::updateDriftLip(const QString& itemId, double progress)
{
    // Off-screen drift lip: only after continuous dwell on the logical rect for
    // kOffscreenLipEngageMs (then gaze may drift onto the edge band / hit expands).
    if (itemId.isEmpty()) {
        m_dwellLipItemId.clear();
        m_dwellLipTrackItemId.clear();
        return;
    }
    if (progress <= 0.0) {
        // progress == 0 on same item: keep track/lip (sequence step boundaries).
        return;
    }
    if (m_dwellLipTrackItemId != itemId) {
        m_dwellLipTrackItemId = itemId;
        m_dwellLipItemId.clear();
        m_dwellLipClock.restart();
    }
    if (m_dwellLipClock.isValid() && m_dwellLipClock.elapsed() >= kOffscreenLipEngageMs) {
        m_dwellLipItemId = itemId;
    }
}

void LayoutInstance::syncEdgeBubble(const QString& itemId, double progress)
{
    updateDriftLip(itemId, progress);

    if (!m_edgeBubbles) {
        return;
    }
    if (itemId.isEmpty() || progress <= 0.0) {
        clearEdgeBubble();
        return;
    }
    const LayoutItem* item = m_document.findItem(itemId);
    if (!item || item->participatesInBoardGrid()) {
        clearEdgeBubble();
        return;
    }

    const auto resolved = resolveItem(*item);
    if (!resolved.showBubble || resolved.band.onScreen.isEmpty()) {
        clearEdgeBubble();
        return;
    }

    ProgressVisuals v = m_progressVisuals;
    if (m_document.dwell.sectionPresent) {
        v = v.mergedWith(m_document.dwell);
    }
    if (item->dwell.sectionPresent) {
        v = v.mergedWith(item->dwell);
    }

    EdgeBubbleOverlay::Bubble b;
    b.key = m_instanceId; // one slot per board instance
    b.label = item->label;
    b.band = resolved.band;
    b.progress = progress;
    b.visuals = v;
    b.flashing = false;
    m_edgeBubbles->setBubble(b);
}

void LayoutInstance::clearEdgeBubble()
{
    if (m_edgeBubbles) {
        m_edgeBubbles->clearBubble(m_instanceId);
    }
}

void LayoutInstance::feedGaze(const GazePoint& point, const QString& itemIdUnderGaze)
{
    if (itemIdUnderGaze != m_activeDwellItemId) {
        if (!m_activeDwellItemId.isEmpty()) {
            clearEdgeBubble();
            emit dwellEngagementEnded(m_instanceId, m_activeDwellItemId);
        }
        // Target change: drop drift lip until new item dwells long enough.
        m_dwellLipItemId.clear();
        m_dwellLipTrackItemId.clear();
        m_activeDwellItemId = itemIdUnderGaze;
        applyDwellForItem(itemIdUnderGaze);
    }
    m_dwell->onGazeSample(point, itemIdUnderGaze);
}

void LayoutInstance::leaveGaze()
{
    clearEdgeBubble();
    if (!m_activeDwellItemId.isEmpty()) {
        emit dwellEngagementEnded(m_instanceId, m_activeDwellItemId);
    }
    m_dwellLipItemId.clear();
    m_dwellLipTrackItemId.clear();
    m_activeDwellItemId.clear();
    m_dwell->leave();
    if (m_window) {
        m_window->setHoverState(QString(), 0.0);
    }
}

} // namespace gazer
