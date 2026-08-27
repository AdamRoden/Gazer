#include "assist/ComboMouse.h"

#include "input/MouseInjector.h"
#include "ui/KeySymbols.h"
#include "ui/Theme.h"
#include "utils/Log.h"
#include "utils/ScreenGrab.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPen>
#include <QRect>
#include <QRectF>
#include <QString>
#include <QtMath>

namespace gazer {

namespace {

struct SliceSpec {
    ComboMouseHit::Slice id;
    const char* icon;
};

constexpr SliceSpec kSlices[] = {
    {ComboMouseHit::Slice::Right, "MouseRightClick"},
    {ComboMouseHit::Slice::Move, "SizeAndPosition"},
    {ComboMouseHit::Slice::Cancel, "Quit"},
    {ComboMouseHit::Slice::Drag, "MouseLeftDownUp"},
    {ComboMouseHit::Slice::Left, "MouseLeftClick"},
};

[[nodiscard]] QPainterPath wedgePath(const QPointF& c, const ComboMouseHit::Wedge& w)
{
    const double qtStart = 90.0 - w.startCw;
    QPainterPath slice;
    slice.moveTo(c);
    slice.arcTo(QRectF(c.x() - w.outer, c.y() - w.outer, w.outer * 2.0, w.outer * 2.0), qtStart,
                -w.spanDeg);
    slice.closeSubpath();
    QPainterPath hole;
    hole.addEllipse(c, w.inner, w.inner);
    return slice.subtracted(hole);
}

void paintPathProgress(QPainter& p, const QPainterPath& zone, const QPointF& c, double outerR,
                       double startDeg, double spanDeg, double progress, const ProgressVisuals& vis)
{
    if (progress <= 0.0 || zone.isEmpty()) {
        return;
    }
    if (vis.style.fillBackground) {
        QColor fill = vis.fillColor.isValid() ? vis.fillColor : vis.progressColor;
        fill.setAlpha(qBound(0, int(fill.alpha() * progress + 24 * progress), 220));
        p.save();
        p.setClipPath(zone);
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawEllipse(c, outerR * progress, outerR * progress);
        p.restore();
    }
    if (vis.style.border) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(vis.borderColor, 2.0 + 2.4 * progress, Qt::SolidLine, Qt::FlatCap,
                      Qt::RoundJoin));
        p.drawPath(zone);
    }
    if (vis.style.radial) {
        const qreal penW = 5.0;
        const QRectF arc(c.x() - outerR + penW, c.y() - outerR + penW, (outerR - penW) * 2.0,
                         (outerR - penW) * 2.0);
        p.setBrush(Qt::NoBrush);
        QColor track = vis.progressColor;
        track.setAlpha(70);
        p.setPen(QPen(track, penW, Qt::SolidLine, Qt::FlatCap));
        p.drawArc(arc, int(startDeg * 16.0), int(spanDeg * 16.0));
        p.setPen(QPen(vis.progressColor, penW, Qt::SolidLine, Qt::FlatCap));
        p.drawArc(arc, int(startDeg * 16.0), int(spanDeg * 16.0 * progress));
    }
}

} // namespace

class ComboMouse::WheelOverlay final : public OverlaySurface {
public:
    WheelOverlay() { hide(); }

    void setState(const ComboMouseHit::Layout& layout, ComboMouseHit::Band band,
                  ComboMouseHit::Slice slice, double dwellProg, bool dragHeld, QPointF dir,
                  const QColor& accent, const ThemeColors& theme, const ProgressVisuals& progress)
    {
        m_layout = layout;
        m_band = band;
        m_slice = slice;
        m_dwellProg = qBound(0.0, dwellProg, 1.0);
        m_dragHeld = dragHeld;
        m_dir = dir;
        m_theme = theme;
        m_progress = progress;
        if (accent.isValid()) {
            m_accent = accent;
        }
        update();
    }

    void place(const QPoint& origin, const QRectF& screen)
    {
        const QRect wr = ComboMouseHit::overlayRect(QPointF(origin), m_layout, screen);
        m_originLocal = QPointF(origin) - QPointF(wr.topLeft());
        if (wr.size() != size()) {
            resize(wr.size());
        }
        if (pos() != wr.topLeft()) {
            move(wr.topLeft());
        }
        showOverlay();
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QPointF c = m_originLocal;
        const QColor cyan = m_accent.isValid() ? m_accent : ThemeColors::defaultProgressColor();
        paintCommands(p, c, cyan);
        paintYellowRing(p, c, cyan);
    }

private:
    void paintCommands(QPainter& p, const QPointF& c, const QColor& cyan)
    {
        for (int i = 0; i < m_layout.wedgeCount; ++i) {
            paintWedge(p, c, cyan, m_layout.wedges[i]);
        }
    }

