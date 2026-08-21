#pragma once

#include "core/GazePoint.h"
#include "layout/InstanceTarget.h"
#include "layout/LayoutInstance.h"
#include "layout/LayoutManager.h"

#include "ui/EdgeBubbleOverlay.h"
#include "ui/ProgressVisuals.h"
#include "ui/Theme.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVector>
#include <functional>
#include <memory>
#include <vector>

namespace gazer {

/// Multi-instance session. One never-destroyed root (master); children show/hide.
///
/// Emit policy: each public mutator fires at most one sessionChanged at the end.
class LayoutInstanceManager final : public QObject {
    Q_OBJECT

public:
    explicit LayoutInstanceManager(LayoutManager& catalog, QObject* parent = nullptr);

    /// Product policy for loadLayout actions / gazer.loadLayout (single implementation).
    [[nodiscard]] bool applyLoadLayout(const QString& sourceInstanceId, const QString& layoutId,
                                       QString* error = nullptr);

    /// Load layout into an existing instance (strict replace). Fails on the root.
    [[nodiscard]] bool loadInto(const QString& instanceId, const QString& layoutId,
                                QString* error = nullptr);

    /// Open a secondary board. Hard-fails if layout is the root master.
    [[nodiscard]] QString openSecondary(const QString& layoutId, QString* error = nullptr);

    /// Layout-editor live preview. Does not write the catalog. Replaces the previous preview.
    [[nodiscard]] QString openEditorPreview(LayoutDocument doc, QString* error = nullptr);
    /// Preview the current layer and register sibling layers so loadLayout swaps stay in-editor.
    [[nodiscard]] QString openEditorPreview(const QVector<LayoutDocument>& family, int currentIndex,
                                            QString* error = nullptr);

    /// Show the home child and raise it.
    void raiseMaster();

    void setRootProperty(const QString& key, const QVariant& value);
    [[nodiscard]] QVariant rootProperty(const QString& key) const;
    [[nodiscard]] QVariantMap rootProperties() const { return m_rootProps; }
    [[nodiscard]] bool expandHome(QString* error = nullptr);
    void collapseHome();

    /// Close every non-master instance (one sessionChanged).
    int closeOtherViews();

    /// Close one secondary instance.
    [[nodiscard]] bool closeInstance(const QString& instanceId, QString* error = nullptr);

    /// Bootstrap / openLayout: master → open/keep root, declared child → show, else secondary.
    [[nodiscard]] QString openInstance(const QString& layoutId, QString* error = nullptr);

    /// Create the never-destroyed root and its declared children.
    [[nodiscard]] bool openMaster(const QString& layoutId, QString* error = nullptr);

    void focusInstance(const QString& instanceId);

    /// When true, opening a secondary while master is home collapses master first.
    void setAutoCollapseMain(bool enabled) { m_autoCollapseMain = enabled; }
    [[nodiscard]] bool autoCollapseMain() const { return m_autoCollapseMain; }

    /// Global auto-close defaults for secondaries (master shells ignore unless layout forces).
    void setAutoCloseDefaults(bool enabled, int idleMs, int fadeMs);
    [[nodiscard]] bool autoCloseEnabled() const { return m_autoCloseEnabled; }
    [[nodiscard]] int autoCloseIdleMs() const { return m_autoCloseIdleMs; }
    [[nodiscard]] int autoCloseFadeMs() const { return m_autoCloseFadeMs; }

    /// Apply global dwell sequence / grace / scan-grace to every open instance.
    void applyGlobalDwellOverride(const QVector<int>& dwellSequence, int graceMs,
                                  int scanGraceMs = DwellStateMachine::kDefaultScanGraceMs);

    void applyProgressVisuals(const ProgressVisuals& visuals);
    void applyTheme(const ThemeColors& theme);

    /// Recompute active-state accents for every open board using @p resolver(activeStateKey).
    using ActiveStateResolver = std::function<bool(const QString& activeStateKey)>;
    void refreshActiveIndicators(const ActiveStateResolver& resolver);

    void setEdgeBubbleOverlay(EdgeBubbleOverlay* overlay);

    /// Suspend dwell/click capture on all boards except isDwellExempt items.
    void setDwellSuspended(bool suspended);
    [[nodiscard]] bool isDwellSuspended() const { return m_dwellSuspended; }
    void toggleDwellSuspended() { setDwellSuspended(!m_dwellSuspended); }

    /// Replace instance document in-place (e.g. dynamic numeric editor board).
    [[nodiscard]] bool setInstanceDocument(const QString& instanceId, LayoutDocument doc,
                                           QString* error = nullptr);

    /// Optional hook to decorate layouts as they load (settings captions, etc.).
    using DocumentDecorator = std::function<void(LayoutDocument&)>;
    void setDocumentDecorator(DocumentDecorator decorator);

    /// Run layout lifecycle action arrays (onOpen / onLoad / onClose).
    using LifecycleRunner =
        std::function<void(const QVector<LayoutAction>& actions, const QString& instanceId)>;
    void setLifecycleRunner(LifecycleRunner runner) { m_lifecycleRunner = std::move(runner); }

    /// Called when an action-loop owner instance closes (stop loops).
    using InstanceTeardownHook = std::function<void(const QString& instanceId)>;
    void setInstanceTeardownHook(InstanceTeardownHook hook)
    {
        m_instanceTeardown = std::move(hook);
    }

