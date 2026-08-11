#include "ui/TrayIcon.h"

#include "ui/AppIcon.h"
#include "utils/Log.h"

#include <QAction>
#include <QIcon>
#include <QMenu>
#include <QPainter>
#include <QPixmap>

namespace gazer {

namespace {

QIcon makeFallbackTrayIcon()
{
    QPixmap pm(64, 64);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(QColor(0xDD, 0xB7, 0x37));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(4, 4, 56, 56, 12, 12);
    p.setBrush(QColor(0x06, 0x15, 0x2E));
    p.drawEllipse(18, 18, 28, 28);
    p.setBrush(QColor(0x77, 0x8E, 0xA4));
    p.drawEllipse(26, 26, 12, 12);
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
    auto* layoutAction = m_menu->addAction(QStringLiteral("Show layout"));
    auto* previewAction = m_menu->addAction(QStringLiteral("Show preview"));
    m_menu->addSeparator();
    auto* quitAction = m_menu->addAction(QStringLiteral("Quit Gazer"));

    connect(layoutAction, &QAction::triggered, this, &TrayIcon::showLayoutRequested);
    connect(previewAction, &QAction::triggered, this, &TrayIcon::showPreviewRequested);
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
