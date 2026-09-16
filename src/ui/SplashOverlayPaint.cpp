#include "ui/SplashOverlay.h"

#include "layout/PageHit.h"
#include "layout/PageTypes.h"
#include "ui/BoardPaint.h"
#include "ui/SplashOverlay_p.h"
#include "utils/ScreenGrab.h"

#include <QFont>
#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QtMath>

namespace gazer {

void SplashOverlay::paintBackdrop(QPainter& p) const
{
    if (!m_frost.isNull()) {
        p.drawPixmap(rect(), m_frost);
    } else {
        p.fillRect(rect(), QColor(12, 14, 18, 230));
    }
    QColor tint = m_theme.bgMain.isValid() ? m_theme.bgMain : QColor(16, 18, 22);
    tint.setAlpha(splash::spec(int(m_phase)).mini ? 200 : 150);
    p.fillRect(rect(), tint);
}

void SplashOverlay::paintMiniScreen(QPainter& p) const
{
    const QRectF mini = miniScreenLocal();
    if (mini.width() < 8 || mini.height() < 8) {
        return;
    }
    p.save();
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 110));
    p.drawRoundedRect(mini.adjusted(10, 14, 10, 14), 10.0, 10.0);

    QPainterPath bezel;
    bezel.addRoundedRect(mini.adjusted(-6, -6, 6, 6), 12.0, 12.0);
    QColor frame = m_theme.bgMain.isValid() ? m_theme.bgMain : QColor(30, 32, 38);
    p.fillPath(bezel, frame);
    QColor rim = m_accent;
    rim.setAlpha(160);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(rim, 2.0));
    p.drawPath(bezel);

    QPainterPath clip;
    clip.addRoundedRect(mini, 6.0, 6.0);
    p.setClipPath(clip);
    if (!m_screenGrab.isNull()) {
        p.drawPixmap(mini, m_screenGrab, QRectF(m_screenGrab.rect()));
    } else if (!m_frost.isNull()) {
        const QRect screen = overlayScreenGeometry();
        const QRect desk = virtualDesktop();
        p.drawPixmap(mini, m_frost, QRectF(QRect(screen.topLeft() - desk.topLeft(), screen.size())));
    } else {
        p.fillRect(mini, QColor(20, 22, 28));
    }
    p.restore();
}

void SplashOverlay::paintSuspendFrame(QPainter& p) const
{
    const QRectF mini = miniScreenLocal();
    if (mini.width() < 8 || mini.height() < 8) {
        return;
    }
    const qreal b = 6.0;
    const QRectF outer = mini.adjusted(-b, -b, b, b);
    QPainterPath ring;
    ring.addRect(outer);
    QPainterPath hole;
    hole.addRect(mini);
    ring -= hole;

    const QRectF chip = mapGlobalToLocal(sleepRect());
    if (chip.width() >= 8 && chip.height() >= 8) {
        QPainterPath gap;
        gap.addRect(chip.adjusted(-14, -10, 14, 14));
        ring -= gap;
    }

    p.save();
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 170, 60, 90));
    p.drawPath(ring);
    p.restore();
}

void SplashOverlay::paintSleepActive(QPainter& p) const
{
    const QRectF r = mapGlobalToLocal(sleepRect());
    if (r.width() < 8 || r.height() < 8) {
        return;
    }
    PageTarget t;
    t.label = QStringLiteral("Sleep");
    t.icon = QStringLiteral("Sleep");
    t.interactive = true;
    t.activeState = QStringLiteral("on");
    t.chrome.radius = PageBox::of(900.0, 900.0, 0.0, 0.0);
    BoardPaint::Live live;
    BoardPaint::paintTarget(p, t, r, m_theme,
                            m_theme.bgMain.isValid() ? m_theme.bgMain : QColor(16, 18, 22), nullptr,
                            false, 0.0, false, true, m_progress, live, false);
}

QRectF SplashOverlay::captionAnchor(splash::Mark mark) const
{
    if (mark == splash::Mark::AmberFrame) {
        const QRectF mini = miniScreenLocal();
        return QRectF(mini.center().x() - 40.0, mini.bottom() - 4.0, 80.0, 8.0);
    }
    return mapGlobalToLocal(markRect(mark));
}

