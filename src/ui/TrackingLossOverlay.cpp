#include "ui/TrackingLossOverlay.h"

#include "utils/WinOverlay.h"

#include <QFont>
#include <QGuiApplication>
#include <QPainter>
#include <QScreen>

namespace gazer {

namespace {
constexpr int kNoticeW = 460;
constexpr int kNoticeH = 84;
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

void TrackingLossOverlay::layoutNotice()
{
    QScreen* screen = QGuiApplication::primaryScreen();
    const QRect desk = screen ? screen->geometry() : QRect(0, 0, 1280, 720);
    const int x0 = desk.center().x() - kNoticeW / 2;
    const int y0 = desk.bottom() - kNoticeH - 64;
    m_statusRect = QRect(x0, y0, kNoticeW, kNoticeH);
    setGeometry(m_statusRect.adjusted(-16, -16, 16, 16));
}

void TrackingLossOverlay::showLoss(const QString& status)
{
    m_status = status;
    if (!isVisible()) {
        layoutNotice();
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
    if (QWindow* wh = windowHandle()) {
        unregisterOverlayWindow(wh);
    }
    hide();
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
    p.setFont(QFont(QStringLiteral("Segoe UI"), 14, QFont::DemiBold));
    p.drawText(status.adjusted(16, 8, -16, -8), Qt::AlignCenter | Qt::TextWordWrap, m_status);
}

} // namespace gazer
