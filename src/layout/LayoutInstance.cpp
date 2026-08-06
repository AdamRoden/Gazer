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
            [this](const QString& itemId) { emit itemActivated(m_instanceId, itemId); });

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
                if (m_window) {
                    m_window->flashItem(itemId);
                }
                clearEdgeBubble();
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
    if (m_window) {
        m_window->setProgressVisuals(visuals);
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
    m_window->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

    if (p.widthPx > 0 && p.heightPx > 0) {
        m_window->setMinimumSize(qMax(40, qMin(p.widthPx, 80)), qMax(40, qMin(p.heightPx, 80)));
        m_window->resize(p.widthPx, p.heightPx);
    } else {
        m_window->setMinimumSize(320, 160);
        m_window->resize(1000, 560);
    }

    if (p.anchor == LayoutWindowPlacement::Anchor::Default) {
        placeRelative(cascadeOffset, cascadeOffset);
        return;
    }

    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) {
        placeRelative(cascadeOffset, cascadeOffset);
        return;
    }

    const QRect avail = screen->availableGeometry();
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

    m_window->move(x, y);
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
    QString hit;
    for (const LayoutItem& item : m_document.items) {
        if (!item.interactive || item.participatesInBoardGrid()) {
            continue;
        }
        const auto r = resolveItem(item);
        if (!r.hit.isEmpty() && r.hit.contains(pt)) {
            hit = item.id; // last match wins
        }
    }
    if (!hit.isEmpty()) {
        return hit;
    }
    return m_window ? m_window->hitTestGlobal(screenPoint) : QString();
}

bool LayoutInstance::containsScreenPoint(const QPointF& screenPoint) const
{
    if (!m_window) {
        return false;
    }
    const QPoint pt = screenPoint.toPoint();
    if (m_window->isVisible() && screenRect().contains(pt)) {
        return true;
    }
    for (const LayoutItem& item : m_document.items) {
        if (!item.interactive || item.participatesInBoardGrid()) {
            continue;
        }
        const auto r = resolveItem(item);
        if (!r.hit.isEmpty() && r.hit.contains(pt)) {
            return true;
        }
    }
    return false;
}

void LayoutInstance::syncEdgeBubble(const QString& itemId, double progress)
{
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

    EdgeBubbleOverlay::Bubble b;
    b.key = m_instanceId; // one slot per board instance
    b.label = item->label;
    b.band = resolved.band;
    b.progress = progress;
    b.color = QColor(0, 220, 255);
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
        }
        m_activeDwellItemId = itemIdUnderGaze;
        applyDwellForItem(itemIdUnderGaze);
    }
    m_dwell->onGazeSample(point, itemIdUnderGaze);
}

void LayoutInstance::leaveGaze()
{
    clearEdgeBubble();
    m_activeDwellItemId.clear();
    m_dwell->leave();
    if (m_window) {
        m_window->setHoverState(QString(), 0.0);
    }
}

} // namespace gazer
