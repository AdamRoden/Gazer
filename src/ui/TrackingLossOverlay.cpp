#include "ui/TrackingLossOverlay.h"

#include "utils/WinOverlay.h"

#include <QFont>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>

namespace gazer {

namespace {
constexpr int kCellW = 220;
constexpr int kCellH = 120;
constexpr int kGap = 16;
constexpr int kStatusH = 84;
} // namespace

TrackingLossOverlay::TrackingLossOverlay(QWidget* parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool
                   | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_QuitOnClose, false);
    hide();
}

TrackingLossOverlay::~TrackingLossOverlay()
{
    if (QWindow* wh = windowHandle()) {
        unregisterOverlayWindow(wh);
    }
}

void TrackingLossOverlay::layoutCells()
{
    m_cells.clear();
    m_cells.push_back({QStringLiteral("restart"), QStringLiteral("Restart"), {}});
    m_cells.push_back({QStringLiteral("close"), QStringLiteral("Close Gazer"), {}});

    QScreen* screen = QGuiApplication::primaryScreen();
    const QRect desk = screen ? screen->geometry() : QRect(0, 0, 1280, 720);
    const int n = m_cells.size();
    const int totalW = n * kCellW + (n - 1) * kGap;
    const int x0 = desk.center().x() - totalW / 2;
    const int y0 = desk.bottom() - kCellH - 64;
    for (int i = 0; i < n; ++i) {
        m_cells[i].rect = QRect(x0 + i * (kCellW + kGap), y0, kCellW, kCellH);
    }
    m_statusRect = QRect(x0, y0 - kStatusH - 12, totalW, kStatusH);
    const QRect u = m_statusRect.united(m_cells.first().rect).united(m_cells.last().rect);
    setGeometry(u.adjusted(-16, -16, 16, 16));
}

void TrackingLossOverlay::showLoss(const QString& status)
{
    m_status = status;
    if (!isVisible()) {
        m_firing = false;
        layoutCells();
        show();
        (void)winId();
        if (QWindow* wh = windowHandle()) {
            registerOverlayWindow(wh, OverlayLayer::Assist);
        }
        applyOverlayWindowChrome(this, /*overlay=*/true);
        return;
    }
    update();
}

void TrackingLossOverlay::hideLoss()
{
    m_firing = false;
    if (QWindow* wh = windowHandle()) {
        unregisterOverlayWindow(wh);
    }
    hide();
}

void TrackingLossOverlay::fire(const QString& id)
{
    if (m_firing) {
        return;
    }
    m_firing = true;
    if (id == QLatin1String("restart")) {
        emit restartRequested();
    } else if (id == QLatin1String("close")) {
        emit closeRequested();
    }
    m_firing = false;
}

void TrackingLossOverlay::mousePressEvent(QMouseEvent* event)
{
    if (!event) {
        return;
    }
    const QPoint global = event->globalPosition().toPoint();
    for (const Cell& c : m_cells) {
        if (c.rect.contains(global)) {
            fire(c.id);
            return;
        }
    }
}

void TrackingLossOverlay::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRect status = m_statusRect.translated(-pos());
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(20, 20, 24, 230));
    p.drawRoundedRect(status, 16, 16);
    p.setPen(Qt::white);
    p.setFont(QFont(QStringLiteral("Segoe UI"), 12));
    p.drawText(status.adjusted(12, 8, -12, -8), Qt::AlignCenter | Qt::TextWordWrap, m_status);

    for (const Cell& c : m_cells) {
        const QRect r = c.rect.translated(-pos());
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(20, 20, 24, 230));
        p.drawRoundedRect(r, 16, 16);
        p.setPen(Qt::white);
        p.setFont(QFont(QStringLiteral("Segoe UI"), 14, QFont::DemiBold));
        p.drawText(r, Qt::AlignCenter, c.label);
    }
}

} // namespace gazer
