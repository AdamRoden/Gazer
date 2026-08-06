#pragma once

#include "core/GazePoint.h"
#include "layout/InstanceTarget.h"
#include "layout/LayoutInstance.h"
#include "layout/LayoutManager.h"

#include "ui/EdgeBubbleOverlay.h"
#include "ui/ProgressVisuals.h"

#include <QObject>
#include <QString>
#include <QVector>
#include <functional>
#include <memory>
#include <vector>

namespace gazer {

/// Multi-instance session with explicit navigation verbs.
/// Master shells use LayoutSessionMeta (role + masterGroup), never content filenames.
///
/// Emit policy: each public mutator fires at most one sessionChanged at the end.
class LayoutInstanceManager final : public QObject {
    Q_OBJECT

public:
    explicit LayoutInstanceManager(LayoutManager& catalog, QObject* parent = nullptr);

    /// Product policy for loadLayout actions / gazer.loadLayout (single implementation).
    [[nodiscard]] bool applyLoadLayout(const QString& sourceInstanceId, const QString& layoutId,
                                       QString* error = nullptr);

    /// Load layout into an existing instance (strict replace). Fails if roles conflict.
    [[nodiscard]] bool loadInto(const QString& instanceId, const QString& layoutId,
                                QString* error = nullptr);

    /// Transition the unique master instance for this layout's masterGroup.
    [[nodiscard]] bool navigateMaster(const QString& layoutId, QString* error = nullptr);

    /// Open a secondary board. Hard-fails if layout is masterShell.
    [[nodiscard]] QString openSecondary(const QString& layoutId, QString* error = nullptr);

    /// Focus/raise master; optionally close the secondary that requested return.
    [[nodiscard]] bool returnToMaster(const QString& fromInstanceId, bool closeSecondary,
                                      QString* error = nullptr);

    /// Expand master home (if docked) and raise it.
    void raiseMaster();

    /// Close every non-master instance (one sessionChanged).
    int closeOtherViews();

    /// Close one secondary instance.
    [[nodiscard]] bool closeInstance(const QString& instanceId, QString* error = nullptr);

    /// Bootstrap / openLayout: masterShell → navigateMaster, else openSecondary.
    [[nodiscard]] QString openInstance(const QString& layoutId, QString* error = nullptr);

    void focusInstance(const QString& instanceId);

    /// When true, opening a secondary while master is home collapses master first.
    void setAutoCollapseMain(bool enabled) { m_autoCollapseMain = enabled; }
    [[nodiscard]] bool autoCollapseMain() const { return m_autoCollapseMain; }

    /// Apply global dwell sequence to every open instance.
    void applyGlobalDwellOverride(const QVector<int>& dwellSequence, int graceMs);

    void applyProgressVisuals(const ProgressVisuals& visuals);

    /// Recompute active-state accents for every open board using @p resolver(activeStateKey).
    using ActiveStateResolver = std::function<bool(const QString& activeStateKey)>;
    void refreshActiveIndicators(const ActiveStateResolver& resolver);

    void setEdgeBubbleOverlay(EdgeBubbleOverlay* overlay);

    /// Replace instance document in-place (e.g. dynamic numeric editor board).
    [[nodiscard]] bool setInstanceDocument(const QString& instanceId, LayoutDocument doc,
                                           QString* error = nullptr);

    /// Optional hook to decorate layouts as they load (settings captions, etc.).
    using DocumentDecorator = std::function<void(LayoutDocument&)>;
    void setDocumentDecorator(DocumentDecorator decorator);

    [[nodiscard]] QString focusedInstanceId() const { return m_focusedId; }
    [[nodiscard]] QString masterInstanceId() const { return m_masterId; }
    [[nodiscard]] LayoutInstance* focusedInstance() const;
    [[nodiscard]] LayoutInstance* masterInstance() const;
    [[nodiscard]] LayoutInstance* instance(const QString& instanceId) const;
    [[nodiscard]] QVector<LayoutInstance*> instances() const;
    [[nodiscard]] int count() const { return static_cast<int>(m_instances.size()); }

    [[nodiscard]] QVector<InstanceTarget> otherInstanceTargets() const;

    [[nodiscard]] bool onGaze(const GazePoint& point);
    void leaveActiveGaze();

    /// Raise HWND for the hit-stack top (last instance) so visual z-order matches gaze.
    void reassertStackTopVisual();

    void hideAll();
    void shutdown();

signals:
    void instanceOpened(const QString& instanceId, const QString& layoutId);
    void instanceClosed(const QString& instanceId);
    void instanceFocused(const QString& instanceId);
    void itemActivated(const QString& instanceId, const QString& itemId);
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
    [[nodiscard]] const LayoutDocument* requireDoc(const QString& layoutId, QString* error);
    [[nodiscard]] QString homeLayoutIdForMaster() const;
    void wireInstance(LayoutInstance* inst);

    LayoutManager& m_catalog;
    std::vector<std::unique_ptr<LayoutInstance>> m_instances;
    QString m_focusedId;
    QString m_masterId;
    QString m_masterGroup;
    QString m_gazeInstanceId;
    int m_nextSerial = 1;
    bool m_autoCollapseMain = true;
    QVector<int> m_globalDwellSequence;
    int m_globalGraceMs = 0;
    ProgressVisuals m_progressVisuals;
    EdgeBubbleOverlay* m_edgeBubbles = nullptr;
    DocumentDecorator m_documentDecorator;
    LayoutDocument decorateCopy(const LayoutDocument& src) const;
};

} // namespace gazer
