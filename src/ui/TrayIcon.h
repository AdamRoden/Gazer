#pragma once

#include <QObject>
#include <QSystemTrayIcon>

class QMenu;

namespace gazer {

/// System tray: show layout, show preview, quit.
class TrayIcon final : public QObject {
    Q_OBJECT

public:
    explicit TrayIcon(QObject* parent = nullptr);
    ~TrayIcon() override;

    [[nodiscard]] bool isAvailable() const;
    [[nodiscard]] bool isVisible() const;

    void setStatus(const QString& tooltip);
    void hideIcon();

signals:
    void showLayoutRequested();
    void showPreviewRequested();
    void layoutEditorRequested();
    void quitRequested();

private:
    void onActivated(QSystemTrayIcon::ActivationReason reason);

    QSystemTrayIcon* m_tray = nullptr;
    QMenu* m_menu = nullptr;
    bool m_available = false;
};

} // namespace gazer
