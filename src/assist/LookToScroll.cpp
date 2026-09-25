#include "assist/LookToScroll.h"

#include "assist/LookToOverlay.h"
#include "assist/PieOverlay.h"
#include "input/InputService.h"
#include "input/MouseInjector.h"
#include "ui/Theme.h"
#include "utils/Log.h"
#include "utils/ScreenGrab.h"

#include <QColor>
#include <QCursor>
#include <QGuiApplication>
#include <QScreen>
#include <QtMath>

namespace gazer {

LookToScroll::LookToScroll(QObject* parent)
    : QObject(parent)
{
    m_clock.start();
    m_overlay = std::make_unique<LookToOverlay>();
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
    liftOutput();
    m_scroller.reset();
    hidePlus();
    hideOverlay();
}

void LookToScroll::setDest(LookToDest dest)
{
    dest = lookToDestFromInt(int(dest));
    if (m_dest == dest) {
        return;
    }
    liftOutput();
    m_dest = dest;
    clampLookToMapSettings(m_dest, m_cfg);
}

LookToMapSettings LookToScroll::config() const
{
    return m_cfg;
}

void LookToScroll::setConfig(const LookToMapSettings& cfg)
{
    m_cfg = cfg;
    clampLookToMapSettings(m_dest, m_cfg);
    if (!m_cfg.hubEnabled && m_scrollSuspended) {
        setScrollSuspended(false);
        return;
    }
    if (m_plusOpen) {
        if (!m_cfg.hubEnabled) {
            closePlus();
            hidePlus();
        } else {
            pushPlusOverlay(m_plusLayout, m_plusBand, m_plusSlice, m_plusDwell.progress());
        }
    }
}

void LookToScroll::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    m_lastSampleMs = -1;
    m_accelSecV = 0.0;
    m_accelSecH = 0.0;
    m_centerProgress = 0.0;
    m_replacing = false;
    m_scrollSuspended = false;
    m_scrollEngaged = false;
    m_plusOpen = false;
    m_lookAwaySec = 0.0;
    m_plusDwell.leave();
    m_invalidGrace.reset();
    liftOutput();
    m_scroller.reset();
    if (!m_enabled) {
        hidePlus();
        hideOverlay();
    } else if (!m_hasOrigin) {
        setScrollOrigin(QCursor::pos());
    } else if (m_pinCursor) {
        pinCursorToOrigin();
    }
    GAZER_INFO << lookToDestLabel(m_dest) << (m_enabled ? "ON" : "OFF");
    emit enabledChanged(m_enabled);
    emit scrollSuspendedChanged(false);
}

void LookToScroll::setScrollOrigin(const QPoint& pos)
{
    m_origin = pos;
    m_hasOrigin = true;
    // A new origin must not resume a half-finished hub dwell or scroll stream.
    m_scrollEngaged = false;
    m_centerProgress = 0.0;
    m_accelSecV = 0.0;
    m_accelSecH = 0.0;
    m_scroller.reset();
    if (m_pinCursor) {
        pinCursorToOrigin();
    }
}

void LookToScroll::pinCursorToOrigin()
{
    if (!m_pinCursor || !m_hasOrigin) {
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
        GAZER_WARN << lookToDestLabel(m_dest) << "pin cursor:" << err;
    }
}

QPoint LookToScroll::originPoint() const
{
    if (m_hasOrigin) {
        return m_origin;
    }
    return QCursor::pos();
}

void LookToScroll::toggle()
{
    setEnabled(!m_enabled);
}

void LookToScroll::nudgeMaxSpeed(int dir)
{
    const double next = nudgeLookToSpeed(m_dest, m_cfg.maxSpeed, dir);
    if (qAbs(next - m_cfg.maxSpeed) < 0.001) {
        return;
    }
    m_cfg.maxSpeed = next;
    GAZER_INFO << lookToDestLabel(m_dest) << "speed" << m_cfg.maxSpeed;
    emit maxNotchesPerSecChanged(m_cfg.maxSpeed);
    if (m_plusOpen) {
        pushPlusOverlay(m_plusLayout, m_plusBand, m_plusSlice, m_plusDwell.progress());
    }
}

void LookToScroll::setScrollMode(LtsScrollMode mode)
{
    mode = ltsScrollModeFromInt(int(mode));
    if (m_cfg.axisMode == mode) {
        return;
    }
    m_cfg.axisMode = mode;
    GAZER_INFO << lookToDestLabel(m_dest) << "mode" << ltsScrollModeName(m_cfg.axisMode);
    emit scrollModeChanged(m_cfg.axisMode);
    if (m_plusOpen) {
        pushPlusOverlay(m_plusLayout, m_plusBand, m_plusSlice, m_plusDwell.progress());
    }
}

