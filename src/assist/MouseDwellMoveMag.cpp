#include "assist/MouseDwellMove.h"
#include "assist/MouseDwellMove_p.h"

#include "ui/PickStyle.h"
#include "utils/Log.h"
#include "utils/ScreenGrab.h"

#include <QGuiApplication>
#include <QScreen>
#include <QtMath>

namespace gazer {

int MouseDwellMove::destSideFor(QScreen* screen) const
{
    const QRect g = screen->geometry();
    const int maxSide = qMax(80, qMin(g.width(), g.height()));
    if (m_magPickFullScreen) {
        return maxSide;
    }
    return qMin(qMax(80, m_pickWindowPx), maxSide);
}

MagPresentation MouseDwellMove::makePreClickSpec(const QPoint& center) const
{
    MagPresentation spec;
    spec.srcCenter = center;
    spec.zoom = m_armZoom.resolvedLevel(m_pickZoom);
    QScreen* screen = QGuiApplication::screenAt(center);
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    spec.destSide = screen ? destSideFor(screen) : 80;
    spec.destCenter = (m_magPickCenterOnDwell || !screen) ? center : screen->geometry().center();
    return spec;
}

MagPresentation MouseDwellMove::makeForesightSpec(const QPoint& srcCenter, const QPoint& destCenter,
                                                 int destSide) const
{
    MagPresentation spec;
    spec.srcCenter = srcCenter;
    spec.destCenter = destCenter;
    spec.zoom = m_armZoom.resolvedLevel(m_pickZoom);
    spec.destSide = destSide;
    return spec;
}

void MouseDwellMove::startAimPhase()
{
    m_outsideSelectsNewRegion = false;
    m_mag = {};
    if (m_purpose != ArmPurpose::LookToScrollPlace
        && m_purpose != ArmPurpose::ComboMousePlace && armWantsForesight()) {
        if (const auto fs = m_foresight.peek(m_clock.elapsed())) {
            QScreen* screen = QGuiApplication::screenAt(*fs);
            if (!screen) {
                screen = QGuiApplication::primaryScreen();
            }
            const int side = screen ? destSideFor(screen) : 0;
            if (side >= 80
                && beginMagPick(makeForesightSpec(*fs, *fs, side), /*outsideSelectsNewRegion=*/true)) {
                m_foresight.clear();
                return;
            }
            GAZER_WARN << "MouseDwellMove foresight zoom failed — falling back";
            m_foresight.clear();
        }
    }
    setPhase(useMagPickThisArm() ? Phase::MagRegion : Phase::Direct);
}

bool MouseDwellMove::beginMagPick(const MagPresentation& spec, bool outsideSelectsNewRegion)
{
    QScreen* screen = QGuiApplication::screenAt(spec.destCenter);
    if (!screen) {
        screen = QGuiApplication::screenAt(spec.srcCenter);
    }
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) {
        GAZER_WARN << "MouseDwellMove mag-pick: no screen";
        return false;
    }

    QRect src;
    QRect dest;
    if (!layoutMagWindow(screen, spec, &src, &dest)) {
        GAZER_WARN << "MouseDwellMove mag-pick: layout failed at" << spec.srcCenter;
        return false;
    }

    QPixmap grab = grabScreenRect(screen, src, QColor(12, 14, 18));
    if (grab.isNull() || grab.width() < 2 || grab.height() < 2) {
        const QRect local = src.translated(-screen->geometry().topLeft());
        grab = screen->grabWindow(0, local.x(), local.y(), local.width(), local.height());
    }
    if (grab.isNull() || grab.width() < 2 || grab.height() < 2) {
        GAZER_WARN << "MouseDwellMove mag-pick: grab failed" << src;
        return false;
    }

