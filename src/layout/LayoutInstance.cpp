#include "layout/LayoutInstance.h"

#include "layout/LayoutVisibility.h"
#include "ui/ProgressVisuals.h"

#include <QGuiApplication>
#include <QScreen>

#include <cmath>

namespace gazer {

LayoutInstance::LayoutInstance(QString instanceId, LayoutDocument document, QObject* parent)
    : QObject(parent)
    , m_instanceId(std::move(instanceId))
    , m_document(std::move(document))
{
    m_window = std::make_unique<LayoutQuickWindow>();
    m_window->setLayout(m_document);
    m_window->setWindowTitle(
        QStringLiteral("Gazer — %1 [%2]").arg(m_document.name, m_instanceId));

    connect(m_window.get(), &LayoutQuickWindow::closeRequested, this,
            [this]() { emit windowCloseRequested(m_instanceId); });
    connect(m_window.get(), &LayoutQuickWindow::itemClicked, this,
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
    m_scaleTimer.setInterval(16);
    connect(&m_scaleTimer, &QTimer::timeout, this, &LayoutInstance::tickScaleAnim);
    applyDwellConfig();

    connect(m_dwell.get(), &DwellStateMachine::dwellProgress, this,
            [this](const QString& itemId, double progress) {
                m_lastProgress = progress;
                if (progress > 0.0) {
                    emit dwellActivity(m_instanceId);
                }
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
                    const ProgressVisuals v = resolvedProgressVisuals(item);
                    m_edgeBubbles->flashThenClear(m_instanceId, v, v.flashMs);
                } else {
                    if (m_window) {
                        m_window->flashItem(itemId);
                    }
                    clearEdgeBubble();
                }
                emit itemActivated(m_instanceId, itemId);
            });

    applyPlacement();
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
    const LayoutItem* item = itemId.isEmpty() ? nullptr : m_document.findItem(itemId);
    const LayoutDwellConfig* itemDwell = item ? &item->dwell : nullptr;

    const auto pickMs = [](bool itemHas, int itemVal, bool useGlobal, int globalVal, bool docHas,
                           int docVal, int fallback) {
        if (itemHas && itemVal >= 0) {
            return itemVal;
        }
        if (useGlobal) {
            return globalVal;
        }
        if (docHas && docVal >= 0) {
            return docVal;
        }
        return fallback;
    };

    QVector<int> seq;
    if (itemDwell && itemDwell->hasTiming) {
        seq = itemDwell->effectiveSequence();
    } else if (!m_globalDwellSequence.isEmpty()) {
        seq = m_globalDwellSequence;
    } else {
        seq = m_document.dwell.effectiveSequence();
    }

    const int grace = pickMs(itemDwell && itemDwell->hasGrace, itemDwell ? itemDwell->graceMs : -1,
                             m_globalGraceMs > 0, m_globalGraceMs,
                             m_document.dwell.hasGrace, m_document.dwell.graceMs,
                             DwellStateMachine::kDefaultInvalidGraceMs);
    const int scanGrace =
        pickMs(itemDwell && itemDwell->hasScanGrace, itemDwell ? itemDwell->scanGraceMs : -1,
               m_globalScanGraceMs >= 0, m_globalScanGraceMs, m_document.dwell.hasScanGrace,
               m_document.dwell.scanGraceMs, DwellStateMachine::kDefaultScanGraceMs);

    m_dwell->setDwellSequence(seq);
    m_dwell->setInvalidGraceMs(qMax(0, grace));
    m_dwell->setScanGraceMs(qMax(0, scanGrace));
}

void LayoutInstance::setGlobalDwellOverride(const QVector<int>& dwellSequence, int graceMs,
                                            int scanGraceMs)
{
    if (m_globalDwellSequence == dwellSequence && m_globalGraceMs == graceMs
        && m_globalScanGraceMs == scanGraceMs) {
        return;
    }
    m_globalDwellSequence = dwellSequence;
    m_globalGraceMs = graceMs;
    m_globalScanGraceMs = scanGraceMs;
    applyDwellForItem(m_activeDwellItemId);
}

void LayoutInstance::setProgressVisuals(const ProgressVisuals& visuals)
{
    m_progressVisuals = visuals;
    if (m_window) {
        m_window->setProgressVisuals(visuals);
    }
}

ProgressVisuals LayoutInstance::resolvedProgressVisuals(const LayoutItem* item) const
{
    ProgressVisuals v = m_progressVisuals;
    if (m_document.dwell.sectionPresent) {
        v = v.mergedWith(m_document.dwell);
    }
    if (item && item->dwell.sectionPresent) {
        v = v.mergedWith(item->dwell);
    }
    if (item) {
        const QColor fallback = m_window ? m_window->theme().text : QColor(255, 255, 255);
        const QColor fg = m_document.style.withOverrides(item->style).foreground.value_or(fallback);
        return v.withItemFlash(fg);
    }
    return v;
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

void LayoutInstance::setPropertyContext(const QVariantMap& props)
{
    m_props = props;
    if (m_window) {
        m_window->setPropertyContext(props);
    }
}

bool LayoutInstance::itemShown(const LayoutItem& item) const
{
    return itemIsShown(item, m_props);
}

void LayoutInstance::mutateItems(const std::function<void(LayoutItem&)>& fn)
{
    if (!fn) {
        return;
    }
    for (LayoutItem& it : m_document.items) {
        fn(it);
    }
    if (m_window) {
        m_window->setLayout(m_document);
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
    applyPlacement();
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
    if (usesDrawerMotion()) {
        // Dismiss must not be flipped back into appear by z-order restacks.
        if (isDismissing()) {
            m_window->keepAboveTaskbar();
            return;
        }
        if (!m_window->isVisible()) {
            playAppear();
            return;
        }
        if (isScaleAnimating()) {
            m_window->keepAboveTaskbar();
            return;
        }
        m_window->showAndRaise();
        return;
    }
    m_window->showAndRaise();
}

void LayoutInstance::hide()
{
    if (isDismissing()) {
        return;
    }
    if (usesDrawerMotion() && m_window && m_window->isVisible()) {
        playDismiss([this]() { forceHide(); });
        return;
    }
    forceHide();
}

void LayoutInstance::forceHide()
{
    cancelScaleAnim(false);
    if (m_window) {
        m_window->hide();
    }
    leaveGaze();
}

bool LayoutInstance::usesDrawerMotion() const
{
    return m_document.placement.drawerMotion;
}

bool LayoutInstance::isScaleAnimating() const
{
    return m_scalePhase != ScalePhase::Idle;
}

bool LayoutInstance::isDismissing() const
{
    return m_scalePhase == ScalePhase::Dismiss;
}

void LayoutInstance::cancelScaleAnim(bool invokeDone)
{
    m_scaleTimer.stop();
    m_scalePhase = ScalePhase::Idle;
    auto done = std::move(m_scaleDone);
    m_scaleDone = {};
    if (invokeDone && done) {
        done();
    }
}

void LayoutInstance::applyScale(double scale)
{
    if (!m_window || !m_scaleTargetGeom.isValid()) {
        return;
    }
    const QRect full = m_scaleTargetGeom;
    const int w = qMax(1, int(std::lround(full.width() * scale)));
    const int h = qMax(1, int(std::lround(full.height() * scale)));
    const int right = full.x() + full.width();
    const int bottom = full.y() + full.height();

    int x = full.center().x() - w / 2;
    int y = full.center().y() - h / 2;

    if (usesDrawerMotion()) {
        m_window->setGeometry(x, bottom - h, w, h);
        return;
    }

    using Anchor = LayoutWindowPlacement::Anchor;
    switch (m_document.placement.anchor) {
    case Anchor::TopLeft:
        x = full.x();
        y = full.y();
        break;
    case Anchor::TopCenter:
        y = full.y();
        break;
    case Anchor::TopRight:
        x = right - w;
        y = full.y();
        break;
    case Anchor::LeftCenter:
        x = full.x();
        break;
    case Anchor::RightCenter:
        x = right - w;
        break;
    case Anchor::BottomLeft:
        x = full.x();
        y = bottom - h;
        break;
    case Anchor::BottomCenter:
        y = bottom - h;
        break;
    case Anchor::BottomRight:
        x = right - w;
        y = bottom - h;
        break;
    case Anchor::Center:
    case Anchor::Default:
        break;
    }

    m_window->setGeometry(x, y, w, h);
}

void LayoutInstance::playAppear()
{
    if (!m_window || !m_document.showsBoardWindow()) {
        if (m_window) {
            m_window->showAndRaise();
        }
        return;
    }
    cancelScaleAnim(false);
    applyPlacement();
    m_scaleTargetGeom = m_window->geometry();
    if (!m_scaleTargetGeom.isValid() || m_scaleTargetGeom.width() < 2) {
        m_window->showAndRaise();
        return;
    }
    m_scalePhase = ScalePhase::Appear;
    m_scaleClock.restart();
    m_window->setMinimumSize(1, 1);
    setFadeOpacity(1.0);
    applyScale(kDrawerMinScale);
    m_window->showAndRaise();
    m_scaleTimer.start();
}

void LayoutInstance::playDismiss(std::function<void()> onDone)
{
    if (!m_window || !m_window->isVisible()) {
        cancelScaleAnim(false);
        if (onDone) {
            onDone();
        }
        return;
    }
    if (m_scalePhase == ScalePhase::Dismiss) {
        m_scaleDone = std::move(onDone);
        return;
    }
    cancelScaleAnim(false);
    m_scaleDone = std::move(onDone);
    if (!m_scaleTargetGeom.isValid() || m_scaleTargetGeom.width() < 2) {
        applyPlacement();
        m_scaleTargetGeom = m_window->geometry();
    }
    m_scalePhase = ScalePhase::Dismiss;
    m_scaleClock.restart();
    m_window->setMinimumSize(1, 1);
    m_scaleTimer.start();
}

void LayoutInstance::tickScaleAnim()
{
    if (m_scalePhase == ScalePhase::Idle) {
        m_scaleTimer.stop();
        return;
    }
    const int dur =
        m_scalePhase == ScalePhase::Appear ? kDrawerAppearMs : kDrawerDismissMs;
    const double t = qBound(0.0, double(m_scaleClock.elapsed()) / double(dur), 1.0);

    double scale = 1.0;
    if (m_scalePhase == ScalePhase::Appear) {
        const double e = 1.0 - (1.0 - t) * (1.0 - t);
        scale = kDrawerMinScale + (1.0 - kDrawerMinScale) * e;
    } else {
        const double e = t * t;
        scale = 1.0 - (1.0 - kDrawerMinScale) * e;
    }
    applyScale(qMax(kDrawerMinScale, scale));

    if (t < 1.0) {
        return;
    }

    const bool appearing = m_scalePhase == ScalePhase::Appear;
    auto done = std::move(m_scaleDone);
    m_scaleDone = {};
    m_scalePhase = ScalePhase::Idle;
    m_scaleTimer.stop();
    if (appearing) {
        applyPlacement();
        if (m_window) {
            m_window->keepAboveTaskbar();
        }
    }
    if (done) {
        done();
    }
}

void LayoutInstance::applyPlacement()
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

    m_window->setMaximumSize(16777215, 16777215);

    const QRect avail = boundsRectFor(m_document.effectiveBoundsMode());

    const int winW = qMax(40, p.width.resolveInt(avail.width(), 1000));
    const int winH = qMax(40, p.height.resolveInt(avail.height(), 560));
    m_window->setMinimumSize(qMax(40, qMin(winW, 80)), qMax(40, qMin(winH, 80)));
    m_window->resize(winW, winH);

    if (p.anchor == LayoutWindowPlacement::Anchor::Default && !p.x.isSet() && !p.y.isSet()) {
        placeRelative(0, 0);
        return;
    }

    if (!avail.isValid()) {
        placeRelative(0, 0);
        return;
    }

    const int margin = qMax(0, p.marginPx);
    const int w = m_window->width();
    const int h = m_window->height();
    // Use top+height (not inclusive bottom()+1) so flush anchors sit exactly on the
    // bounds edge when marginPx is 0.
    const int left = avail.x();
    const int top = avail.y();
    const int right = avail.x() + avail.width();   // exclusive
    const int bottom = avail.y() + avail.height(); // exclusive

    int x = left + margin;
    int y = top + margin;

    switch (p.anchor) {
    case LayoutWindowPlacement::Anchor::TopLeft:
        break;
    case LayoutWindowPlacement::Anchor::TopCenter:
        x = left + (avail.width() - w) / 2;
        break;
    case LayoutWindowPlacement::Anchor::TopRight:
        x = right - w - margin;
        break;
    case LayoutWindowPlacement::Anchor::Center:
        x = left + (avail.width() - w) / 2;
        y = top + (avail.height() - h) / 2;
        break;
    case LayoutWindowPlacement::Anchor::LeftCenter:
        x = left + margin;
        y = top + (avail.height() - h) / 2;
        break;
    case LayoutWindowPlacement::Anchor::RightCenter:
        x = right - w - margin;
        y = top + (avail.height() - h) / 2;
        break;
    case LayoutWindowPlacement::Anchor::BottomLeft:
        y = bottom - h - margin;
        break;
    case LayoutWindowPlacement::Anchor::BottomCenter:
        x = left + (avail.width() - w) / 2;
        y = bottom - h - margin;
        break;
    case LayoutWindowPlacement::Anchor::BottomRight:
        x = right - w - margin;
        y = bottom - h - margin;
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

    m_window->move(x, y);
}

void LayoutInstance::placeRelative(int offsetX, int offsetY)
{
    const QRect avail = boundsRectFor(m_document.effectiveBoundsMode());
    const QPoint origin = avail.isValid() ? (avail.topLeft() + QPoint(80, 80)) : QPoint(80, 80);
    m_window->move(origin + QPoint(offsetX, offsetY));
}

QRect LayoutInstance::boundsRectFor(BoundsMode mode) const
{
    // Placement uses the primary screen's reference rect. Do not use the window's
    // current QScreen* — cascade/move can pin it to the wrong monitor or a stale
    // geometry and break boundsMode (screen vs desktop).
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) {
        return QRect(0, 0, 1920, 1080);
    }
    return mode == BoundsMode::Screen ? screen->geometry() : screen->availableGeometry();
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
    // Apply layout-level boundsMode when the region does not override it.
    LayoutItem copy = item;
    if (copy.hasDwellRegion && !copy.dwellRegion.hasBoundsMode && m_document.hasBoundsMode) {
        copy.dwellRegion.hasBoundsMode = true;
        copy.dwellRegion.boundsMode = m_document.boundsMode;
    } else if (copy.hasDwellRegion && !copy.dwellRegion.hasBoundsMode
               && m_document.placement.hasBoundsMode) {
        copy.dwellRegion.hasBoundsMode = true;
        copy.dwellRegion.boundsMode = m_document.placement.boundsMode;
    }
    return DwellRegionSpace::resolveItem(copy, boardOrigin(), screenRect(), boardScreen());
}

QRect LayoutInstance::itemHitRect(const LayoutItem& item) const
{
    const auto r = resolveItem(item);
    const bool engaged = !m_dwellLipItemId.isEmpty() && m_dwellLipItemId == item.id;
    QRect hit = r.hitWithDriftLip(engaged);
    // While progress chrome is visible, include the on-screen progress region.
    if (m_activeDwellItemId == item.id && m_lastProgress > 0.01) {
        const QRect prog = progressHitRect(item, m_lastProgress);
        if (!prog.isEmpty()) {
            hit = hit.united(prog);
        }
    }
    return hit;
}

QRect LayoutInstance::progressHitRect(const LayoutItem& item, double progress) const
{
    if (item.participatesInBoardGrid()) {
        return {};
    }
    const auto resolved = resolveItem(item);
    if (!resolved.showBubble || resolved.band.onScreen.isEmpty()) {
        return {};
    }
    const QRectF strip = DwellRegionSpace::progressStrip(resolved.band, progress);
    // Prefer the growing progress strip; fall back to full on-screen band.
    if (!strip.isEmpty()) {
        return strip.toRect();
    }
    return resolved.band.onScreen.toRect();
}

bool LayoutInstance::containsVisibleUnboundedProgress(const QPointF& screenPoint) const
{
    if (m_activeDwellItemId.isEmpty() || m_lastProgress <= 0.01) {
        return false;
    }
    const LayoutItem* item = m_document.findItem(m_activeDwellItemId);
    if (!item || item->participatesInBoardGrid()) {
        return false;
    }
    if (m_dwellSuspended && !item->isDwellExempt()) {
        return false;
    }
    const QRect prog = progressHitRect(*item, m_lastProgress);
    // Also accept full item hit (logical + lip + progress) so drift stays on target.
    const QRect hit = itemHitRect(*item);
    const QPoint pt = screenPoint.toPoint();
    return (!prog.isEmpty() && prog.contains(pt)) || (!hit.isEmpty() && hit.contains(pt));
}

void LayoutInstance::setAutoCloseTiming(int idleMs, int fadeMs)
{
    m_autoCloseIdleMs = qMax(500, idleMs);
    m_autoCloseFadeMs = qMax(50, fadeMs);
}

void LayoutInstance::resetAutoCloseClock(qint64 nowMs)
{
    m_lastActivityMs = nowMs;
    m_autoCloseDismissDone = false;
    if (m_autoCloseDismissActive) {
        m_autoCloseDismissActive = false;
        cancelScaleAnim(false);
        if (m_window && m_document.showsBoardWindow()) {
            applyPlacement();
        }
    }
    setFadeOpacity(1.0);
}

double LayoutInstance::autoCloseOpacity(qint64 nowMs) const
{
    if (!m_autoCloseEnabled) {
        return 1.0;
    }
    const qint64 idle = nowMs - m_lastActivityMs;
    if (idle < m_autoCloseIdleMs) {
        return 1.0;
    }
    // Instant 50% after idle; stay there through the hold and the shrink.
    return kAutoCloseFadeFloor;
}

bool LayoutInstance::autoCloseFinished(qint64 nowMs) const
{
    Q_UNUSED(nowMs);
    return m_autoCloseEnabled && m_autoCloseDismissDone;
}

bool LayoutInstance::autoCloseIdleElapsed(qint64 nowMs) const
{
    if (!m_autoCloseEnabled) {
        return false;
    }
    return nowMs - m_lastActivityMs >= qint64(m_autoCloseIdleMs);
}

bool LayoutInstance::autoCloseReadyToDismiss(qint64 nowMs) const
{
    if (!m_autoCloseEnabled) {
        return false;
    }
    return nowMs - m_lastActivityMs
           >= qint64(m_autoCloseIdleMs) + qint64(m_autoCloseFadeMs);
}

void LayoutInstance::applyAutoCloseVisuals(qint64 nowMs)
{
    if (!m_window) {
        return;
    }

    setFadeOpacity(autoCloseOpacity(nowMs));

    if (!m_autoCloseEnabled || !m_document.showsBoardWindow()) {
        return;
    }

    if (!autoCloseReadyToDismiss(nowMs)) {
        if (m_autoCloseDismissActive) {
            m_autoCloseDismissActive = false;
            m_autoCloseDismissDone = false;
            cancelScaleAnim(false);
            applyPlacement();
            setFadeOpacity(autoCloseOpacity(nowMs));
        }
        return;
    }

    // Drawer collapse owns playDismiss so chrome can hide the board when it ends.
    if (usesDrawerMotion()) {
        return;
    }

    if (!m_autoCloseDismissActive && !isDismissing()) {
        m_autoCloseDismissActive = true;
        playDismiss([this]() { m_autoCloseDismissDone = true; });
    }
}

void LayoutInstance::setFadeOpacity(double opacity)
{
    if (m_window) {
        m_window->setBoardOpacity(qBound(0.0, opacity, 1.0));
    }
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

QRect LayoutInstance::itemScreenRect(const QString& itemId) const
{
    const LayoutItem* item = m_document.findItem(itemId);
    if (!item || !itemShown(*item)) {
        return {};
    }
    return itemHitRect(*item);
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
        if (!allow(item) || !itemShown(item) || item.participatesInBoardGrid()) {
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
        if (!item.interactive || !itemShown(item) || item.participatesInBoardGrid()) {
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

    const ProgressVisuals v = resolvedProgressVisuals(item);

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
        m_lastProgress = 0.0;
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
    m_lastProgress = 0.0;
    m_dwell->leave();
    if (m_window) {
        m_window->setHoverState(QString(), 0.0);
    }
}

} // namespace gazer