void LookToScroll::cycleScrollMode()
{
    setScrollMode(cycleLtsScrollMode(m_cfg.axisMode));
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
    m_accelSecV = 0.0;
    m_accelSecH = 0.0;
    m_centerProgress = 0.0;
    m_scrollEngaged = false;
    m_invalidGrace.reset();
    liftOutput();
    if (!suspended) {
        m_replacing = false;
        closePlus();
        hidePlus();
        hideOverlay();
    }
    GAZER_INFO << lookToDestLabel(m_dest) << (suspended ? "SUSPENDED" : "resumed");
    emit scrollSuspendedChanged(m_scrollSuspended);
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

void LookToScroll::setDwellSequence(const QVector<int>& ms)
{
    m_plusDwell.setDwellSequence(ms);
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
    m_overlay->setState(m_cfg, {}, 0.0, 0.0, hubVisualRadiusPx(), true);
    m_overlay->placeCenter(originPoint());
}

void LookToScroll::openPlus()
{
    if (!m_cfg.hubEnabled) {
        return;
    }
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

QString LookToScroll::hubSpeedLabel() const
{
    return lookToSpeedText(m_dest, m_cfg.maxSpeed);
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
    fillLtsSliceIcons(m_cfg.axisMode, a.sliceIcons);
    a.hubLabel = hubSpeedLabel();
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

void LookToScroll::updateOverlay(const QPoint& center, const QPointF& gaze, double gain,
                                 double centerProg, bool paused)
{
    if (!m_overlay) {
        return;
    }
    m_overlay->setState(m_cfg, gaze, gain, centerProg, hubVisualRadiusPx(), paused);
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
    liftOutput();

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
    if (point.valid && m_cfg.hubEnabled) {
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

void LookToScroll::liftOutput()
{
    m_scroller.lift();
    m_mouseRemX = 0.0;
    m_mouseRemY = 0.0;
    zeroJoystick();
}

void LookToScroll::zeroJoystick()
{
    if (!m_input) {
        m_driveJoyX = m_driveJoyY = false;
        return;
    }
    const char* xAxis = m_dest == LookToDest::RightStick ? "rx" : "lx";
    const char* yAxis = m_dest == LookToDest::RightStick ? "ry" : "ly";
    QString err;
    if (m_driveJoyX) {
        (void)m_input->gamepad().setAxis(QLatin1String(xAxis), 0.0, &err);
    }
    if (m_driveJoyY) {
        (void)m_input->gamepad().setAxis(QLatin1String(yAxis), 0.0, &err);
    }
    m_driveJoyX = m_driveJoyY = false;
}

void LookToScroll::warnJoystick(const QString& err)
{
    if (m_joyWarned) {
        return;
    }
    m_joyWarned = true;
    const QString msg =
        err.isEmpty() ? QStringLiteral("Virtual gamepad unavailable") : err;
    if (m_notify) {
        m_notify(msg);
    }
    GAZER_WARN << lookToDestLabel(m_dest) << msg;
}

void LookToScroll::applyOutput(double nx, double ny, double gain, double sampleDt)
{
    double vx = nx;
    double vy = ny;
    applyLtsScrollMode(m_cfg.axisMode, vy, vx);

    const double accelV = 1.0 + m_cfg.accelPerSec * m_accelSecV;
    const double accelH = 1.0 + m_cfg.accelPerSec * m_accelSecH;

    switch (m_dest) {
    case LookToDest::Scroll: {
        const double base = m_cfg.maxSpeed * PixelScroller::kPixelsPerNotch * gain;
        const double rateV = qMax(kLtsMinEngagedPxPerSec, base * accelV);
        const double rateH = qMax(kLtsMinEngagedPxPerSec, base * accelH);
        double dv = (-vy) * rateV * sampleDt;
        double dh = vx * rateH * sampleDt;
        applyLtsScrollMode(m_cfg.axisMode, dv, dh);
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
        break;
    }
    case LookToDest::Mouse: {
        const double rate = m_cfg.maxSpeed * gain;
        m_mouseRemX += vx * rate * accelH * sampleDt;
        m_mouseRemY += vy * rate * accelV * sampleDt;
        const int dx = int(m_mouseRemX);
        const int dy = int(m_mouseRemY);
        if (dx == 0 && dy == 0) {
            return;
        }
        m_mouseRemX -= double(dx);
        m_mouseRemY -= double(dy);
        QString err;
        (void)MouseInjector::moveBy(dx, dy, &err);
        break;
    }
    case LookToDest::LeftStick:
    case LookToDest::RightStick: {
        if (!m_input) {
            return;
        }
        const double scale = qBound(0.0, m_cfg.maxSpeed, 1.0) * gain;
        double lx = vx * scale * accelH;
        double ly = -vy * scale * accelV;
        applyLtsScrollMode(m_cfg.axisMode, ly, lx);
        lx = qBound(-1.0, lx, 1.0);
        ly = qBound(-1.0, ly, 1.0);
        const char* xAxis = m_dest == LookToDest::RightStick ? "rx" : "lx";
        const char* yAxis = m_dest == LookToDest::RightStick ? "ry" : "ly";
        QString err;
        if (!m_input->gamepad().setAxis(QLatin1String(xAxis), lx, &err)) {
            warnJoystick(err);
        }
        if (!m_input->gamepad().setAxis(QLatin1String(yAxis), ly, &err)) {
            warnJoystick(err);
        }
        m_driveJoyX = true;
        m_driveJoyY = true;
        break;
    }
    }
}

void LookToScroll::onGaze(const GazePoint& point, bool pauseInput)
{
    if (!m_enabled) {
        liftOutput();
        hideOverlay();
        return;
    }
    if (m_replacing) {
        m_scrollEngaged = false;
        liftOutput();
        hideOverlay();
        return;
    }
    if (m_scrollSuspended) {
        updatePausedMenu(point);
        return;
    }

    const qint64 now = m_clock.elapsed();
    const qint64 sampleTs = point.timestampMs > 0 ? point.timestampMs : now;
    if (pauseInput || !point.valid) {
        pinCursorToOrigin();
        if (m_invalidGrace.onInvalid(sampleTs) == InvalidGazeGrace::Result::Holding) {
            return;
        }
        m_scrollEngaged = false;
        liftOutput();
        hideOverlay();
        m_centerProgress = 0.0;
        return;
    }
    m_invalidGrace.onValid();

    const QPoint origin = originPoint();
    pinCursorToOrigin();
    const QPointF gaze(point.x, point.y);
    const QPointF delta = gaze - QPointF(origin);
    const double dist = qSqrt(delta.x() * delta.x() + delta.y() * delta.y());
    const double dirX = dist > 1.0 ? delta.x() / dist : 0.0;
    const double dirY = dist > 1.0 ? delta.y() / dist : 0.0;
    const double hubR = hubDwellRadiusPx();
    const bool inHub = m_cfg.hubEnabled && dist <= hubR;
    const bool vActive =
        m_cfg.axisMode != LtsScrollMode::Horizontal && ltsAxisAccelActive(delta.y(), m_cfg.deadzonePx);
    const bool hActive =
        m_cfg.axisMode != LtsScrollMode::Vertical && ltsAxisAccelActive(delta.x(), m_cfg.deadzonePx);

    const double sampleDt =
        m_lastSampleMs < 0 ? 0.016
                           : qBound(0.004, (now - m_lastSampleMs) / 1000.0, 0.05);
    m_lastSampleMs = now;

    m_scrollEngaged =
        !inHub && lookToKeepEngaged(m_scrollEngaged, dist, m_cfg) && dist >= 1.0;
    const double gain = m_scrollEngaged ? lookToGain(dist, m_cfg) : 0.0;
    const bool grow = m_scrollEngaged && gain > 0.0;
    stepLtsAxisAccelSec(m_accelSecV, vActive, grow, grow ? sampleDt : 0.0);
    stepLtsAxisAccelSec(m_accelSecH, hActive, grow, grow ? sampleDt : 0.0);

    if (inHub) {
        liftOutput();
        m_centerProgress =
            qBound(0.0, m_centerProgress + sampleDt * 1000.0 / double(m_cfg.centerDwellMs), 1.0);
        updateOverlay(origin, delta, 0.0, m_centerProgress, false);
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

    // Engaged includes the inner hysteresis band where gain is 0; scroll still
    // streams at kLtsMinEngagedPxPerSec so PixelScroller leftovers do not reset.
    if (!m_scrollEngaged) {
        liftOutput();
        updateOverlay(origin, delta, 0.0, m_centerProgress, false);
        return;
    }

    updateOverlay(origin, delta, gain, m_centerProgress, false);
    applyOutput(dirX, dirY, gain, sampleDt);
}

} // namespace gazer
