#include "assist/LookToScroll.h"

#include "assist/LtsSpeed.h"
#include "assist/PieOverlay.h"
#include "input/MouseInjector.h"
#include "ui/KeySymbols.h"
#include "ui/OverlaySurface.h"
#include "ui/Theme.h"
#include "utils/Log.h"
#include "utils/ScreenGrab.h"

#include <QColor>
#include <QCursor>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPen>
#include <QScreen>
#include <QtMath>

namespace gazer {

namespace {

void paintRoundProgress(QPainter& p, const QPointF& c, double radius, double prog, const QColor& color)
{
    if (prog <= 0.01 || radius < 4.0) {
        return;
    }
    p.setBrush(Qt::NoBrush);
    const qreal w = qBound(2.6, radius * 0.14, 4.8);
    p.setPen(QPen(color, w, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(QRectF(c.x() - radius, c.y() - radius, radius * 2.0, radius * 2.0), 90 * 16,
              int(-360 * 16 * prog));
}

} // namespace

class LookToScroll::RingOverlay final : public OverlaySurface {
public:
    RingOverlay()
    {
        resize(m_box, m_box);
        hide();
    }

    void setState(int deadzonePx, int falloffPx, double activity, double centerProg, double dirX,
                  double dirY, LtsIndicator style, double hubRadius, bool paused = false)
    {
        m_deadzone = deadzonePx;
        m_falloff = qMax(40, falloffPx);
        m_activity = qBound(0.0, activity, 1.0);
        m_centerProg = qBound(0.0, centerProg, 1.0);
        m_dirX = dirX;
        m_dirY = dirY;
        m_style = style;
        m_hubR = qMax(8.0, hubRadius);
        m_paused = paused;

        const int activator = qMax(8, qRound(m_hubR));
        const bool compact = m_paused || m_style == LtsIndicator::PauseOnly;
        int side = activator * 2 + 36;
        if (!compact) {
            const int grow = qMax(48, m_falloff / 3);
            const int halo = qRound(orbThickness(m_deadzone) * 2.2);
            side = (m_deadzone + grow) * 2 + halo * 2 + 16;
            side = qMax(side, activator * 2 + 36);
        }
        if (side != m_box) {
            m_box = side;
            resize(m_box, m_box);
        }
        update();
    }

    void placeCenter(const QPoint& screenCenter)
    {
        move(screenCenter.x() - width() / 2, screenCenter.y() - height() / 2);
        showOverlay();
        update();
    }

    void setAccent(const QColor& c)
    {
        if (c.isValid()) {
            m_accent = c;
            update();
        }
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QPointF c(rect().center());
        const QColor cyan = m_accent.isValid() ? m_accent : ThemeColors::defaultProgressColor();
        if (!m_paused && m_style != LtsIndicator::PauseOnly) {
            paintOrb(p, c, cyan);
        }
        paintActivator(p, c, cyan);
    }

private:
    [[nodiscard]] static double orbThickness(int deadzonePx)
    {
        return qBound(16.0, double(qMax(1, deadzonePx)) * 0.24, 40.0);
    }

    void paintOrb(QPainter& p, const QPointF& c, const QColor& cyan)
    {
        const bool hollow = m_style == LtsIndicator::Hollow;
        double stretch = 0.0;
        double ang = 0.0;
        const double len = qSqrt(m_dirX * m_dirX + m_dirY * m_dirY);
        if (len > 0.05 && m_activity > 0.02) {
            stretch = qMax(48.0, m_falloff / 3.0) * m_activity;
            ang = qRadiansToDegrees(qAtan2(m_dirY / len, m_dirX / len));
        }
        const int alpha = hollow ? int(40 + 32 * m_activity) : int(22 + 20 * m_activity);
        const OrbKey key{width(),
                         height(),
                         m_deadzone,
                         alpha,
                         cyan.rgba(),
                         int(m_style),
                         qRound(stretch),
                         stretch > 0.5 ? qRound(ang) : 0};
        if (key != m_orbKey || m_orbBlur.isNull()) {
            m_orbKey = key;
            m_orbBlur = renderOrbBlur(c, stretch, ang, cyan, alpha, hollow);
        }
        p.drawImage(rect().topLeft(), m_orbBlur);
    }

    [[nodiscard]] QImage renderOrbBlur(const QPointF& c, double stretch, double ang,
                                       const QColor& cyan, int alpha, bool hollow) const
    {
        const double R = double(qMax(1, m_deadzone));
        const double shift = stretch * 0.5;
        QPainterPath body;
        body.addEllipse(QPointF(shift, 0), R + shift, R);
        if (hollow) {
            QPainterPathStroker s;
            s.setWidth(orbThickness(m_deadzone));
            s.setCapStyle(Qt::RoundCap);
            s.setJoinStyle(Qt::RoundJoin);
            body = s.createStroke(body);
        }

        QImage src(size(), QImage::Format_ARGB32_Premultiplied);
        src.fill(Qt::transparent);
        {
            QPainter ip(&src);
            ip.setRenderHint(QPainter::Antialiasing, true);
            ip.translate(c);
            if (stretch > 0.5) {
                ip.rotate(ang);
            }
            ip.setPen(Qt::NoPen);
            ip.setBrush(QColor(cyan.red(), cyan.green(), cyan.blue(), alpha));
            ip.drawPath(body);
        }
        const int factor = 5;
        const QSize small(qMax(1, src.width() / factor), qMax(1, src.height() / factor));
        return src.scaled(small, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
            .scaled(src.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    void paintActivator(QPainter& p, const QPointF& c, const QColor& accent)
    {
        const double hubR = m_hubR;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 50));
        p.drawEllipse(c, hubR, hubR);
        p.setBrush(QColor(accent.red(), accent.green(), accent.blue(), 40));
        p.drawEllipse(c, hubR, hubR);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(accent.red(), accent.green(), accent.blue(), 150), 2.2, Qt::SolidLine,
                      Qt::RoundCap));
        p.drawEllipse(c, hubR, hubR);
        if (m_paused) {
            const double side = hubR * 1.2;
            const QRectF icon(c.x() - side * 0.5, c.y() - side * 0.5, side, side);
            const QColor fg(255, 255, 255, 230);
            KeySymbols::paint(p, QStringLiteral("Sleep"), icon, fg);
        } else {
            paintRoundProgress(p, c, qMax(4.0, hubR - 1.5), m_centerProg, accent);
        }
    }

    int m_deadzone = 110;
    int m_falloff = 360;
    int m_box = 260;
    double m_hubR = 27.0;
    double m_activity = 0.0;
    double m_centerProg = 0.0;
    double m_dirX = 0.0;
    double m_dirY = 0.0;
    LtsIndicator m_style = LtsIndicator::Filled;
    bool m_paused = false;
    QColor m_accent = ThemeColors::defaultProgressColor();

    struct OrbKey {
        int w = 0;
        int h = 0;
        int deadzone = 0;
        int alpha = 0;
        QRgb rgb = 0;
        int style = 0;
        int stretchQ = 0;
        int angQ = 0;
        bool operator==(const OrbKey&) const = default;
    };
    OrbKey m_orbKey;
    QImage m_orbBlur;
};

LookToScroll::LookToScroll(QObject* parent)
    : QObject(parent)
{
    m_clock.start();
    m_overlay = std::make_unique<RingOverlay>();
    m_plus = std::make_unique<PieOverlay>();
    connect(&m_plusDwell, &DwellStateMachine::itemActivated, this, [this](const QString& id) {
        if (!m_plusOpen || !m_enabled) {
            return;
        }
        firePlusAction(ltsMenuActionFromId(id));
        if (m_plusOpen) {
            m_plusDwell.leave();
        }
    });
}

LookToScroll::~LookToScroll()
{
    m_scroller.reset();
    hidePlus();
    hideOverlay();
}

void LookToScroll::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    m_lastTickMs = -1;
    m_lastSampleMs = -1;
    m_outsideSec = 0.0;
    m_centerProgress = 0.0;
    m_replacing = false;
    m_scrollSuspended = false;
    m_scrollEngaged = false;
    m_plusOpen = false;
    m_lookAwaySec = 0.0;
    m_plusDwell.leave();
    m_invalidGrace.reset();
    m_scroller.reset();
    if (!m_enabled) {
        m_hasOrigin = false;
        hidePlus();
        hideOverlay();
    } else if (!m_hasOrigin) {
        setScrollOrigin(QCursor::pos());
    } else {
        pinCursorToOrigin();
    }
    GAZER_INFO << "LookToScroll" << (m_enabled ? "ON" : "OFF");
    emit enabledChanged(m_enabled);
    emit scrollSuspendedChanged(false);
}

void LookToScroll::setScrollOrigin(const QPoint& pos)
{
    m_origin = pos;
    m_hasOrigin = true;
    m_scroller.reset();
    pinCursorToOrigin();
}

void LookToScroll::pinCursorToOrigin()
{
    if (!m_hasOrigin) {
        return;
    }
    const QPoint now = QCursor::pos();
    // Qt/Windows DPI can round setPos by a pixel. Warping every sample against
    // that error injects a 1px mouse oscillation that shakes the scroll target.
    if (qAbs(now.x() - m_origin.x()) <= 2 && qAbs(now.y() - m_origin.y()) <= 2) {
        return;
    }
    QString err;
    if (!MouseInjector::moveTo(m_origin.x(), m_origin.y(), &err) && !err.isEmpty()) {
        GAZER_WARN << "LookToScroll pin cursor:" << err;
    }
}

QPoint LookToScroll::originPoint() const
{
    return m_hasOrigin ? m_origin : QCursor::pos();
}

void LookToScroll::toggle()
{
    setEnabled(!m_enabled);
}

void LookToScroll::nudgeMaxSpeed(int dir)
{
    const double next = nudgeLtsSpeed(m_maxNotchesPerSec, dir);
    if (qAbs(next - m_maxNotchesPerSec) < 0.001) {
        return;
    }
    m_maxNotchesPerSec = next;
    GAZER_INFO << "LookToScroll speed" << m_maxNotchesPerSec;
    emit maxNotchesPerSecChanged(m_maxNotchesPerSec);
    if (m_plusOpen) {
        pushPlusOverlay(m_plusLayout, m_plusBand, m_plusSlice, m_plusDwell.progress());
    }
}

void LookToScroll::setScrollMode(LtsScrollMode mode)
{
    mode = ltsScrollModeFromInt(int(mode));
    if (m_scrollMode == mode) {
        return;
    }
    m_scrollMode = mode;
    GAZER_INFO << "LookToScroll mode" << ltsScrollModeName(m_scrollMode);
    emit scrollModeChanged(m_scrollMode);
    if (m_plusOpen) {
        pushPlusOverlay(m_plusLayout, m_plusBand, m_plusSlice, m_plusDwell.progress());
    }
}

void LookToScroll::cycleScrollMode()
{
    setScrollMode(cycleLtsScrollMode(m_scrollMode));
}

void LookToScroll::resumeScroll()
{
    if (!m_enabled || !m_scrollSuspended) {
        return;
    }
    setScrollSuspended(false);
}

void LookToScroll::requestReset()
{
    if (!m_enabled) {
        return;
    }
    m_replacing = true;
    closePlus();
    setScrollSuspended(true);
    hideOverlay();
    emit placeScrollPointRequested();
}

void LookToScroll::cancelOriginPlace()
{
    if (!m_enabled || !m_replacing) {
        return;
    }
    m_replacing = false;
    if (m_scrollSuspended) {
        setScrollSuspended(false);
    }
}

void LookToScroll::pauseAtHub()
{
    m_replacing = false;
    if (!m_scrollSuspended) {
        setScrollSuspended(true);
    }
    openPlus();
}

void LookToScroll::setScrollSuspended(bool suspended)
{
    if (m_scrollSuspended == suspended) {
        return;
    }
    m_scrollSuspended = suspended;
    m_outsideSec = 0.0;
    m_centerProgress = 0.0;
    m_scrollEngaged = false;
    m_invalidGrace.reset();
    m_scroller.lift();
    if (!suspended) {
        m_replacing = false;
        closePlus();
        hidePlus();
        hideOverlay();
    }
    GAZER_INFO << "LookToScroll scroll" << (suspended ? "SUSPENDED" : "resumed");
    emit scrollSuspendedChanged(m_scrollSuspended);
}

void LookToScroll::setDeadzonePx(int px)
{
    m_deadzonePx = qMax(20, px);
}

void LookToScroll::setFalloffPx(int px)
{
    m_falloffPx = qMax(40, px);
}

void LookToScroll::setMaxNotchesPerSec(double n)
{
    m_maxNotchesPerSec = snapLtsSpeed(n);
    if (m_plusOpen) {
        pushPlusOverlay(m_plusLayout, m_plusBand, m_plusSlice, m_plusDwell.progress());
    }
}

void LookToScroll::setAccent(const QColor& c)
{
    m_accent = c;
    if (m_overlay) {
        m_overlay->setAccent(c);
    }
}

void LookToScroll::setRadii(int innerPx, int sharedPx, int outerPx)
{
    double inner = double(innerPx);
    double shared = double(sharedPx);
    double outer = double(outerPx);
    ComboMouseHit::clampRadii(inner, shared, outer);
    m_innerPx = inner;
    m_sharedPx = shared;
    m_outerPx = outer;
    if (m_plusOpen) {
        pushPlusOverlay(plusLayout(), m_plusBand, m_plusSlice, m_plusDwell.progress());
    }
}

void LookToScroll::setAnnulusColors(const QColor& inner, const QColor& outer)
{
    if (inner.isValid()) {
        m_innerColor = inner;
    }
    if (outer.isValid()) {
        m_outerColor = outer;
    }
    if (m_plusOpen) {
        pushPlusOverlay(m_plusLayout, m_plusBand, m_plusSlice, m_plusDwell.progress());
    }
}

void LookToScroll::setScanGraceMs(int ms)
{
    m_plusDwell.setScanGraceMs(ms);
}

void LookToScroll::setDwellGraceMs(int ms)
{
    m_plusDwell.setInvalidGraceMs(ms);
    m_invalidGrace.graceMs = ms;
}

void LookToScroll::setDwellMs(int ms)
{
    m_plusDwell.setDwellMs(ms);
}

void LookToScroll::setDwellSequence(const QVector<int>& ms)
{
    m_plusDwell.setDwellSequence(ms);
}

void LookToScroll::setAccelPerSec(double a)
{
    m_accelPerSec = qBound(0.0, a, 2.0);
}

void LookToScroll::setCenterDwellMs(int ms)
{
    m_centerDwellMs = qMax(200, ms);
}

void LookToScroll::setActiveWhenOverBoard(bool allow)
{
    m_allowOverBoard = allow;
}

void LookToScroll::setIndicatorStyle(LtsIndicator style)
{
    m_indicatorStyle = style;
}

void LookToScroll::hideOverlay()
{
    if (m_overlay) {
        m_overlay->hide();
    }
}

void LookToScroll::hidePlus()
{
    m_plusDwell.leave();
    if (m_plus) {
        m_plus->hide();
    }
}

void LookToScroll::showPausedHub()
{
    if (!m_overlay) {
        return;
    }
    m_overlay->setState(m_deadzonePx, m_falloffPx, 0.0, 0.0, 0.0, 0.0, m_indicatorStyle,
                        hubVisualRadiusPx(), true);
    m_overlay->placeCenter(originPoint());
}

void LookToScroll::openPlus()
{
    m_lookAwaySec = 0.0;
    hideOverlay();
    m_plusOpen = true;
    m_plusDwell.leave();
    const ComboMouseHit::Layout L = plusLayout();
    m_plusLayout = L;
    m_plusBand = ComboMouseHit::Band::Deadzone;
    m_plusSlice = ComboMouseHit::Slice::Right;
    pushPlusOverlay(L, m_plusBand, m_plusSlice, 0.0);
}

void LookToScroll::closePlus()
{
    if (!m_plusOpen) {
        return;
    }
    m_plusOpen = false;
    m_lookAwaySec = 0.0;
    hidePlus();
}

QRectF LookToScroll::plusScreenRect() const
{
    return QRectF(overlayScreenGeometry());
}

ComboMouseHit::Layout LookToScroll::plusLayout() const
{
    return makeLtsLayout(QPointF(originPoint()), plusScreenRect(), m_innerPx, m_sharedPx, m_outerPx);
}

void LookToScroll::pushPlusOverlay(const ComboMouseHit::Layout& L, ComboMouseHit::Band band,
                                   ComboMouseHit::Slice slice, double dwellProg)
{
    if (!m_plus || !m_plusOpen) {
        return;
    }
    m_plusLayout = L;
    m_plusBand = band;
    m_plusSlice = slice;
    PieOverlay::Appearance a;
    a.layout = L;
    a.band = band;
    a.slice = slice;
    a.dwellProg = dwellProg;
    a.accent = m_accent;
    a.theme = m_theme;
    a.innerColor = m_innerColor;
    a.outerColor = m_outerColor;
    fillLtsSliceIcons(m_scrollMode, a.sliceIcons);
    a.hubLabel = QString::number(int(qRound(m_maxNotchesPerSec)));
    m_plus->setAppearance(a);
    m_plus->place(originPoint(), plusScreenRect());
}

QString LookToScroll::plusHitId(ComboMouseHit::Band band, ComboMouseHit::Slice slice)
{
    return QLatin1String(ltsMenuActionId(ltsMenuActionFromHit(band, slice)));
}

void LookToScroll::firePlusAction(LtsMenuAction action)
{
    switch (action) {
    case LtsMenuAction::Resume:
        resumeScroll();
        break;
    case LtsMenuAction::Faster:
        nudgeMaxSpeed(+1);
        break;
    case LtsMenuAction::Slower:
        nudgeMaxSpeed(-1);
        break;
    case LtsMenuAction::Reset:
        requestReset();
        break;
    case LtsMenuAction::Quit:
        setEnabled(false);
        break;
    case LtsMenuAction::CycleMode:
        cycleScrollMode();
        break;
    case LtsMenuAction::None:
        break;
    }
}

bool LookToScroll::containsGaze(const GazePoint& point) const
{
    if (!m_plusOpen || !point.valid) {
        return false;
    }
    const auto h = ComboMouseHit::hit(point.toPointF(), QPointF(originPoint()), plusLayout());
    return h.band != ComboMouseHit::Band::None;
}

void LookToScroll::updateOverlay(const QPoint& center, double gazeDist, double dirX, double dirY,
                                 bool active, double centerProg)
{
    if (!m_overlay) {
        return;
    }
    double activity = 0.0;
    if (active && !m_scrollSuspended && gazeDist > m_deadzonePx) {
        const double t = qBound(0.0, (gazeDist - m_deadzonePx) / double(m_falloffPx), 1.0);
        activity = easeLtsFalloff(t);
    }
    m_overlay->setState(m_deadzonePx, m_falloffPx, activity, centerProg, dirX, dirY,
                        m_indicatorStyle, hubVisualRadiusPx());
    m_overlay->placeCenter(center);
}

double LookToScroll::screenHeightPx() const
{
    QScreen* s = QGuiApplication::screenAt(originPoint());
    if (!s) {
        s = QGuiApplication::primaryScreen();
    }
    return s ? double(s->geometry().height()) : 1080.0;
}

void LookToScroll::updatePausedMenu(const GazePoint& point)
{
    m_scroller.lift();

    const qint64 now = m_clock.elapsed();
    const double sampleDt =
        m_lastSampleMs < 0 ? 0.016
                           : qBound(0.004, (now - m_lastSampleMs) / 1000.0, 0.05);
    m_lastSampleMs = now;

    GazePoint gp = point;
    if (gp.timestampMs <= 0) {
        gp.timestampMs = now;
    }

    if (m_plusOpen) {
        hideOverlay();
        const ComboMouseHit::Layout L = plusLayout();
        if (containsGaze(point)) {
            m_lookAwaySec = 0.0;
            pinCursorToOrigin();
            const auto h = ComboMouseHit::hit(gp.toPointF(), QPointF(originPoint()), L);
            const QString id = plusHitId(h.band, h.slice);
            m_plusDwell.onGazeSample(gp, id);
            if (!m_plusOpen) {
                return;
            }
            pushPlusOverlay(L, h.band, h.slice, m_plusDwell.progress());
            return;
        }
        m_plusDwell.leave();
        m_lookAwaySec += sampleDt;
        if (m_lookAwaySec < kLtsPlusDismissGraceSec) {
            pushPlusOverlay(L, ComboMouseHit::Band::None, ComboMouseHit::Slice::Right, 0.0);
            return;
        }
        closePlus();
    }

    m_lookAwaySec = 0.0;
    if (point.valid) {
        const QPointF delta = point.toPointF() - QPointF(originPoint());
        const double dist = qSqrt(delta.x() * delta.x() + delta.y() * delta.y());
        if (dist <= hubDwellRadiusPx()) {
            openPlus();
            return;
        }
    }
    showPausedHub();
}

double LookToScroll::hubVisualRadiusPx() const
{
    return screenHeightPx() * (kLtsHubVisualDiameterFrac * 0.5);
}

double LookToScroll::hubDwellRadiusPx() const
{
    return screenHeightPx() * (kLtsHubDwellDiameterFrac * 0.5);
}

void LookToScroll::onGaze(const GazePoint& point, bool pauseInput)
{
    if (!m_enabled) {
        m_scroller.lift();
        hideOverlay();
        return;
    }
    if (m_replacing) {
        m_scrollEngaged = false;
        m_scroller.lift();
        hideOverlay();
        return;
    }
    if (m_scrollSuspended) {
        updatePausedMenu(point);
        return;
    }

    const qint64 now = m_clock.elapsed();
    const qint64 sampleTs = point.timestampMs > 0 ? point.timestampMs : now;

    if (!point.valid) {
        if (m_invalidGrace.onInvalid(sampleTs) == InvalidGazeGrace::Result::Holding) {
            return;
        }
        m_scrollEngaged = false;
        m_scroller.lift();
        hideOverlay();
        return;
    }
    m_invalidGrace.onValid();

    if (pauseInput && !m_allowOverBoard) {
        m_scrollEngaged = false;
        m_scroller.lift();
        hideOverlay();
        m_centerProgress = 0.0;
        m_outsideSec = 0.0;
        return;
    }

    const QPoint origin = originPoint();
    pinCursorToOrigin();
    const QPointF gaze(point.x, point.y);
    const QPointF delta = gaze - QPointF(origin);
    const double dist = qSqrt(delta.x() * delta.x() + delta.y() * delta.y());
    const double dirX = dist > 1.0 ? delta.x() / dist : 0.0;
    const double dirY = dist > 1.0 ? delta.y() / dist : 0.0;
    const double hubR = hubDwellRadiusPx();

    const double sampleDt =
        m_lastSampleMs < 0 ? 0.016
                           : qBound(0.004, (now - m_lastSampleMs) / 1000.0, 0.05);
    m_lastSampleMs = now;

    if (dist <= hubR) {
        m_scrollEngaged = false;
        m_scroller.lift();
        m_outsideSec = 0.0;
        m_centerProgress =
            qBound(0.0, m_centerProgress + sampleDt * 1000.0 / double(m_centerDwellMs), 1.0);
        updateOverlay(origin, dist, dirX, dirY, false, m_centerProgress);
        if (m_centerProgress >= 1.0) {
            m_centerProgress = 0.0;
            pauseAtHub();
        }
        return;
    }

    m_centerProgress = qMax(0.0, m_centerProgress - sampleDt * 2.5);
    if (m_centerProgress <= 0.01) {
        m_centerProgress = 0.0;
    }

    m_scrollEngaged = ltsKeepScrolling(m_scrollEngaged, dist, m_deadzonePx) && dist >= 1.0;
    if (!m_scrollEngaged) {
        m_scroller.lift();
        m_outsideSec = 0.0;
        updateOverlay(origin, dist, dirX, dirY, false, m_centerProgress);
        return;
    }

    updateOverlay(origin, dist, dirX, dirY, true, m_centerProgress);

    if (m_lastTickMs >= 0 && (now - m_lastTickMs) < m_intervalMs) {
        return;
    }
    const double tickDt =
        m_lastTickMs < 0 ? (m_intervalMs / 1000.0)
                         : qBound(0.008, (now - m_lastTickMs) / 1000.0, 0.08);
    m_lastTickMs = now;
    if (dist > m_deadzonePx) {
        m_outsideSec += tickDt;
    }

    const double tLin = qBound(0.0, (dist - m_deadzonePx) / double(m_falloffPx), 1.0);
    const double t = easeLtsFalloff(tLin);
    const double accel = qMin(m_accelMax, 1.0 + m_accelPerSec * m_outsideSec);

    const double nx = delta.x() / dist;
    const double ny = delta.y() / dist;
    double rate = m_maxNotchesPerSec * PixelScroller::kPixelsPerNotch * t * accel;
    if (rate < kLtsMinEngagedPxPerSec) {
        rate = kLtsMinEngagedPxPerSec;
    }
    double dv = (-ny) * rate * tickDt;
    double dh = (nx)*rate * tickDt;
    applyLtsScrollMode(m_scrollMode, dv, dh);

    if (qAbs(dv) < 1e-6 && qAbs(dh) < 1e-6) {
        return;
    }

    QString err;
    if (m_scroller.scrollBy(dh, dv, &err)) {
        emit scrolled(int(dv > 0 ? qFloor(dv) : qCeil(dv)),
                      int(dh > 0 ? qFloor(dh) : qCeil(dh)));
    } else if (!err.isEmpty()) {
        GAZER_WARN << "LookToScroll:" << err;
    }
}

} // namespace gazer