    void paintWedge(QPainter& p, const QPointF& c, const QColor& cyan, const ComboMouseHit::Wedge& w)
    {
        const bool hover = m_band == ComboMouseHit::Band::Slice && m_slice == w.id;
        const bool armed = w.id == ComboMouseHit::Slice::Drag && m_dragHeld;
        const QPainterPath zone = wedgePath(c, w);

        QColor fill(12, 14, 18, hover || armed ? 230 : 210);
        if (armed && m_theme.cellActive.isValid()) {
            fill = m_theme.cellActive;
        }
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawPath(zone);

        const QColor primary = m_theme.accent.isValid() ? m_theme.accent : cyan;
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(armed && m_theme.accentHover.isValid() ? m_theme.accentHover : primary,
                      hover || armed ? 3.0 : 2.0, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
        p.drawPath(zone);

        ProgressVisuals vis = m_progress;
        vis.style.fillBackground = true;
        vis.style.border = false;
        vis.style.radial = true;
        vis.fillColor = QColor(cyan.red(), cyan.green(), cyan.blue(), 90);
        vis.progressColor = cyan;
        vis.borderColor = primary;
        const double qtStart = 90.0 - w.startCw;
        const double prog = hover ? qMax(0.08, m_dwellProg) : 0.0;
        paintPathProgress(p, zone, c, w.outer, qtStart, -w.spanDeg, prog, vis);

        const double midDeg = w.startCw + w.spanDeg * 0.5;
        const double rad = qDegreesToRadians(midDeg - 90.0);
        const double midR = (w.inner + w.outer) * 0.5;
        const QPointF mid(c.x() + qCos(rad) * midR, c.y() + qSin(rad) * midR);
        const double iconSide = qBound(40.0, (w.outer - w.inner) * 0.78, 108.0);
        const QRectF icon(mid.x() - iconSide * 0.5, mid.y() - iconSide * 0.5, iconSide, iconSide);
        const QColor fg =
            armed && m_theme.accent.isValid() ? m_theme.accent : QColor(255, 255, 255, 235);
        KeySymbols::paint(p, QLatin1String(kSlices[int(w.id)].icon), icon, fg);
    }

    void paintYellowRing(QPainter& p, const QPointF& c, const QColor& cyan)
    {
        const double inner = m_layout.deadzone;
        const double outer = m_layout.ringOuter;
        QPainterPath ring;
        ring.addEllipse(c, outer, outer);
        QPainterPath hole;
        hole.addEllipse(c, inner, inner);
        const QPainterPath annulus = ring.subtracted(hole);
        const bool drift = m_band == ComboMouseHit::Band::Drift;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 196, 40, drift ? 120 : 80));
        p.drawPath(annulus);
        ProgressVisuals vis = m_progress;
        vis.style.fillBackground = true;
        vis.style.border = true;
        vis.style.radial = true;
        vis.borderColor = cyan;
        vis.fillColor = QColor(cyan.red(), cyan.green(), cyan.blue(), 70);
        vis.progressColor = cyan;
        paintPathProgress(p, annulus, c, outer, 90.0, -360.0, drift ? m_dwellProg : 0.0, vis);

        if (drift) {
            const double len = qSqrt(m_dir.x() * m_dir.x() + m_dir.y() * m_dir.y());
            if (len > 0.05) {
                const QPointF tip = c + m_dir / len * ((inner + outer) * 0.5);
                p.setPen(QPen(QColor(cyan.red(), cyan.green(), cyan.blue(), 220), 2.4,
                              Qt::SolidLine, Qt::RoundCap));
                p.drawLine(c, tip);
            }
        }
    }

    ComboMouseHit::Layout m_layout;
    ComboMouseHit::Band m_band = ComboMouseHit::Band::None;
    ComboMouseHit::Slice m_slice = ComboMouseHit::Slice::Right;
    double m_dwellProg = 0.0;
    bool m_dragHeld = false;
    QPointF m_dir;
    QColor m_accent = ThemeColors::defaultProgressColor();
    ThemeColors m_theme = ThemeColors::darkPreset();
    ProgressVisuals m_progress;
    QPointF m_originLocal;
};

ComboMouse::ComboMouse(QObject* parent)
    : QObject(parent)
{
    m_clock.start();
    m_overlay = std::make_unique<WheelOverlay>();
    connect(&m_dwell, &DwellStateMachine::itemActivated, this,
            [this](const QString& id) { onActivated(id); });
}

ComboMouse::~ComboMouse()
{
    hideWheel();
    releaseDrag();
}

OverlaySurface* ComboMouse::overlay() const
{
    return m_overlay.get();
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

QRectF ComboMouse::screenRect() const
{
    return QRectF(overlayScreenGeometry());
}

ComboMouseHit::Layout ComboMouse::layout() const
{
    const QRectF screen = screenRect();
    const double screenH = screen.height() >= 200.0 ? screen.height() : 1080.0;
    const double pieThick = qBound(52.0, screenH * 0.08, 92.0);
    const double fullPie =
        qMax(ComboMouseHit::kRingOuterPx + pieThick, qBound(140.0, screenH * 0.16, 280.0));
    return ComboMouseHit::makeLayout(QPointF(m_origin), screen, ComboMouseHit::kHoleRadiusPx,
                                     ComboMouseHit::kRingOuterPx, fullPie);
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
    m_overlay->setState(L, band, slice, dwellProg, isDragHeld(), dir, m_accent, m_theme,
                        m_progress);
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
    m_overlay->setState(m_layout, ComboMouseHit::Band::Deadzone, ComboMouseHit::Slice::Right, 0.0,
                        isDragHeld(), {}, m_accent, m_theme, m_progress);
    m_overlay->place(m_origin, screenRect());
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
        adoptLayout(layout());
        m_overlay->setState(m_layout, ComboMouseHit::Band::Drift, ComboMouseHit::Slice::Right,
                            m_dwell.progress(), isDragHeld(), m_nudgeDir, m_accent, m_theme,
                            m_progress);
        m_overlay->place(m_origin, screenRect());
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
    if (qAbs(dir.x()) < 0.01 && qAbs(dir.y()) < 0.01) {
        return;
    }
    QPoint step(0, 0);
    if (qAbs(dir.x()) >= qAbs(dir.y())) {
        step.setX(dir.x() > 0.0 ? 1 : -1);
    } else {
        step.setY(dir.y() > 0.0 ? 1 : -1);
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
