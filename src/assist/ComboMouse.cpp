#include "assist/ComboMouse.h"

#include "assist/PieOverlay.h"
#include "input/MouseInjector.h"
#include "utils/Log.h"
#include "utils/ScreenGrab.h"

#include <QRect>
#include <QRectF>
#include <QString>

namespace gazer {
namespace {

constexpr const char* kComboSliceIcons[ComboMouseHit::kSliceCount] = {
    "mouseRightClick",
    "mouseMove",
    "close",
    "mouseLeftDrag",
    "mouseLeftClick",
};

} // namespace

ComboMouse::ComboMouse(QObject* parent)
    : QObject(parent)
{
    m_clock.start();
    m_overlay = std::make_unique<PieOverlay>();
    connect(&m_dwell, &DwellStateMachine::itemActivated, this,
            [this](const QString& id) { onActivated(id); });
}

ComboMouse::~ComboMouse()
{
    hideWheel();
    releaseDrag();
}

void ComboMouse::setScanGraceMs(int ms)
{
    m_dwell.setScanGraceMs(ms);
}

void ComboMouse::setDwellGraceMs(int ms)
{
    m_dwell.setInvalidGraceMs(ms);
}

void ComboMouse::setDwellMs(int ms)
{
    m_dwell.setDwellMs(ms);
}

void ComboMouse::setDwellSequence(const QVector<int>& ms)
{
    m_dwell.setDwellSequence(ms);
}

void ComboMouse::setPaused(bool paused)
{
    if (m_paused == paused) {
        return;
    }
    m_paused = paused;
    m_dwell.leave();
    if (m_paused) {
        releaseDrag();
    }
}

void ComboMouse::setAccent(const QColor& c)
{
    m_accent = c;
}

void ComboMouse::setRadii(int innerPx, int sharedPx, int outerPx)
{
    double inner = double(innerPx);
    double shared = double(sharedPx);
    double outer = double(outerPx);
    ComboMouseHit::clampRadii(inner, shared, outer);
    m_innerPx = inner;
    m_sharedPx = shared;
    m_outerPx = outer;
    if (m_wheelVisible) {
        pushOverlay(layout(), m_band, m_slice, m_dwell.progress(), m_nudgeDir);
    }
}

void ComboMouse::setAnnulusColors(const QColor& inner, const QColor& outer)
{
    if (inner.isValid()) {
        m_innerColor = inner;
    }
    if (outer.isValid()) {
        m_outerColor = outer;
    }
    if (m_wheelVisible) {
        pushOverlay(layout(), m_band, m_slice, m_dwell.progress(), m_nudgeDir);
    }
}

QRect ComboMouse::originGateRect() const
{
    const int r = qMax(24, qRound(m_outerPx)) + 16;
    return QRect(m_origin.x() - r, m_origin.y() - r, r * 2, r * 2);
}

QRectF ComboMouse::screenRect() const
{
    return QRectF(overlayScreenGeometry());
}

ComboMouseHit::Layout ComboMouse::layout() const
{
    return ComboMouseHit::makeLayout(QPointF(m_origin), screenRect(), m_innerPx, m_sharedPx,
                                     m_outerPx);
}

void ComboMouse::adoptLayout(const ComboMouseHit::Layout& L)
{
    if (!ComboMouseHit::packingEqual(L, m_layout)) {
        m_dwell.leave();
    }
    m_layout = L;
}

void ComboMouse::pushOverlay(const ComboMouseHit::Layout& L, ComboMouseHit::Band band,
                             ComboMouseHit::Slice slice, double dwellProg, QPointF dir)
{
    adoptLayout(L);
    m_band = band;
    m_slice = slice;
    PieOverlay::Appearance a;
    a.layout = L;
    a.band = band;
    a.slice = slice;
    a.dwellProg = dwellProg;
    a.accent = m_accent;
    a.theme = m_theme;
    a.innerColor = m_innerColor;
    a.outerColor = m_outerColor;
    for (int i = 0; i < ComboMouseHit::kSliceCount; ++i) {
        a.sliceIcons[i] = kComboSliceIcons[i];
    }
    a.innerActive = band == ComboMouseHit::Band::Drift;
    a.driftDir = dir;
    a.armed = isDragHeld();
    a.armedSlice = ComboMouseHit::Slice::Drag;
    m_overlay->setAppearance(a);
    m_overlay->place(m_origin, screenRect());
}

void ComboMouse::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    m_dwell.leave();
    if (!m_enabled) {
        hideWheel();
        releaseDrag();
    }
    GAZER_INFO << "ComboMouse" << (m_enabled ? "ON" : "OFF");
    emit enabledChanged(m_enabled);
}

void ComboMouse::showAt(const QPoint& pos)
{
    if (!m_enabled) {
        return;
    }
    m_origin = pos;
    clampOrigin();
    pinCursor();
    m_wheelVisible = true;
    m_dwell.leave();
    m_layout = layout();
    m_band = ComboMouseHit::Band::Deadzone;
    m_slice = ComboMouseHit::Slice::Right;
    pushOverlay(m_layout, m_band, m_slice, 0.0, {});
    emit wheelVisibleChanged(true);
}

void ComboMouse::requestMove()
{
    if (!m_enabled) {
        return;
    }
    hideWheel();
    emit placeRequested();
}

void ComboMouse::hideWheel()
{
    const bool was = m_wheelVisible;
    m_wheelVisible = false;
    m_dwell.leave();
    if (m_overlay) {
        m_overlay->hide();
    }
    if (was) {
        emit wheelVisibleChanged(false);
    }
}