    m_mag = spec;
    m_mag.destSide = dest.width();
    m_outsideSelectsNewRegion = outsideSelectsNewRegion;
    m_magSourceRect = src;
    m_magDisplayRect = dest;
    m_magPixmap = grab.scaled(dest.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    m_magGazeInside = true;

    QString hint = QStringLiteral("Dwell to pick point (static zoom)");
    if (outsideSelectsNewRegion) {
        if (useMagPickThisArm() && armWantsBonusZoom()) {
            hint = QStringLiteral("Foresight — dwell inside to zoom again, outside for a new region");
        } else if (useMagPickThisArm()) {
            hint = QStringLiteral("Foresight — dwell inside to place, outside for pre-click zoom");
        } else {
            hint = QStringLiteral("Foresight — dwell inside to place");
        }
    }

    if (m_cursor) {
        m_cursor->hide();
    }
    m_magOverlay->showCapture(m_magPixmap, dest, hint, m_pickWindowRound);
    setPhase(Phase::MagPoint);
    resetDwell();
    markSelectDeadline();
    GAZER_INFO << "MouseDwellMove mag-pick region" << src << "dest" << dest;
    return true;
}

bool MouseDwellMove::gazeInZoomWindow(const QPointF& gaze) const
{
    if (m_magDisplayRect.isEmpty()) {
        return false;
    }
    if (!m_pickWindowRound) {
        return m_magDisplayRect.contains(gaze.toPoint());
    }
    const QRectF r = m_magDisplayRect;
    const double rx = r.width() * 0.5;
    const double ry = r.height() * 0.5;
    if (rx <= 0.0 || ry <= 0.0) {
        return false;
    }
    const double nx = (gaze.x() - r.center().x()) / rx;
    const double ny = (gaze.y() - r.center().y()) / ry;
    return (nx * nx + ny * ny) <= 1.0;
}

QPoint MouseDwellMove::mapDisplayToSource(const QPointF& gaze) const
{
    if (m_magSourceRect.isEmpty() || m_magDisplayRect.width() < 1
        || m_magDisplayRect.height() < 1) {
        return gaze.toPoint();
    }
    const double gx = qBound(double(m_magDisplayRect.left()), gaze.x(),
                             double(m_magDisplayRect.right()));
    const double gy = qBound(double(m_magDisplayRect.top()), gaze.y(),
                             double(m_magDisplayRect.bottom()));
    const double lx =
        (gx - m_magDisplayRect.left()) * m_magSourceRect.width() / double(m_magDisplayRect.width());
    const double ly = (gy - m_magDisplayRect.top()) * m_magSourceRect.height()
                      / double(m_magDisplayRect.height());
    const int sx = qBound(m_magSourceRect.left(), m_magSourceRect.left() + qRound(lx),
                          m_magSourceRect.right());
    const int sy = qBound(m_magSourceRect.top(), m_magSourceRect.top() + qRound(ly),
                          m_magSourceRect.bottom());
    return {sx, sy};
}

void MouseDwellMove::finishMagPoint(const QPointF& gaze)
{
    if (m_magSourceRect.isEmpty() || m_magDisplayRect.width() < 1
        || m_magDisplayRect.height() < 1) {
        GAZER_WARN << "MouseDwellMove mag pick: empty geometry";
        setArmed(false);
        return;
    }
    placeCursor(mapDisplayToSource(gaze));
}

void MouseDwellMove::onGazeInZoom(const QPointF& g, double dtSec)
{
    if (m_magOverlay && !m_magOverlay->isVisible() && !m_magPixmap.isNull()) {
        m_magOverlay->showCapture(m_magPixmap, m_magDisplayRect, {}, m_pickWindowRound);
    }

    const bool inside = gazeInZoomWindow(g);
    if (inside != m_magGazeInside) {
        m_magGazeInside = inside;
        resetDwell();
        applyDwellForPhase();
        if (m_magOverlay) {
            m_magOverlay->setProgress(0.0);
            if (!inside) {
                m_magOverlay->clearPick();
            }
        }
        if (inside && m_cursor) {
            m_cursor->hide();
        }
    }

    if (!inside) {
        if (!m_outsideSelectsNewRegion) {
            resetDwell();
            if (m_magOverlay) {
                m_magOverlay->setProgress(0.0);
                m_magOverlay->clearPick();
            }
            return;
        }
        const bool done = m_dwell.sample(g, dtSec);
        emit progressChanged(m_dwell.progress());
        if (m_cursor) {
            m_cursor->setProgress(m_dwell.progress());
            m_cursor->placeCenter(
                QPoint(qRound(m_dwell.commitPos().x()), qRound(m_dwell.commitPos().y())));
        }
        if (!done) {
            return;
        }
        const QPoint outside(qRound(m_dwell.commitPos().x()), qRound(m_dwell.commitPos().y()));
        if (m_magPickEnabled) {
            if (!beginMagPick(makePreClickSpec(outside), /*outsideSelectsNewRegion=*/false)) {
                GAZER_WARN << "MouseDwellMove outside pre-click zoom failed";
            }
        } else {
            placeCursor(outside);
        }
        return;
    }

    const bool done = m_dwell.sample(g, dtSec);
    const QPointF follow = m_dwell.commitPos();
    const QPoint local(
        qBound(0, qRound(follow.x() - m_magDisplayRect.left()),
               qMax(0, m_magDisplayRect.width() - 1)),
        qBound(0, qRound(follow.y() - m_magDisplayRect.top()),
               qMax(0, m_magDisplayRect.height() - 1)));
    m_magOverlay->setPickLocal(local);
    m_magOverlay->setProgress(m_dwell.progress());
    emit progressChanged(m_dwell.progress());
    if (!done) {
        return;
    }

    if (m_outsideSelectsNewRegion && useMagPickThisArm() && armWantsBonusZoom()) {
        const int side = m_mag.destSide > 0 ? m_mag.destSide
                                            : destSideFor(QGuiApplication::primaryScreen());
        if (!beginMagPick(makeForesightSpec(mapDisplayToSource(follow),
                                            QPoint(qRound(follow.x()), qRound(follow.y())), side),
                          /*outsideSelectsNewRegion=*/false)) {
            finishMagPoint(follow);
        }
    } else {
        finishMagPoint(follow);
    }
}

} // namespace gazer