    [[nodiscard]] QString focusedInstanceId() const { return m_focusedId; }
    [[nodiscard]] QString masterInstanceId() const { return m_masterId; }
    [[nodiscard]] LayoutInstance* focusedInstance() const;
    [[nodiscard]] LayoutInstance* masterInstance() const;
    [[nodiscard]] LayoutInstance* instance(const QString& instanceId) const;
    [[nodiscard]] QVector<LayoutInstance*> instances() const;
    [[nodiscard]] int count() const { return static_cast<int>(m_instances.size()); }
    /// Visible board windows (hidden children and the headless root are omitted).
    [[nodiscard]] int visibleBoardCount() const;

    [[nodiscard]] QVector<InstanceTarget> otherInstanceTargets() const;

    [[nodiscard]] bool onGaze(const GazePoint& point);
    void leaveActiveGaze();

    /// Raise HWND for the hit-stack top (last instance) so visual z-order matches gaze.
    void reassertStackTopVisual();

    /// HWND_TOPMOST for every visible above-taskbar board (no animation restart).
    void restackChrome();

    void hideAll();
    void shutdown();

signals:
    void instanceOpened(const QString& instanceId, const QString& layoutId);
    void instanceClosed(const QString& instanceId);
    void instanceFocused(const QString& instanceId);
    void itemActivated(const QString& instanceId, const QString& itemId);
    void dwellEngagementEnded(const QString& instanceId, const QString& itemId);
    void dwellSuspendChanged(bool suspended);
    void sessionChanged();

private:
    QString makeInstanceId(const QString& layoutId);
    /// Focus only. Emits instanceFocused if changed. Never emits sessionChanged.
    /// When raiseWindow is false, only updates focus id — does NOT reorder hit stack.
    void setFocused(const QString& instanceId, bool raiseWindow);
    /// Remove a secondary without signals (caller owns notify).
    [[nodiscard]] bool eraseSecondary(const QString& instanceId, QString* error);
    void onWindowCloseRequested(const QString& instanceId);
    [[nodiscard]] LayoutInstance* findInstanceAt(const QPointF& screenPoint) const;
    [[nodiscard]] bool applyDocument(LayoutInstance* inst, const QString& layoutId,
                                     QString* error);
    /// In-place document swap: onClose + stop loops, then setDocument + onLoad.
    void replaceInstanceDocument(LayoutInstance* inst, LayoutDocument newDoc, bool fireOnLoad);
    [[nodiscard]] const LayoutDocument* requireDoc(const QString& layoutId, QString* error);
    enum class RootChrome { Docked, Drawer, Quit };

    [[nodiscard]] QString homeLayoutIdForMaster() const;
    void wireInstance(LayoutInstance* inst);
    void applyInstanceChrome(LayoutInstance* inst);
    [[nodiscard]] std::unique_ptr<LayoutInstance> makeWiredInstance(const QString& layoutId,
                                                                   const LayoutDocument& doc);
    [[nodiscard]] bool isRootChildLayout(const QString& layoutId) const;
    void applyChromeProps();
    void setRootChrome(RootChrome next);
    void syncChildVisibility();
    [[nodiscard]] bool isDeclaredChildLayout(const QString& layoutId) const;
    [[nodiscard]] bool isChildInstance(const QString& instanceId) const;
    [[nodiscard]] LayoutChildRef childSpecForLayout(const QString& layoutId) const;
    [[nodiscard]] LayoutInstance* homeInstance() const;
    [[nodiscard]] LayoutInstance* quitInstance() const;
    bool showChildLayout(const QString& layoutId, QString* error);
    QString spawnChild(const LayoutChildRef& child, QString* error);
    [[nodiscard]] qint64 nowMs() const;

    LayoutManager& m_catalog;
    std::vector<std::unique_ptr<LayoutInstance>> m_instances;
    QString m_focusedId;
    QString m_masterId;
    QString m_gazeInstanceId;
    QVariantMap m_rootProps;
    /// child slot id → instance id
    QHash<QString, QString> m_childInstanceBySlot;
    /// layout id → instance id for declared children
    QHash<QString, QString> m_childInstanceByLayout;
    int m_nextSerial = 1;
    bool m_autoCollapseMain = true;
    bool m_autoCloseEnabled = true;
    int m_autoCloseIdleMs = 10000;
    int m_autoCloseFadeMs = 3000;
    QVector<int> m_globalDwellSequence;
    int m_globalGraceMs = 0;
    int m_globalScanGraceMs = DwellStateMachine::kDefaultScanGraceMs;
    ProgressVisuals m_progressVisuals;
    ThemeColors m_theme = ThemeColors::darkPreset();
    EdgeBubbleOverlay* m_edgeBubbles = nullptr;
    DocumentDecorator m_documentDecorator;
    LifecycleRunner m_lifecycleRunner;
    InstanceTeardownHook m_instanceTeardown;
    QString m_editorPreviewId;
    bool m_dwellSuspended = false;
    RootChrome m_rootChrome = RootChrome::Docked;
    bool m_homeDismissing = false;
    LayoutDocument decorateCopy(const LayoutDocument& src) const;
    void fireLifecycle(const QVector<LayoutAction>& actions, const QString& instanceId) const;
    void applyAutoClosePolicy(LayoutInstance* inst);
    void noteDwellActivity(const QString& instanceId);
    void tickAutoClose(qint64 nowMs);
};

} // namespace gazer