void SplashOverlay::paintDwellZone(QPainter& p, const QRect& global, double pulse) const
{
    const QRectF local = mapGlobalToLocal(global);
    if (local.width() < 8 || local.height() < 8) {
        return;
    }
    p.save();
    QColor fill = m_accent.isValid() ? m_accent : ThemeColors::defaultProgressColor();
    fill.setAlpha(qBound(40, qRound(70.0 + 40.0 * pulse), 130));
    QPainterPath path;
    path.addRoundedRect(local, 14.0, 14.0);
    p.fillPath(path, fill);
    QColor pen = m_accent;
    pen.setAlpha(220);
    p.strokePath(path, QPen(pen, 3.0, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));

    const QRectF mini = miniScreenLocal();
    if (local.top() > mini.bottom() - 4) {
        p.setPen(QPen(pen, 2.0, Qt::DashLine, Qt::RoundCap));
        p.drawLine(QPointF(local.center().x(), mini.bottom()),
                   QPointF(local.center().x(), local.top()));
    }
    p.setPen(m_theme.text.isValid() ? m_theme.text : Qt::white);
    p.setFont(QFont(BoardPaint::segoeFamily(), 11, QFont::DemiBold));
    p.drawText(local.adjusted(10, 8, -10, -8), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
               QStringLiteral("Dwell zone  ·  below the screen"));
    p.restore();
}

