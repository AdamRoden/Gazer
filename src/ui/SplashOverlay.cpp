#include "ui/SplashOverlay.h"

#include "layout/ChromeBlur.h"
#include "layout/PageSession.h"
#include "layout/PageTypes.h"
#include "ui/AppIcon.h"
#include "ui/SplashOverlay_p.h"
#include "utils/ScreenGrab.h"

#include <QDir>
#include <QEasingCurve>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QScreen>
#include <QVector>
#include <QtMath>

namespace gazer {

namespace {

QPixmap captureDesktopBlur(double radius)
{
    const QRect desk = virtualDesktop();
    if (desk.isEmpty()) {
        return {};
    }
    QPixmap canvas(desk.size());
    canvas.fill(QColor(16, 18, 22));
    canvas.setDevicePixelRatio(1.0);
    {
        QPainter p(&canvas);
        for (QScreen* s : QGuiApplication::screens()) {
            if (!s) {
                continue;
            }
            const QRect g = s->geometry();
            const QPixmap piece = grabScreenRect(s, g);
            if (!piece.isNull()) {
                p.drawPixmap(g.topLeft() - desk.topLeft(), piece);
            }
        }
    }
    return downscaleBlur(canvas, qBound(1.0, radius, kChromeBlurMax));
}

QPixmap capturePrimarySharp()
{
    QScreen* s = QGuiApplication::primaryScreen();
    return s ? grabScreenRect(s, s->geometry()) : QPixmap{};
}

double ease(double t)
{
    QEasingCurve c(QEasingCurve::InOutCubic);
    return c.valueForProgress(qBound(0.0, t, 1.0));
}

QPointF lerp(const QPointF& a, const QPointF& b, double t)
{
    return QPointF(a.x() + (b.x() - a.x()) * t, a.y() + (b.y() - a.y()) * t);
}

QPointF rectCenter(const QRect& r, const QPointF& fallback)
{
    return (r.width() < 8 || r.height() < 8) ? fallback : QRectF(r).center();
}

double scaleValue(splash::Scale s, double fitted)
{
    return s == splash::Scale::One ? 1.0 : fitted;
}

int g_splashRecordFrame = 0;

void resetSplashRecordFrame()
{
    g_splashRecordFrame = 0;
}

void dumpSplashRecordFrame(SplashOverlay* w)
{
    const QString dir = qEnvironmentVariable("GAZER_SPLASH_RECORD");
    if (dir.isEmpty() || !w) {
        return;
    }
    QDir().mkpath(dir);
    QImage img(w->size(), QImage::Format_RGB32);
    img.fill(Qt::black);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    w->render(&p);
    p.end();
    img.save(QStringLiteral("%1/f-%2.jpg").arg(dir).arg(g_splashRecordFrame, 5, 10, QChar('0')),
             "JPG", 82);
    ++g_splashRecordFrame;
}

} // namespace

SplashOverlay::SplashOverlay(QWidget* parent)
    : OverlaySurface(parent)
{
    setOverlayLayer(OverlayLayer::Assist);
    m_timer.setInterval(splash::kTickMs);
    connect(&m_timer, &QTimer::timeout, this, [this]() { tick(); });
    m_recapture.setSingleShot(true);
    connect(&m_recapture, &QTimer::timeout, this, [this]() {
        if (m_active) {
            captureFrost();
            update();
        }
    });
    m_navDwell.setDwellMs(splash::kNavDwellMs);
    m_navDwell.setStableRadiusPx(48);
    hide();
}

void SplashOverlay::setTheme(const ThemeColors& theme)
{
    m_theme = theme;
    if (m_active) {
        update();
    }
}

void SplashOverlay::setAccent(const QColor& c)
{
    if (c.isValid()) {
        m_accent = c;
        m_progress.progressColor = c;
    }
    if (m_active) {
        update();
    }
}

void SplashOverlay::setProgressVisuals(const ProgressVisuals& v)
{
    m_progress = v;
    if (m_accent.isValid()) {
        m_progress.progressColor = m_accent;
    }
    if (m_active) {
        update();
    }
}

void SplashOverlay::setSession(PageSession* pages)
{
    if (m_pages) {
        disconnect(m_pages, nullptr, this, nullptr);
    }
    m_pages = pages;
    if (m_pages) {
        connect(m_pages, &PageSession::sessionChanged, this, [this]() {
            if (m_active) {
                captureFrost();
                update();
            }
        });
    }
}

void SplashOverlay::start()
{
    if (m_active) {
        m_timer.stop();
        m_recapture.stop();
        setSuspended(false);
    }

    const QIcon icon = loadAppIcon();
    const qreal dpr = qMax(1.0, devicePixelRatioF());
    m_logo = icon.pixmap(QSize(qRound(220 * dpr), qRound(220 * dpr)));
    if (!m_logo.isNull()) {
        m_logo.setDevicePixelRatio(dpr);
    }

    refreshGeometry();
    captureFrost();
    m_active = true;
    m_navHit = NavChip::None;
    m_navDwell.reset();
    m_clock.restart();
    m_lastMs = 0;
    m_navLastMs = -1;
    resetSplashRecordFrame();
    enter(Phase::Welcome);
    m_gaze = gazePos();
    m_gazeValid = true;
    showOverlay();
    m_timer.start();
    emit activeChanged(true);
    update();
}

void SplashOverlay::skip()
{
    if (m_active && m_phase != Phase::Fade && m_phase != Phase::Done) {
        enter(Phase::Fade);
    }
}

void SplashOverlay::cancel()
{
    finish();
}

void SplashOverlay::onGaze(const GazePoint& point)
{
    if (!m_active) {
        return;
    }
    const qint64 now = m_clock.isValid() ? m_clock.elapsed() : 0;
    double dtSec = 0.016;
    if (m_navLastMs >= 0) {
        dtSec = qBound(0.004, (now - m_navLastMs) / 1000.0, 0.08);
    }
    m_navLastMs = now;
    if (!point.valid) {
        m_navHit = NavChip::None;
        m_navDwell.reset();
        m_navLastMs = -1;
        return;
    }
    NavChip hit = hitNav(QPoint(qRound(point.x), qRound(point.y)));
    if ((hit == NavChip::Prev && !prevEnabled()) || (hit == NavChip::Next && !nextEnabled())) {
        hit = NavChip::None;
    }
    if (hit != m_navHit) {
        m_navHit = hit;
        m_navDwell.reset();
    }
    if (hit == NavChip::None) {
        return;
    }
    if (!m_navDwell.sample(point.toPointF(), dtSec)) {
        return;
    }
    m_navDwell.reset();
    m_navHit = NavChip::None;
    if (hit == NavChip::Skip) {
        skip();
    } else if (hit == NavChip::Next) {
        goNext();
    } else {
        goPrev();
    }
}

void SplashOverlay::tick()
{
    if (!m_active) {
        return;
    }
    const qint64 now = m_clock.elapsed();
    int dt = int(now - m_lastMs);
    m_lastMs = now;
    dt = qBound(0, dt, 80);
    m_phaseElapsedMs += dt;

    const auto& s = splash::spec(int(m_phase));
    if (s.autoMs > 0 && m_phaseElapsedMs >= s.autoMs) {
        enter(Phase(int(m_phase) + 1));
    }
    m_gaze = gazePos();
    m_gazeValid = true;
    update();
    static qint64 lastBucket = -1;
    const qint64 bucket = m_clock.elapsed() / 50;
    if (bucket != lastBucket) {
        lastBucket = bucket;
        dumpSplashRecordFrame(this);
    }
}

void SplashOverlay::enter(Phase phase)
{
    if (phase == Phase::Done) {
        finish();
        return;
    }
    m_phase = phase;
    m_phaseElapsedMs = 0;
    const auto& s = splash::spec(int(m_phase));
    if (s.closeMenu) {
        setMasterLayers({1});
    }
    if (s.openMenu) {
        setMasterLayers({2});
    }
    if (s.suspend >= 0) {
        setSuspended(s.suspend == 1);
    }
    scheduleRecapture(s.recaptureMs);
}

void SplashOverlay::goNext()
{
    const auto& s = splash::spec(int(m_phase));
    if (s.introMs > 0 && m_phaseElapsedMs < s.introMs) {
        m_phaseElapsedMs = s.introMs;
        return;
    }
    if (m_phase < Phase::Fade) {
        enter(Phase(int(m_phase) + 1));
    }
}

void SplashOverlay::goPrev()
{
    if (m_phase == Phase::Welcome || m_phase == Phase::Done) {
        return;
    }
    for (int i = int(m_phase) - 1; i >= 0; --i) {
        if (splash::spec(i).autoMs == 0) {
            enter(Phase(i));
            return;
        }
    }
}

void SplashOverlay::finish()
{
    m_recapture.stop();
    setSuspended(false);
    m_timer.stop();
    const bool wasActive = m_active;
    m_active = false;
    m_phase = Phase::Done;
    m_navDwell.reset();
    hide();
    if (wasActive) {
        emit activeChanged(false);
        emit finished();
    }
}

void SplashOverlay::refreshGeometry()
{
    const QRect desk = virtualDesktop();
    if (desk.isValid()) {
        setGeometry(desk);
    }
}

void SplashOverlay::captureFrost()
{
    refreshGeometry();
    m_frost = captureDesktopBlur(splash::kBlurRadius);
    m_screenGrab = capturePrimarySharp();
}

void SplashOverlay::scheduleRecapture(int ms)
{
    m_recapture.stop();
    if (ms < 0) {
        return;
    }
    if (ms == 0) {
        captureFrost();
        update();
        return;
    }
    m_recapture.start(ms);
}

void SplashOverlay::setMasterLayers(const QVector<int>& layers)
{
    if (!m_pages) {
        return;
    }
    PageAction a;
    a.type = PageActionType::ShowLayers;
    a.layers = layers;
    QString err;
    (void)m_pages->showLayers({a}, QLatin1String(splash::kPage), {}, &err);
}

void SplashOverlay::setSuspended(bool on)
{
    if (!m_pages) {
        return;
    }
    m_pages->armDwellStartHold();
    m_pages->setDwellSuspended(on);
}

QRect SplashOverlay::targetRect(const QString& id) const
{
    return m_pages ? m_pages->targetVisualRect(QLatin1String(splash::kPage), id) : QRect{};
}

QRect SplashOverlay::dwellRect(const QString& id) const
{
    return m_pages ? m_pages->targetDwellRect(QLatin1String(splash::kPage), id) : QRect{};
}

QRect SplashOverlay::menuRect() const
{
    const QRect show = targetRect(QLatin1String(splash::kShow));
    return show.isEmpty() ? targetRect(QLatin1String(splash::kHide)) : show;
}

QRect SplashOverlay::menuDwell() const
{
    QRect d = dwellRect(QLatin1String(splash::kShow));
    if (d.width() < 8) {
        d = dwellRect(QLatin1String(splash::kHide));
    }
    return d;
}

QRect SplashOverlay::sleepRect() const
{
    return targetRect(QLatin1String(splash::kSleep));
}

QRect SplashOverlay::sleepDwell() const
{
    return dwellRect(QLatin1String(splash::kSleep));
}

QRect SplashOverlay::markRect(splash::Mark mark) const
{
    using splash::Mark;
    switch (mark) {
    case Mark::MenuChip:
        return menuRect();
    case Mark::MenuDwell:
        return menuDwell();
    case Mark::SleepChip:
        return sleepRect();
    case Mark::SleepDwell:
        return sleepDwell();
    case Mark::Drawer: {
        const QRect a = targetRect(QStringLiteral("open_keyboard"));
        const QRect b = targetRect(QStringLiteral("open_settings"));
        return (a.width() >= 8 && b.width() >= 8) ? a.united(b) : menuRect();
    }
    case Mark::AmberFrame:
    case Mark::Center:
    case Mark::None:
        break;
    }
    return {};
}

QPointF SplashOverlay::markPoint(splash::Mark mark) const
{
    const QPointF center = QRectF(overlayScreenGeometry()).center();
    const QPointF last = m_gazeValid ? m_gaze : center;
    if (mark == splash::Mark::Center) {
        return center;
    }
    return rectCenter(markRect(mark), last);
}

QRect SplashOverlay::navClusterGlobal() const
{
    const QRect screen = overlayScreenGeometry();
    const int totalW = 3 * splash::kChipW + 2 * splash::kChipGap;
    return QRect(screen.center().x() - totalW / 2, screen.center().y() - splash::kChipH / 2, totalW,
                 splash::kChipH);
}

QRect SplashOverlay::prevRectGlobal() const
{
    const QRect c = navClusterGlobal();
    return QRect(c.left(), c.top(), splash::kChipW, splash::kChipH);
}

QRect SplashOverlay::nextRectGlobal() const
{
    const QRect c = navClusterGlobal();
    return QRect(c.left() + splash::kChipW + splash::kChipGap, c.top(), splash::kChipW,
                 splash::kChipH);
}

QRect SplashOverlay::skipRectGlobal() const
{
    const QRect c = navClusterGlobal();
    return QRect(c.left() + 2 * (splash::kChipW + splash::kChipGap), c.top(), splash::kChipW,
                 splash::kChipH);
}

bool SplashOverlay::prevEnabled() const
{
    return m_phase != Phase::Welcome && m_phase != Phase::Done;
}

bool SplashOverlay::nextEnabled() const
{
    return m_phase != Phase::Fade && m_phase != Phase::Done;
}

double SplashOverlay::fittedScale() const
{
    const QRect screen = overlayScreenGeometry();
    if (screen.height() < 80) {
        return splash::kWantedScale;
    }
    const auto& s = splash::spec(int(m_phase));
    const QRect dwell = markRect(s.dwellZone != splash::Mark::None ? s.dwellZone
                                                                  : splash::Mark::MenuDwell);
    const double extra =
        dwell.isEmpty() ? 0.0 : qMax(0.0, dwell.center().y() + 48.0 - screen.bottom());
    const double avail = double(screen.height() - splash::kMiniTopPad - 16);
    const double sMax = avail / qMax(1.0, double(screen.height()) + extra);
    return qBound(0.78, qMin(splash::kWantedScale, sMax), 0.90);
}

double SplashOverlay::viewScale() const
{
    const auto& s = splash::spec(int(m_phase));
    const double fitted = fittedScale();
    return lerp(QPointF(scaleValue(s.scaleFrom, fitted), 0),
                QPointF(scaleValue(s.scaleTo, fitted), 0), ease(phaseT()))
        .x();
}

int SplashOverlay::miniTopPad() const
{
    const auto& s = splash::spec(int(m_phase));
    const double from = s.scaleFrom == splash::Scale::One ? 0.0 : double(splash::kMiniTopPad);
    const double to = s.scaleTo == splash::Scale::One ? 0.0 : double(splash::kMiniTopPad);
    return qRound(from + (to - from) * ease(phaseT()));
}

QRectF SplashOverlay::miniScreenLocal() const
{
    const QRect localScreen = localRect(overlayScreenGeometry());
    const double s = viewScale();
    QRectF mini(0, 0, localScreen.width() * s, localScreen.height() * s);
    mini.moveLeft(localScreen.center().x() - mini.width() / 2.0);
    mini.moveTop(localScreen.top() + miniTopPad());
    return mini;
}

QPointF SplashOverlay::mapGlobalToLocal(QPointF global) const
{
    const QRect screen = overlayScreenGeometry();
    const QRectF mini = miniScreenLocal();
    const double s = viewScale();
    const QPointF sl = global - QPointF(screen.topLeft());
    return QPointF(mini.left() + sl.x() * s, mini.top() + sl.y() * s);
}

QRectF SplashOverlay::mapGlobalToLocal(const QRect& global) const
{
    if (global.isEmpty()) {
        return {};
    }
    return QRectF(mapGlobalToLocal(QPointF(global.topLeft())),
                  QSizeF(global.width() * viewScale(), global.height() * viewScale()));
}

QRect SplashOverlay::localRect(const QRect& global) const
{
    return global.translated(-pos());
}

SplashOverlay::NavChip SplashOverlay::hitNav(const QPoint& global) const
{
    if (skipRectGlobal().contains(global)) {
        return NavChip::Skip;
    }
    if (nextRectGlobal().contains(global)) {
        return NavChip::Next;
    }
    if (prevRectGlobal().contains(global)) {
        return NavChip::Prev;
    }
    return NavChip::None;
}

double SplashOverlay::phaseT() const
{
    const auto& s = splash::spec(int(m_phase));
    const int dur = s.introMs > 0 ? s.introMs : s.autoMs;
    return dur <= 0 ? 1.0 : qBound(0.0, double(m_phaseElapsedMs) / double(dur), 1.0);
}

double SplashOverlay::enterFade() const
{
    return qBound(0.0, double(m_phaseElapsedMs) / 320.0, 1.0);
}

QPointF SplashOverlay::gazePos() const
{
    const auto& s = splash::spec(int(m_phase));
    const QPointF a = markPoint(s.gazeFrom);
    const QPointF b = markPoint(s.gazeTo);
    if (s.lerpGaze) {
        return lerp(a, b, ease(phaseT()));
    }
    return b;
}

double SplashOverlay::fadeAlpha() const
{
    if (m_phase == Phase::Fade) {
        return 1.0 - phaseT();
    }
    return m_phase == Phase::Done ? 0.0 : 1.0;
}

} // namespace gazer
