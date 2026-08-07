#pragma once

#include "app/ActionDispatcher.h"
#include "app/GazeRouter.h"
#include "app/GazerServices.h"
#include "core/ITracker.h"
#include "ui/DockRevealOverlay.h"
#include "ui/DwellSuspendOverlay.h"
#include "ui/EdgeBubbleOverlay.h"
#include "ui/PreviewWindow.h"
#include "ui/TrayIcon.h"

#include <QObject>
#include <memory>

namespace gazer {

/// Shell: tracker + tray/preview/overlays over GazerServices.
class Application final : public QObject {
    Q_OBJECT

public:
    explicit Application(QObject* parent = nullptr);
    ~Application() override;

    [[nodiscard]] bool initialize();

private:
    [[nodiscard]] bool startTracker();
    void wireTracker();
    void syncMasterChrome();
    void syncDwellSuspendOverlay();
    /// Navigate master to home (expandLayoutId) and force-show. Used by Main ▶.
    [[nodiscard]] bool expandMasterShell(QString* error = nullptr);

    void onQuitRequested();
    void onGaze(const gazer::GazePoint& point);
    void onItemActivated(const QString& instanceId, const QString& itemId);
    void onTobiiStreamFailed(const QString& reason);
    void fallbackToMouse();
    void shutdownUi();

    std::unique_ptr<GazerServices> m_svc;
    std::unique_ptr<ActionDispatcher> m_actions;
    std::unique_ptr<ITracker> m_tracker;
    GazeRouter m_gazeRouter;
    std::unique_ptr<DockRevealOverlay> m_dockReveal;
    std::unique_ptr<EdgeBubbleOverlay> m_edgeBubbles;
    std::unique_ptr<DwellSuspendOverlay> m_dwellSuspendOverlay;
    std::unique_ptr<PreviewWindow> m_preview;
    std::unique_ptr<TrayIcon> m_tray;
    /// Collapsed dock has been revealed this dock session (not re-hidden until expand).
    bool m_dockRevealed = false;
};

} // namespace gazer