void SplashOverlay::paintHoleRing(QPainter& p, const QRectF& local, double pulse) const
{
    if (local.width() < 8 || local.height() < 8) {
        return;
    }
    QColor ring = m_accent;
    ring.setAlpha(qBound(40, qRound(200.0 * pulse), 230));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(ring, 3.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawRoundedRect(local.adjusted(-6, -6, 6, 6), 12.0, 12.0);
}

void SplashOverlay::paintGaze(QPainter& p, const QPointF& local, double progress) const
{
    QColor fill = m_accent.isValid() ? m_accent : ThemeColors::defaultProgressColor();
    fill.setAlpha(120);
    p.setPen(Qt::NoPen);
    p.setBrush(fill);
    p.drawEllipse(local, splash::kGazeRadius, splash::kGazeRadius);
    QColor rim = fill;
    rim.setAlpha(230);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(rim, 3.0));
    p.drawEllipse(local, splash::kGazeRadius - 1.0, splash::kGazeRadius - 1.0);
    if (progress > 0.02) {
        const QRectF arc(local.x() - splash::kGazeRadius - 8.0,
                         local.y() - splash::kGazeRadius - 8.0,
                         (splash::kGazeRadius + 8.0) * 2.0, (splash::kGazeRadius + 8.0) * 2.0);
        QColor pie = m_accent;
        pie.setAlpha(210);
        p.setPen(QPen(pie, 5.0, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(arc, 90 * 16, -int(progress * 360.0 * 16.0));
    }
}

void SplashOverlay::paintWelcome(QPainter& p, double opacity) const
{
    if (opacity <= 0.02) {
        return;
    }
    p.save();
    const QRect screen = localRect(overlayScreenGeometry());
    const QRect chips = localRect(navClusterGlobal());
    const int logo = 180;
    const int titleH = 40;
    const int bodyH = 48;
    const int blockH = logo + 8 + titleH + 4 + bodyH;
    QRect block(screen.left(), chips.top() - 20 - blockH, screen.width(), blockH);
    if (block.top() < screen.top() + 12) {
        block.moveTop(screen.top() + 12);
    }
    QRect logoR(screen.center().x() - logo / 2, block.top(), logo, logo);
    p.setOpacity(p.opacity() * opacity);
    if (!m_logo.isNull()) {
        const QSize s = m_logo.size() / qMax(1, int(m_logo.devicePixelRatio()));
        const QRect dest(screen.center().x() - s.width() / 2,
                         logoR.top() + (logo - s.height()) / 2, s.width(), s.height());
        p.drawPixmap(dest, m_logo);
    }
    const QString family = BoardPaint::segoeFamily();
    p.setPen(m_theme.text.isValid() ? m_theme.text : QColor(245, 245, 248));
    p.setFont(QFont(family, 28, QFont::DemiBold));
    const QRect title(screen.left(), logoR.bottom() + 8, screen.width(), titleH);
    p.drawText(title, Qt::AlignHCenter | Qt::AlignTop, QStringLiteral("Welcome to Gazer"));
    p.setPen(m_theme.textSecondary.isValid() ? m_theme.textSecondary : QColor(210, 214, 220));
    p.setFont(QFont(family, 14, QFont::Normal));
    p.drawText(QRect(screen.left() + 40, title.bottom() + 4, screen.width() - 80, bodyH),
               Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
               QStringLiteral("Look at Next to continue. Dock chips wake when you look "
                              "just below the screen."));
    p.restore();
}

void SplashOverlay::paintCaption(QPainter& p, const QRectF& nearLocal, const QString& title,
                                const QString& body, double opacity, bool below) const
{
    if (opacity <= 0.02 || nearLocal.isEmpty()) {
        return;
    }
    p.save();
    const QRect screen = localRect(overlayScreenGeometry());
    const QString family = BoardPaint::segoeFamily();
    QFont titleFont(family, 13, QFont::DemiBold);
    QFont bodyFont(family, 11, QFont::Normal);
    const int flags = int(Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap);
    const int padX = 16;
    const int padTop = 10;
    const int padBot = 12;
    const int gap = 6;
    const int maxInner = qMax(200, screen.width() - 24 - 2 * padX);
    int inner = 300 - 2 * padX;
    auto measure = [&](int w) {
        const QRect tr = QFontMetrics(titleFont).boundingRect(QRect(0, 0, w, 4000), flags, title);
        const QRect br = QFontMetrics(bodyFont).boundingRect(QRect(0, 0, w, 4000), flags, body);
        return QSize(qMax(tr.width(), br.width()), tr.height() + gap + br.height());
    };
    QSize content = measure(inner);
    while (inner < maxInner && content.height() > 56) {
        inner = qMin(maxInner, inner + 24);
        content = measure(inner);
    }
    int titlePx = 13;
    int bodyPx = 11;
    while (content.height() > 72 && titlePx > 10) {
        --titlePx;
        --bodyPx;
        titleFont.setPointSize(titlePx);
        bodyFont.setPointSize(qMax(9, bodyPx));
        content = measure(inner);
    }
    QRect b(0, 0, inner + 2 * padX, content.height() + padTop + padBot);
    const int maxH = qMax(64, screen.height() - 24);
    if (b.height() > maxH) {
        b.setHeight(maxH);
    }
    if (below) {
        b.moveCenter(QPoint(qRound(nearLocal.center().x()),
                            qRound(nearLocal.bottom()) + 18 + b.height() / 2));
    } else {
        b.moveCenter(QPoint(qRound(nearLocal.center().x()),
                            qRound(nearLocal.top()) - 16 - b.height() / 2));
    }
    if (b.top() < screen.top() + 12) {
        b.moveTop(screen.top() + 12);
    }
    if (b.bottom() > screen.bottom() - 12) {
        b.moveBottom(screen.bottom() - 12);
    }
    if (b.left() < screen.left() + 12) {
        b.moveLeft(screen.left() + 12);
    }
    if (b.right() > screen.right() - 12) {
        b.moveRight(screen.right() - 12);
    }
    p.setOpacity(p.opacity() * opacity);
    QColor bg = m_theme.bgMain.isValid() ? m_theme.bgMain : QColor(24, 26, 32);
    bg.setAlpha(235);
    BoardPaint::fillRound(p, QRectF(b), 14.0, bg);
    QColor rim = m_accent;
    rim.setAlpha(180);
    BoardPaint::strokeRound(p, QRectF(b), 14.0, rim, 2.0);
    p.setPen(QPen(rim, 2.0, Qt::SolidLine, Qt::RoundCap));
    if (below) {
        p.drawLine(QPoint(b.center().x(), b.top()),
                   QPoint(qRound(nearLocal.center().x()), qRound(nearLocal.bottom())));
    } else {
        p.drawLine(QPoint(b.center().x(), b.bottom()),
                   QPoint(qRound(nearLocal.center().x()), qRound(nearLocal.top())));
    }
    const QRect titleR(b.left() + padX, b.top() + padTop, b.width() - 2 * padX,
                       QFontMetrics(titleFont)
                           .boundingRect(QRect(0, 0, inner, 4000), flags, title)
                           .height());
    p.setPen(m_theme.text.isValid() ? m_theme.text : Qt::white);
    p.setFont(titleFont);
    p.drawText(titleR, flags, title);
    const QRect bodyR(titleR.left(), titleR.bottom() + gap, titleR.width(),
                      b.bottom() - padBot - (titleR.bottom() + gap));
    p.setPen(m_theme.textSecondary.isValid() ? m_theme.textSecondary : QColor(210, 214, 220));
    p.setFont(bodyFont);
    p.drawText(bodyR, flags, body);
    p.restore();
}

void SplashOverlay::paintNavChip(QPainter& p, const QRect& global, const QString& icon,
                                const QString& label, double progress, bool enabled) const
{
    p.save();
    p.setOpacity(p.opacity() * (enabled ? 1.0 : 0.42));
    PageTarget t;
    t.label = label;
    t.icon = icon;
    t.interactive = enabled;
    t.chrome.background.token = QStringLiteral("bg95");
    t.chrome.radius = PageBox::all(double(splash::kChipRadius));
    BoardPaint::Live live;
    BoardPaint::paintTarget(p, t, QRectF(localRect(global)), m_theme,
                            m_theme.bgMain.isValid() ? m_theme.bgMain : QColor(16, 18, 22), nullptr,
                            enabled && progress > 0.02, enabled ? progress : 0.0, false, false,
                            m_progress, live, false);
    p.restore();
}

void SplashOverlay::paintNav(QPainter& p) const
{
    const auto& s = splash::spec(int(m_phase));
    const QRect cluster = localRect(navClusterGlobal());
    if (s.title[0] != '\0') {
        p.setPen(m_theme.textSecondary.isValid() ? m_theme.textSecondary : QColor(200, 204, 210));
        p.setFont(QFont(BoardPaint::segoeFamily(), 13, QFont::DemiBold));
        p.drawText(QRect(cluster.left(), cluster.top() - 36, cluster.width(), 28),
                   Qt::AlignHCenter | Qt::AlignVCenter,
                   QStringLiteral("%1  ·  %2 / %3")
                       .arg(QLatin1String(s.title))
                       .arg(s.step)
                       .arg(splash::kStepCount));
    }
    const double pPrev = (m_navHit == NavChip::Prev) ? m_navDwell.progress() : 0.0;
    const double pNext = (m_navHit == NavChip::Next) ? m_navDwell.progress() : 0.0;
    const double pSkip = (m_navHit == NavChip::Skip) ? m_navDwell.progress() : 0.0;
    paintNavChip(p, prevRectGlobal(), QStringLiteral("ArrowLeft"), QStringLiteral("Previous"),
                 pPrev, prevEnabled());
    const QString next = (m_phase == Phase::Callouts) ? QStringLiteral("Done")
                                                      : QStringLiteral("Next");
    paintNavChip(p, nextRectGlobal(), QStringLiteral("ArrowRight"), next, pNext, nextEnabled());
    paintNavChip(p, skipRectGlobal(), QStringLiteral("close"), QStringLiteral("Skip"), pSkip, true);
}

void SplashOverlay::paintEvent(QPaintEvent*)
{
    if (!m_active) {
        return;
    }
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.setOpacity(fadeAlpha());

    const auto& s = splash::spec(int(m_phase));
    paintBackdrop(p);
    if (viewScale() < 0.995) {
        paintMiniScreen(p);
        if (s.suspend == 1) {
            paintSleepActive(p);
            paintSuspendFrame(p);
        }
    }

    const double pulse = 0.55 + 0.45 * (0.5 + 0.5 * qSin(m_clock.elapsed() / 280.0));
    if (s.dwellZone != splash::Mark::None) {
        paintDwellZone(p, markRect(s.dwellZone), pulse);
    }
    if (s.chipRing == splash::Mark::Drawer) {
        for (const splash::Callout& c : splash::kCallouts) {
            paintHoleRing(p, mapGlobalToLocal(targetRect(QLatin1String(c.targetId))), pulse);
        }
    } else if (s.chipRing != splash::Mark::None) {
        paintHoleRing(p, mapGlobalToLocal(markRect(s.chipRing)), pulse);
    }

    if (s.welcome) {
        const double op = (s.introMs > 0) ? (1.0 - phaseT()) : enterFade();
        paintWelcome(p, op);
    }
    if (s.capTitle[0] != '\0') {
        const double op = s.lerpGaze ? phaseT() : enterFade();
        paintCaption(p, captionAnchor(s.captionAt), QLatin1String(s.capTitle),
                     QLatin1String(s.capBody), op, s.captionBelow);
    }
    if (s.callouts) {
        int i = 0;
        for (const splash::Callout& c : splash::kCallouts) {
            QRectF box = mapGlobalToLocal(targetRect(QLatin1String(c.targetId)));
            box.translate(0, -double((i % 2) * 64));
            paintCaption(p, box, QLatin1String(c.title), QLatin1String(c.body), enterFade());
            ++i;
        }
    }

    if (s.gazeCue && m_gazeValid) {
        double gp = 0.0;
        if (s.lerpGaze) {
            gp = phaseT();
        } else if (s.autoMs == 0 && !s.welcome && !s.callouts) {
            gp = 0.35 + 0.65 * pulse;
        }
        paintGaze(p, mapGlobalToLocal(m_gaze), gp);
    }

    paintNav(p);
    p.setOpacity(1.0);
}

} // namespace gazer
