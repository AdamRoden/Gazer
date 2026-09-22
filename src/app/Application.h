#pragma once

#include "app/GazeRouter.h"
#include "layout/PageTypes.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QVector>
#include <memory>

namespace gazer {

class ActionChannel;
class ActionDispatcher;
class DwellSuspendOverlay;
class GazerServices;
class Heartbeat;
class ITracker;
class LayoutEditorWindow;
class OverlayStackWatch;
class HeadPreviewRenderer;
class PreviewWindow;
class SessionWatch;
class SplashOverlay;
class TrayIcon;

/// Shell: tracker + tray/preview/overlays over GazerServices.
class Application final : public QObject {
    Q_OBJECT

public:
    explicit Application(QObject* parent = nullptr);
    ~Application() override;

    [[nodiscard]] bool initialize();
    void takeInbound(std::unique_ptr<ActionChannel> channel);
    void runInbound(const QString& text);

private:
    [[nodiscard]] bool startTracker();
    void wireTracker();
    void updateTrayStatus();
    void syncDwellSuspendOverlay();

    void onQuitRequested();
    void startSplash();
    void onSplashFinished();
    void syncSplashChrome();
    void showMasterLayers(const QVector<int>& layers);
    void openPageEditor(const QString& pageId = {});
    [[nodiscard]] bool testEditedLayout(const PageDocument& doc, QString* error);
    void onGaze(const gazer::GazePoint& point);
    void updateHeadPosePaint();
    void onTobiiStreamFailed(const QString& reason);
    void fallbackToMouse();
    void shutdownUi();
    void rescueReset();
    void pulseHeartbeat();
    void onInjectPaused(bool paused);

    std::unique_ptr<GazerServices> m_svc;
    std::unique_ptr<ActionDispatcher> m_actions;
    std::unique_ptr<ActionChannel> m_inbound;
    std::unique_ptr<ITracker> m_tracker;
    GazeRouter m_gazeRouter;
    std::unique_ptr<SplashOverlay> m_splash;
    bool m_splashSavedMag = false;
    std::unique_ptr<DwellSuspendOverlay> m_dwellSuspendOverlay;
    std::unique_ptr<PreviewWindow> m_preview;
    std::unique_ptr<HeadPreviewRenderer> m_headPreviewGl;
    QElapsedTimer m_headPaintClock;
    std::unique_ptr<LayoutEditorWindow> m_editor;
    std::unique_ptr<TrayIcon> m_tray;
    std::unique_ptr<OverlayStackWatch> m_stackWatch;
    std::unique_ptr<Heartbeat> m_heartbeat;
    std::unique_ptr<SessionWatch> m_sessionWatch;
    QTimer m_pulse;
    bool m_safeMode = false;
};

} // namespace gazer
