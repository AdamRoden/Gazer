#include "assist/LookToMaps.h"

#include "assist/LookToOverlay.h"
#include "utils/Log.h"
#include "utils/ScreenGrab.h"

#include <QGuiApplication>
#include <QScreen>
#include <QtMath>

namespace gazer {

namespace {

double hubVisualRadiusAt(QPoint origin)
{
    QScreen* s = QGuiApplication::screenAt(origin);
    if (!s) {
        s = QGuiApplication::primaryScreen();
    }
    const double h = s ? double(s->geometry().height()) : 1080.0;
    return h * (kLtsHubVisualDiameterFrac * 0.5);
}

} // namespace

LookToMaps::LookToMaps(QObject* parent)
    : QObject(parent)
{
    for (int i = 0; i < kLookToDestCount; ++i) {
        const auto dest = LookToDest(i);
        m_maps[i] = std::make_unique<LookToScroll>();
        m_maps[i]->setDest(dest);
        m_maps[i]->setConfig(defaultLookToMapSettings(dest));
        connectMap(dest);
    }
    m_previewHud = std::make_unique<LookToOverlay>(LookToOverlay::Kind::Preview);
}

LookToMaps::~LookToMaps() = default;

LookToScroll& LookToMaps::map(LookToDest dest)
{
    return *m_maps[int(lookToDestFromInt(int(dest)))];
}

const LookToScroll& LookToMaps::map(LookToDest dest) const
{
    return *m_maps[int(lookToDestFromInt(int(dest)))];
}

void LookToMaps::connectMap(LookToDest dest)
{
    LookToScroll* m = m_maps[int(dest)].get();
    connect(m, &LookToScroll::enabledChanged, this, [this, dest](bool on) {
        updatePinCursor();
        emit enabledChanged(dest, on);
    });
    connect(m, &LookToScroll::scrollSuspendedChanged, this,
            [this, dest](bool on) { emit outputSuspendedChanged(dest, on); });
    connect(m, &LookToScroll::maxNotchesPerSecChanged, this,
            [this, dest](double v) { emit maxSpeedChanged(dest, v); });
    connect(m, &LookToScroll::scrollModeChanged, this,
            [this, dest](LtsScrollMode mode) { emit axisModeChanged(dest, mode); });
    connect(m, &LookToScroll::placeScrollPointRequested, this, [this, dest]() {
        m_placing = true;
        m_placingDest = dest;
        emit placeOriginRequested(dest);
    });
}

void LookToMaps::setInput(InputService* input)
{
    forEachMap([input](LookToScroll& m) { m.setInput(input); });
}

void LookToMaps::setNotifyFn(LookToScroll::NotifyFn fn)
{
    forEachMap([&fn](LookToScroll& m) { m.setNotifyFn(fn); });
}

void LookToMaps::setAccent(const QColor& c)
{
    m_accent = c;
    forEachMap([&c](LookToScroll& m) { m.setAccent(c); });
    if (m_previewHud) {
        m_previewHud->setAccent(c);
    }
}

void LookToMaps::setTheme(const ThemeColors& theme)
{
    forEachMap([&theme](LookToScroll& m) { m.setTheme(theme); });
}

void LookToMaps::setPieRadii(int innerPx, int sharedPx, int outerPx)
{
    forEachMap([innerPx, sharedPx, outerPx](LookToScroll& m) {
        m.setRadii(innerPx, sharedPx, outerPx);
    });
}

void LookToMaps::setAnnulusColors(const QColor& inner, const QColor& outer)
{
    forEachMap([&inner, &outer](LookToScroll& m) { m.setAnnulusColors(inner, outer); });
}

void LookToMaps::setScanGraceMs(int ms)
{
    forEachMap([ms](LookToScroll& m) { m.setScanGraceMs(ms); });
}

void LookToMaps::setDwellGraceMs(int ms)
{
    forEachMap([ms](LookToScroll& m) { m.setDwellGraceMs(ms); });
}

void LookToMaps::setDwellSequence(const QVector<int>& ms)
{
    forEachMap([&ms](LookToScroll& m) { m.setDwellSequence(ms); });
}

void LookToMaps::applyConfig(LookToDest dest, const LookToMapSettings& cfg)
{
    map(dest).setConfig(cfg);
    if (m_preview && dest == m_previewDest) {
        updatePreviewHud();
    }
}

void LookToMaps::setPreview(LookToDest dest, bool on)
{
    m_preview = on;
    m_previewDest = dest;
    if (!on) {
        m_highlight = LookToRing::None;
        if (m_previewHud) {
            m_previewHud->setHighlight(LookToRing::None);
            m_previewHud->hide();
        }
        return;
    }
    updatePreviewHud();
}

void LookToMaps::clearPreview()
{
    setPreview(m_previewDest, false);
}

void LookToMaps::setHighlightRing(LookToDest dest, LookToRing ring)
{
    if (dest != m_previewDest) {
        return;
    }
    m_highlight = ring;
    if (m_preview) {
        updatePreviewHud();
    }
}

void LookToMaps::updatePreviewHud()
{
    if (!m_previewHud) {
        return;
    }
    if (!m_preview) {
        m_previewHud->hide();
        return;
    }
    const LookToScroll& m = map(m_previewDest);
    const QPoint origin =
        m.hasScrollOrigin() ? m.scrollOrigin() : overlayScreenGeometry().center();
    QPointF gazeOff;
    double gain = 0.0;
    if (m_lastGaze.valid) {
        gazeOff = m_lastGaze.toPointF() - QPointF(origin);
        const double dist = qSqrt(gazeOff.x() * gazeOff.x() + gazeOff.y() * gazeOff.y());
        gain = lookToGain(dist, m.config());
    }
    m_previewHud->setAccent(m_accent);
    m_previewHud->setHighlight(m_highlight);
    m_previewHud->setState(m.config(), gazeOff, gain, 0.0, hubVisualRadiusAt(origin), false);
    m_previewHud->placeCenter(origin);
}

void LookToMaps::beginPlace(LookToDest dest)
{
    m_placing = true;
    m_placingDest = dest;
}

void LookToMaps::onPlaced(QPoint pos)
{
    if (!m_placing) {
        return;
    }
    LookToScroll& m = map(m_placingDest);
    m.setScrollOrigin(pos);
    if (!m.isEnabled()) {
        m.setEnabled(true);
    } else {
        m.setScrollSuspended(false);
    }
    m_placing = false;
    updatePinCursor();
    if (m_preview && m_placingDest == m_previewDest) {
        updatePreviewHud();
    }
}

void LookToMaps::cancelPlace()
{
    if (!m_placing) {
        return;
    }
    map(m_placingDest).cancelOriginPlace();
    m_placing = false;
}

void LookToMaps::disableAll()
{
    cancelPlace();
    m_idlePause.invalidate();
    forEachMap([](LookToScroll& m) { m.setEnabled(false); });
}

void LookToMaps::setOutputPaused(bool on)
{
    m_outputPaused = on;
    if (on) {
        m_idlePause.invalidate();
    }
}

bool LookToMaps::anyEnabled() const
{
    for (int i = 0; i < kLookToDestCount; ++i) {
        if (m_maps[i]->isEnabled()) {
            return true;
        }
    }
    return false;
}

bool LookToMaps::containsGaze(const GazePoint& point) const
{
    for (int i = 0; i < kLookToDestCount; ++i) {
        if (m_maps[i]->containsGaze(point)) {
            return true;
        }
    }
    return false;
}

bool LookToMaps::anyPieOpen() const
{
    for (int i = 0; i < kLookToDestCount; ++i) {
        if (m_maps[i]->isPieOpen()) {
            return true;
        }
    }
    return false;
}

void LookToMaps::updatePinCursor()
{
    const bool pin = map(LookToDest::Scroll).isEnabled() && !map(LookToDest::Mouse).isEnabled();
    map(LookToDest::Scroll).setPinCursorEnabled(pin);
}

void LookToMaps::onGaze(const GazePoint& point, bool pauseInput)
{
    m_lastGaze = point;
    updatePinCursor();
    const bool overPie = containsGaze(point);
    // Place-cursor counts as paused. A clock left running while every map is
    // off is already past 90s on the sample after the next place.
    if (!(anyEnabled() && pauseInput && !overPie && !m_placing)) {
        m_idlePause.invalidate();
    } else if (!m_idlePause.isValid()) {
        m_idlePause.start();
    } else if (m_idlePause.elapsed() >= 90000) {
        GAZER_INFO << "Look-to idle timeout — disabling maps";
        disableAll();
    }
    for (int i = 0; i < kLookToDestCount; ++i) {
        LookToScroll* m = m_maps[i].get();
        bool pause = pauseInput || m_outputPaused;
        if (overPie && !m->isPieOpen()) {
            pause = true;
        }
        if (overPie && m->isPieOpen() && !m_outputPaused) {
            pause = false;
        }
        m->onGaze(point, pause);
    }
    if (m_preview) {
        updatePreviewHud();
    }
}

} // namespace gazer
