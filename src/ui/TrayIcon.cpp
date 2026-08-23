#include "ui/TrayIcon.h"

#include "ui/AppIcon.h"
#include "utils/Log.h"

#include <QAction>
#include <QIcon>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace gazer {

namespace {

QIcon makeFallbackTrayIcon()
{
    QPixmap pm(64, 64);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::black);
    p.drawRoundedRect(0, 0, 64, 64, 8, 8);
    p.setBrush(Qt::white);
    p.drawEllipse(16, 18, 28, 28);
    p.setBrush(Qt::black);
    p.drawEllipse(24, 26, 12, 12);
    QPainterPath lid;
    lid.moveTo(6, 30);
    lid.cubicTo(18, 10, 40, 8, 60, 12);
    lid.cubicTo(44, 16, 22, 22, 6, 30);
    p.setBrush(Qt::white);
    p.drawPath(lid);
    QPolygonF spur;
    spur << QPointF(36, 36) << QPointF(52, 60) << QPointF(40, 38);
    p.drawPolygon(spur);
    p.end();
    return QIcon(pm);
}

QIcon makeTrayIcon()
{
    QIcon icon = loadAppIcon();
    if (!icon.isNull()) {
        return icon;
    }
    GAZER_WARN << "App icon assets missing; using fallback tray glyph";
    return makeFallbackTrayIcon();
}

} // namespace

TrayIcon::TrayIcon(QObject* parent)
    : QObject(parent)
{
    m_available = QSystemTrayIcon::isSystemTrayAvailable();
    if (!m_available) {
        GAZER_WARN << "System tray is not available on this desktop";
        return;
    }

    m_menu = new QMenu();
    auto* layoutAction = m_menu->addAction(QStringLiteral("Show pages"));
    auto* previewAction = m_menu->addAction(QStringLiteral("Show preview"));
    auto* editorAction = m_menu->addAction(QStringLiteral("Page editor"));
    m_menu->addSeparator();
    auto* quitAction = m_menu->addAction(QStringLiteral("Quit Gazer"));

    connect(layoutAction, &QAction::triggered, this, &TrayIcon::showLayoutRequested);
    connect(previewAction, &QAction::triggered, this, &TrayIcon::showPreviewRequested);
    connect(editorAction, &QAction::triggered, this, &TrayIcon::layoutEditorRequested);
    connect(quitAction, &QAction::triggered, this, &TrayIcon::quitRequested);

    m_tray = new QSystemTrayIcon(makeTrayIcon(), this);
    m_tray->setContextMenu(m_menu);
    m_tray->setToolTip(QStringLiteral("Gazer"));
    m_tray->show();

    connect(m_tray, &QSystemTrayIcon::activated, this, &TrayIcon::onActivated);
    GAZER_INFO << "System tray icon shown";
}

TrayIcon::~TrayIcon()
{
    if (m_tray) {
        m_tray->hide();
    }
    delete m_menu;
    m_menu = nullptr;
}

bool TrayIcon::isAvailable() const
{
    return m_available;
}

bool TrayIcon::isVisible() const
{
    return m_tray && m_tray->isVisible();
}

void TrayIcon::setStatus(const QString& tooltip)
{
    if (m_tray) {
        m_tray->setToolTip(QStringLiteral("Gazer — %1").arg(tooltip));
    }
}

void TrayIcon::hideIcon()
{
    if (m_tray) {
        m_tray->hide();
    }
}

void TrayIcon::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger
        || reason == QSystemTrayIcon::DoubleClick) {
        emit showLayoutRequested();
    }
}

} // namespace gazer
