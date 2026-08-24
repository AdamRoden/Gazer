#pragma once

#include "core/GazePoint.h"
#include "layout/DwellStateMachine.h"
#include "layout/PageHit.h"
#include "layout/PageTypes.h"
#include "ui/PageHostWindow.h"
#include "ui/ProgressVisuals.h"
#include "ui/Theme.h"

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QScreen>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QVariantMap>
#include <QVector>
#include <functional>
#include <memory>

namespace gazer {

class PageCatalog;

class PageSession final : public QObject {
    Q_OBJECT

public:
    using DispatchFn = std::function<void(const QVector<PageAction>& actions, const QString& pageId,
                                          const QString& targetId)>;
    using DecorateFn = std::function<void(PageDocument&)>;
    using ActiveFn = std::function<bool(const QString& activeStateKey)>;
    using LoopToggleFn = std::function<bool(const PageTarget& t, const QString& pageId)>;
    using LoopLatchFn = std::function<void()>;
    using LoopStopPageFn = std::function<void(const QString& pageId)>;

    enum class RootChrome { Docked, Drawer, Quit };

    explicit PageSession(QObject* parent = nullptr);

    [[nodiscard]] bool openRoot(const QString& xmlPath, QString* error = nullptr);
    void setLayoutsDirectory(const QString& dir) { m_layoutsDir = dir; }
    void setCatalog(PageCatalog* catalog) { m_catalog = catalog; }
    [[nodiscard]] bool openPage(const QString& id, QString* error = nullptr);
    /// Put the attached page's center on @p screenCenter. Size stays as authored.
    /// @p leaveGate: do not activate the cell under gaze until gaze leaves it.
    [[nodiscard]] bool placeAttachedCenter(const QString& id, const QPoint& screenCenter,
                                           bool leaveGate = true);
    void closePage(const QString& id);
    int closeAttached();
    [[nodiscard]] bool hasRoot() const { return m_root.isValid(); }
    [[nodiscard]] bool hasPage(const QString& id) const;
    [[nodiscard]] const PageDocument& root() const { return m_root; }
    [[nodiscard]] PageHostWindow* window() const { return m_host.get(); }
    [[nodiscard]] const QVector<PageTarget>& targets() const { return m_targets; }
    [[nodiscard]] const QVector<PageGridPaint>& gridPaints() const { return m_gridPaints; }
    [[nodiscard]] double drawerScale() const { return m_drawerScale; }
    /// Attach or refresh an in-memory page. decorate=true runs the session decorator.
    [[nodiscard]] bool attachDocument(PageDocument doc, QString* error = nullptr,
                                      bool decorate = false);
    void registerMemoryPage(PageDocument doc);
    void closePreviewPages();
    static QString previewId(const QString& catalogId);
    static bool isPreviewId(const QString& id);

    void setDispatch(DispatchFn fn) { m_dispatch = std::move(fn); }
    void setDecorate(DecorateFn fn) { m_decorate = std::move(fn); }
    void setActiveResolver(ActiveFn fn) { m_active = std::move(fn); }
    void setLoopToggle(LoopToggleFn fn) { m_loopToggle = std::move(fn); }
    void setLoopLatchClear(LoopLatchFn fn) { m_loopLatchClear = std::move(fn); }
    void setLoopStopPage(LoopStopPageFn fn) { m_loopStopPage = std::move(fn); }
    void refreshDecorated();
    void refreshActive();

    void setTheme(const ThemeColors& theme);
    void setProgressVisuals(const ProgressVisuals& visuals);
    void setGlobalDwell(const QVector<int>& sequence, int graceMs, int scanGraceMs);

    [[nodiscard]] RootChrome rootChrome() const { return m_chrome; }
    [[nodiscard]] bool isExpanded() const { return m_chrome != RootChrome::Docked; }
    void setAutoCollapseMain(bool on) { m_autoCollapseMain = on; }
    void setDwellSuspended(bool on);
    void toggleDwellSuspended() { setDwellSuspended(!m_dwellSuspended); }
    [[nodiscard]] bool isDwellSuspended() const { return m_dwellSuspended; }
    [[nodiscard]] int openCount() const { return hasRoot() ? 1 + m_attached.size() : 0; }
    [[nodiscard]] QString topPageId() const;

    bool applyPageAction(PageVerb verb, PageTargetKind kind, const QString& id,
                         QString* error = nullptr);
    bool applyNav(const PageAction& action, const QString& sourcePageId,
                  const QString& sourceTargetId, QString* error = nullptr);
    [[nodiscard]] bool goBack(QString* error = nullptr);