void ComboMouse::pinCursor()
{
    QString err;
    if (!MouseInjector::moveTo(m_origin.x(), m_origin.y(), &err) && !err.isEmpty()) {
        GAZER_WARN << "ComboMouse pin cursor:" << err;
    }
}

void ComboMouse::clampOrigin()
{
    const QRect work = overlayScreenGeometry();
    if (work.width() < 2 || work.height() < 2) {
        return;
    }
    m_origin.setX(qBound(work.left(), m_origin.x(), work.left() + work.width() - 1));
    m_origin.setY(qBound(work.top(), m_origin.y(), work.top() + work.height() - 1));
}

void ComboMouse::moveOrigin(const QPoint& pos)
{
    m_origin = pos;
    clampOrigin();
    pinCursor();
    if (m_wheelVisible && m_overlay) {
        pushOverlay(layout(), ComboMouseHit::Band::Drift, ComboMouseHit::Slice::Right,
                    m_dwell.progress(), m_nudgeDir);
    }
}

bool ComboMouse::holdingLeft() const
{
    return m_heldQuery ? m_heldQuery() : m_localHeld;
}

bool ComboMouse::isDragHeld() const
{
    return m_enabled && holdingLeft();
}

void ComboMouse::setDragHeld(bool held)
{
    if (holdingLeft() == held) {
        return;
    }
    if (m_hold) {
        if (!m_hold(held)) {
            return;
        }
    } else {
        QString err;
        if (held) {
            if (!MouseInjector::buttonDown(QStringLiteral("left"), &err)) {
                GAZER_WARN << "ComboMouse drag down:" << err;
                return;
            }
        } else if (!MouseInjector::buttonUp(QStringLiteral("left"), &err)) {
            GAZER_WARN << "ComboMouse drag up:" << err;
            return;
        }
    }
    m_localHeld = held;
}

void ComboMouse::releaseDrag()
{
    if (holdingLeft()) {
        setDragHeld(false);
    }
}

void ComboMouse::fireSlice(ComboMouseHit::Slice slice)
{
    if (m_paused) {
        return;
    }
    switch (slice) {
    case ComboMouseHit::Slice::Cancel:
        setEnabled(false);
        break;
    case ComboMouseHit::Slice::Drag:
        setDragHeld(!holdingLeft());
        break;
    case ComboMouseHit::Slice::Left: {
        releaseDrag();
        pinCursor();
        QString err;
        if (!MouseInjector::click(QStringLiteral("left"), &err)) {
            GAZER_WARN << "ComboMouse left click:" << err;
        }
        break;
    }
    case ComboMouseHit::Slice::Right: {
        releaseDrag();
        pinCursor();
        QString err;
        if (!MouseInjector::click(QStringLiteral("right"), &err)) {
            GAZER_WARN << "ComboMouse right click:" << err;
        }
        break;
    }
    case ComboMouseHit::Slice::Move:
        requestMove();
        break;
    }
}

QString ComboMouse::hitId(const ComboMouseHit::Result& h)
{
    if (h.band == ComboMouseHit::Band::Drift) {
        return QStringLiteral("ring");
    }
    if (h.band == ComboMouseHit::Band::Slice) {
        return QStringLiteral("slice.%1").arg(int(h.slice));
    }
    return {};
}

void ComboMouse::nudgeToward(const QPointF& dir)
{
    const QPoint step = ComboMouseHit::snap8(dir);
    if (step.x() == 0 && step.y() == 0) {
        return;
    }
    moveOrigin(m_origin + step);
}

void ComboMouse::onActivated(const QString& id)
{
    if (m_paused || !m_enabled) {
        return;
    }
    if (id == QLatin1String("ring")) {
        nudgeToward(m_nudgeDir);
        return;
    }
    if (id.startsWith(QLatin1String("slice."))) {
        bool ok = false;
        const int i = id.mid(6).toInt(&ok);
        if (ok && i >= 0 && i < ComboMouseHit::kSliceCount) {
            fireSlice(ComboMouseHit::Slice(i));
        }
        if (m_wheelVisible) {
            m_dwell.leave();
        }
    }
}

bool ComboMouse::containsGaze(const GazePoint& point) const
{
    if (!m_wheelVisible || !point.valid) {
        return false;
    }
    const auto h = ComboMouseHit::hit(point.toPointF(), QPointF(m_origin), layout());
    return h.band != ComboMouseHit::Band::None;
}

void ComboMouse::onGaze(const GazePoint& point, bool pauseInput)
{
    if (!m_enabled || !m_wheelVisible) {
        return;
    }
    GazePoint gp = point;
    if (gp.timestampMs <= 0) {
        gp.timestampMs = m_clock.elapsed();
    }

    const ComboMouseHit::Layout L = layout();
    if (!gp.valid || pauseInput || m_paused) {
        m_dwell.leave();
        pushOverlay(L, ComboMouseHit::Band::Deadzone, ComboMouseHit::Slice::Right, 0.0, {});
        return;
    }

    const QPointF g = gp.toPointF();
    const auto h = ComboMouseHit::hit(g, QPointF(m_origin), L);
    if (h.band == ComboMouseHit::Band::Drift) {
        m_nudgeDir = g - QPointF(m_origin);
    }
    if (h.band == ComboMouseHit::Band::Deadzone) {
        pinCursor();
    }
    adoptLayout(L);
    m_dwell.onGazeSample(gp, hitId(h));
    if (!m_wheelVisible) {
        return;
    }
    pushOverlay(L, h.band, h.slice, m_dwell.progress(), m_nudgeDir);
}

} // namespace gazer
