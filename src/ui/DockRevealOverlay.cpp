#include "ui/DockRevealOverlay.h"

#include "utils/Log.h"

#include <QGuiApplication>
#include <QPainter>
#include <QPaintEvent>
#include <QScreen>
#include <QShowEvent>

namespace gazer {

namespace {
const char* kHitId = "dock_reveal";
}

DockRevealOverlay::DockRevealOverlay(QObject* parent)
    : OverlaySurface(nullptr)
    , m_dwell(this)
{
    Q_UNUSED(parent);
    resize(m_hotW, m_hotH);
    hide();

    m_dwell.setDwellMs(500);
    m_dwell.setInvalidGraceMs(220);
    m_dwell.setEnabled(true);
    connect(&m_dwell, &DwellStateMachine::dwellProgress, this,
            [this](const QString& id, double progress) {
                m_hoverId = id;
                m_progress = progress;
                update();
            });
    connect(&m_dwell, &DwellStateMachine::itemActivated, this, [this](const QString& id) {
        m_dwell.leave();
        m_hoverId.clear();
        m_progress = 0.0;
        update();
        if (id == QLatin1String(kHitId)) {
            GAZER_INFO << "Dock reveal activated";
            emit dockRevealRequested();
        }
    });

    if (auto* app = qGuiApp) {
        connect(app, &QGuiApplication::primaryScreenChanged, this,
                [this](QScreen*) { syncPlacement(); });
    }
    syncPlacement();
}

void DockRevealOverlay::setEnabledReveal(bool enabled)
{
    if (m_enabled == enabled) {
        if (enabled) {
            ensureVisible();
        }
        return;
    }
    m_enabled = enabled;
    if (!m_enabled) {
        m_dwell.leave();
        m_hoverId.clear();
        m_progress = 0.0;
        hide();
        GAZER_INFO << "Dock reveal OFF";
        return;
    }
    GAZER_INFO << "Dock reveal ON at" << m_hotScreenRect;
    ensureVisible();
}

void DockRevealOverlay::setDwellMs(int ms)
{
    m_dwell.setDwellMs(qMax(250, ms));
}

void DockRevealOverlay::syncPlacement()
{
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) {
        return;
    }

    // Bottom-left band that lives mostly in the taskbar / below the work area so
    // it does not cover the dock chip (chip sits just above avail.bottom()).
    // Still tall enough for easy gaze landing.
    const QRect avail = screen->availableGeometry();
    const QRect full = screen->geometry();
    const int margin = 4;
    const int x = full.left() + margin;
    // Sit primarily under the work area; peek ~36px into content for easy hit.
    const int top = qMax(full.top(), avail.bottom() - 36);
    const int bottom = full.bottom() + 1;
    const int h = qMax(m_hotH, bottom - top);
    const int w = m_hotW;

    m_hotScreenRect = QRect(x, top, w, h);
    setGeometry(m_hotScreenRect);
}

void DockRevealOverlay::ensureVisible()
{
    if (!m_enabled) {
        return;
    }
    syncPlacement();
    showOverlay();
}

void DockRevealOverlay::showEvent(QShowEvent* event)
{
    OverlaySurface::showEvent(event);
    syncPlacement();
}

bool DockRevealOverlay::containsGaze(const GazePoint& point) const
{
    if (!m_enabled || !point.valid) {
        return false;
    }
    // Hit-test in screen space — do not require widget mapToGlobal (can lag).
    return m_hotScreenRect.adjusted(-20, -20, 20, 20)
        .contains(QPoint(qRound(point.x), qRound(point.y)));
}

void DockRevealOverlay::onGaze(const GazePoint& point)
{
    if (!m_enabled) {
        return;
    }
    if (!isVisible()) {
        ensureVisible();
    }

    if (!point.valid) {
        m_dwell.onGazeSample(point, m_hoverId);
        return;
    }

    const QString hit = containsGaze(point) ? QString::fromLatin1(kHitId) : QString();
    if (!hit.isEmpty() && m_hoverId.isEmpty()) {
        GAZER_DEBUG << "Dock reveal gaze enter" << point.x << point.y;
    }
    m_dwell.onGazeSample(point, hit);
    if (hit.isEmpty() && !m_hoverId.isEmpty()) {
        m_hoverId.clear();
        m_progress = 0.0;
        update();
    }
}

void DockRevealOverlay::paintEvent(QPaintEvent* /*event*/)
{
    if (!m_enabled) {
        return;
    }

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const bool hover = !m_hoverId.isEmpty();
    // Visible enough to find while learning; stronger while dwelling.
    const int alpha = hover ? 200 : 70;
    p.setBrush(QColor(10, 126, 164, alpha));
    p.setPen(QPen(QColor(0, 220, 255, hover ? 255 : 120), hover ? 3.0 : 1.5));
    p.drawRoundedRect(rect().adjusted(2, 2, -2, -2), 12, 12);

    p.setPen(QColor(240, 248, 255, hover ? 255 : 180));
    p.setFont(QFont(QStringLiteral("Segoe UI"), 12, QFont::DemiBold));
    p.drawText(rect(), Qt::AlignCenter,
               hover ? QStringLiteral("Show Dock…") : QStringLiteral("Dock"));

    if (hover && m_progress > 0.0) {
        p.setPen(QPen(QColor(0, 220, 255), 4.0));
        p.setBrush(Qt::NoBrush);
        p.drawArc(rect().adjusted(10, 10, -10, -10), 90 * 16,
                  static_cast<int>(-360 * 16 * m_progress));
    }
}

} // namespace gazer
