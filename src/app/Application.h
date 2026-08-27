#pragma once

#include "app/GazeRouter.h"
#include "layout/PageTypes.h"

#include <QObject>
#include <QVector>
#include <memory>

namespace gazer {

class ActionDispatcher;
class DwellSuspendOverlay;
class GazerServices;
class ITracker;
class LayoutEditorWindow;
class OverlayStackWatch;
class PreviewWindow;
class TrayIcon;

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
    void updateTrayStatus();
    void syncDwellSuspendOverlay();

    void onQuitRequested();
    void openPageEditor(const QString& pageId = {});
    [[nodiscard]] bool testEditedLayout(const QVector<PageDocument>& family, int currentIndex,
                                        QString* error);
    void onGaze(const gazer::GazePoint& point);
    void onTobiiStreamFailed(const QString& reason);
    void fallbackToMouse();
    void shutdownUi();

    std::unique_ptr<GazerServices> m_svc;
    std::unique_ptr<ActionDispatcher> m_actions;
    std::unique_ptr<ITracker> m_tracker;
    GazeRouter m_gazeRouter;
    std::unique_ptr<DwellSuspendOverlay> m_dwellSuspendOverlay;
    std::unique_ptr<PreviewWindow> m_preview;
    std::unique_ptr<LayoutEditorWindow> m_editor;
    std::unique_ptr<TrayIcon> m_tray;
    std::unique_ptr<OverlayStackWatch> m_stackWatch;
    bool m_restacking = false;
};

} // namespace gazer
