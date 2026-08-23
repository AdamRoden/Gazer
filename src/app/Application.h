#pragma once

#include "app/ActionDispatcher.h"
#include "app/GazeRouter.h"
#include "app/GazerServices.h"
#include "core/ITracker.h"
#include "ui/DwellSuspendOverlay.h"
#include "layout/PageTypes.h"
#include "ui/PreviewWindow.h"
#include "ui/TrayIcon.h"

#include <QObject>
#include <QVector>
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
    void updateTrayStatus();
    void syncDwellSuspendOverlay();
    [[nodiscard]] bool expandMasterShell(QString* error = nullptr);

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
    std::unique_ptr<class LayoutEditorWindow> m_editor;
    std::unique_ptr<TrayIcon> m_tray;
    bool m_restacking = false;
};

} // namespace gazer
