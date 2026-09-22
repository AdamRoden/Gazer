#include "ui/RescueOverlay.h"

#include "utils/WinOverlay.h"

#include <QCursor>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>

namespace gazer {

namespace {
constexpr int kDwellMs = 900;
constexpr int kCellW = 240;
constexpr int kCellH = 120;
constexpr int kGap = 16;
} // namespace

RescueOverlay::RescueOverlay(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool
                   | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_QuitOnClose, false);
    m_tick.setInterval(50);
    connect(&m_tick, &QTimer::timeout, this, &RescueOverlay::tickGaze);
    hide();
}

void RescueOverlay::setMode(Mode mode)
{
    if (m_mode == mode) {
        if (mode != Mode::Hidden) {
            layoutCells();
            raise();
            applyOverlayWindowChrome(this, true);
        }
        return;
    }
    m_mode = mode;
    m_hoverId.clear();
    m_dwell.invalidate();
    if (mode == Mode::Hidden) {
        m_tick.stop();
        hide();
        return;
    }
    layoutCells();
    show();
    raise();
    applyOverlayWindowChrome(this, true);
    m_tick.start();
}

void RescueOverlay::layoutCells()
{
    m_cells.clear();
    QScreen* screen = QGuiApplication::primaryScreen();
    const QRect desk = screen ? screen->geometry() : QRect(0, 0, 1280, 720);
    if (m_mode == Mode::Yield) {
        m_cells.push_back({QStringLiteral("yield"), QStringLiteral("Show desktop"), {}});
    } else if (m_mode == Mode::CrashLoop) {
        m_cells.push_back({QStringLiteral("restart"), QStringLiteral("Restart"), {}});
        m_cells.push_back({QStringLiteral("safe"), QStringLiteral("Mouse pointer"), {}});
        m_cells.push_back({QStringLiteral("quit"), QStringLiteral("Quit"), {}});
    }
    const int n = m_cells.size();
    if (n <= 0) {
        return;
    }
    const int totalW = n * kCellW + (n - 1) * kGap;
    const int x0 = desk.center().x() - totalW / 2;
    const int y0 = desk.bottom() - kCellH - 48;
    for (int i = 0; i < n; ++i) {
        m_cells[i].rect = QRect(x0 + i * (kCellW + kGap), y0, kCellW, kCellH);
    }
    QRect u = m_cells[0].rect;
    for (int i = 1; i < n; ++i) {
        u = u.united(m_cells[i].rect);
    }
    setGeometry(u.adjusted(-12, -12, 12, 12));
}

void RescueOverlay::tickGaze()
{
    const QPoint g = QCursor::pos();
    const QPoint local = g - pos();
    QString hit;
    for (const Cell& c : m_cells) {
        if (c.rect.translated(-pos()).contains(local)) {
            hit = c.id;
            break;
        }
    }
    if (hit != m_hoverId) {
        m_hoverId = hit;
        if (hit.isEmpty()) {
            m_dwell.invalidate();
        } else {
            m_dwell.start();
        }
        update();
        return;
    }
    if (!hit.isEmpty() && m_dwell.isValid() && m_dwell.elapsed() >= kDwellMs) {
        m_dwell.invalidate();
        fire(hit);
    }
    update();
}

void RescueOverlay::fire(const QString& id)
{
    if (id == QLatin1String("yield")) {
        emit yieldDesktop();
    } else if (id == QLatin1String("restart")) {
        emit restartHost();
    } else if (id == QLatin1String("safe")) {
        emit restartSafe();
    } else if (id == QLatin1String("quit")) {
        emit quitGuard();
    }
}

void RescueOverlay::mousePressEvent(QMouseEvent* event)
{
    if (!event) {
        return;
    }
    const QPoint local = event->globalPosition().toPoint() - pos();
    for (const Cell& c : m_cells) {
        if (c.rect.translated(-pos()).contains(local)) {
            fire(c.id);
            return;
        }
    }
}

void RescueOverlay::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    for (const Cell& c : m_cells) {
        const QRect r = c.rect.translated(-pos());
        const bool hover = c.id == m_hoverId;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(20, 20, 24, 220));
        p.drawRoundedRect(r, 16, 16);
        if (hover && m_dwell.isValid()) {
            const double t = qBound(0.0, double(m_dwell.elapsed()) / double(kDwellMs), 1.0);
            QRect fill = r.adjusted(8, 8, -8, -8);
            fill.setWidth(int(fill.width() * t));
            p.setBrush(QColor(255, 170, 60, 140));
            p.drawRoundedRect(fill, 10, 10);
        }
        p.setPen(Qt::white);
        p.setFont(QFont(QStringLiteral("Segoe UI"), 16, QFont::DemiBold));
        p.drawText(r, Qt::AlignCenter, c.label);
    }
}

} // namespace gazer