    [[nodiscard]] bool onGaze(const GazePoint& point);
    /// Hit-test only: true if gaze is over a board/cell/zone (does not run dwell).
    [[nodiscard]] bool hitsChrome(const GazePoint& point) const;
    /// True if @p pos is inside a painted grid of page @p id (including shell grids).
    [[nodiscard]] bool hitsPage(const QString& id, const QPointF& pos) const;
    void leaveGaze();
    void raise();
    void hideHost();

    void activateTarget(const QString& targetId);
    [[nodiscard]] QVector<QRect> unpauseGapRects() const;

signals:
    void sessionChanged();
    void dwellSuspendChanged(bool suspended);
    void targetActivated(const QString& pageId, const QString& targetId);

private:
    [[nodiscard]] PageFrame frame() const;
    void rebuild();
    void applyDwellFor(const PageTarget* t);
    void setRootChrome(RootChrome next, bool animate = true);
    [[nodiscard]] RootChrome chromeForSlot(PageRootSlot slot) const;
    [[nodiscard]] QSet<QString> hiddenRootGrids() const;
    [[nodiscard]] const PageTarget* findTarget(const QString& id) const;
    [[nodiscard]] QString xmlPathFor(const QString& id) const;
    void ingest(const PageDocument& doc, const QSet<QString>& hiddenGrids,
                const QSet<QString>& hiddenZones, QVector<PageTarget>& rest,
                QVector<PageTarget>& shellLayer, QVector<PageGridPaint>& restGrids,
                QVector<PageGridPaint>& shellGrids);
    void armLeaveGate(const QString& pageId);
    void clearLeaveGate();
    [[nodiscard]] bool blockedByLeaveGate(const PageTarget* hit);
    void noteActivity();
    void tickAutoClose();
    [[nodiscard]] int autoCloseIdleMs() const;
    void syncDrawerScale();

    struct AttachedPage {
        PageDocument doc;
    };

    struct PageBreadcrumb {
        QVector<PageDocument> attached;
        RootChrome chrome = RootChrome::Docked;
        QSet<QString> hiddenZones;
    };

    [[nodiscard]] PageBreadcrumb captureBreadcrumb() const;
    void restoreBreadcrumb(PageBreadcrumb snap);
    [[nodiscard]] bool applyNavMutation(const PageAction& action, const QString& sourcePageId,
                                        const QString& sourceTargetId, QString* error);
    [[nodiscard]] bool applyNavPage(PageVerb verb, PageNavScope scope, const QString& id,
                                    const QString& sourcePageId, QString* error);
    [[nodiscard]] bool applyNavGrid(PageVerb verb, PageNavScope scope, const QString& id,
                                    QString* error);
    [[nodiscard]] bool applyNavZone(PageVerb verb, PageNavScope scope, const QString& id,
                                    const QString& sourceTargetId, QString* error);
    void closePagesExcept(const QString& keepId);
    [[nodiscard]] const PageZone* findZoneAnywhere(const QString& id) const;

    PageDocument m_root;
    QVector<PageBreadcrumb> m_crumbs;
    QString m_layoutsDir;
    QHash<QString, PageDocument> m_memory;
    QVector<AttachedPage> m_attached;
    QSet<QString> m_hiddenZones;
    QVariantMap m_props;
    bool m_dwellSuspended = false;
    bool m_autoCollapseMain = false;
    QVector<PageTarget> m_targets;
    QVector<PageGridPaint> m_gridPaints;
    std::unique_ptr<PageHostWindow> m_host;
    DwellStateMachine m_dwell;
    QVector<int> m_globalSequence = {800};
    int m_globalGraceMs = 180;
    int m_globalScanGraceMs = 100;
    QString m_hoverId;
    DispatchFn m_dispatch;
    DecorateFn m_decorate;
    ActiveFn m_active;
    LoopToggleFn m_loopToggle;
    LoopLatchFn m_loopLatchClear;
    LoopStopPageFn m_loopStopPage;
    QTimer m_autoCloseTimer;
    QElapsedTimer m_idleClock;
    bool m_idleClockRunning = false;
    QVector<QPointer<QScreen>> m_boundScreens;
    PageCatalog* m_catalog = nullptr;
    GazePoint m_lastGaze;
    QString m_leaveGatePage;
    QString m_leaveGateKey;

    RootChrome m_chrome = RootChrome::Docked;
    QTimer m_drawerTimer;
    QElapsedTimer m_drawerClock;
    enum class DrawerPhase { Idle, Appear, Dismiss };
    DrawerPhase m_drawerPhase = DrawerPhase::Idle;
    double m_drawerScale = 1.0;
    static constexpr double kDrawerMinScale = 0.05;
    static constexpr int kDrawerAppearMs = 420;
    static constexpr int kDrawerDismissMs = 500;

    void playDrawerAppear();
    void playDrawerDismiss();
    void tickDrawer();
};

} // namespace gazer
