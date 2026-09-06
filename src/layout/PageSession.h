#pragma once

#include "core/GazePoint.h"
#include "layout/DwellStateMachine.h"
#include "layout/PageHit.h"
#include "layout/PageNav.h"
#include "layout/PageTypes.h"

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QScreen>
#include <QString>
#include <QTimer>
#include <QVariantMap>
#include <QVector>
#include <functional>
#include <memory>

namespace gazer {

class PageCatalog;
class PageHostWindow;
struct ProgressVisuals;
struct ThemeColors;

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

    explicit PageSession(QObject* parent = nullptr);
    ~PageSession() override;

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
    /// Attach or replace an in-memory page. decorate=true runs the session decorator.
    /// A new page always comes to front. Replacing a buried page restacks it unless
    /// `restack` is false (live refresh of a page behind a modal). Replacing the
    /// already-top page is a live refresh: dwell sequence continues.
    [[nodiscard]] bool attachDocument(PageDocument doc, QString* error = nullptr,
                                      bool decorate = false, bool restack = true);
    [[nodiscard]] PageDocument attachedCopy(const QString& id) const;
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
    void setShiftHeld(bool on);

    void setTheme(const ThemeColors& theme);
    void setProgressVisuals(const ProgressVisuals& visuals);
    void setGlobalDwell(const QVector<int>& sequence, int graceMs, int scanGraceMs);
    void setDailyDriverDwell(const QVector<int>& sequence, int scanGraceMs);

    void setAutoCollapseMain(bool on) { m_autoCollapseMain = on; }
    void setLayoutAutoClose(bool on, int idleMs);
    void setDwellSuspended(bool on);
    void toggleDwellSuspended() { setDwellSuspended(!m_dwellSuspended); }
    [[nodiscard]] bool isDwellSuspended() const { return m_dwellSuspended; }
    [[nodiscard]] int openCount() const { return hasRoot() ? 1 + m_attached.size() : 0; }
    [[nodiscard]] QString topPageId() const;

    bool applyNav(const PageAction& action, const QString& sourcePageId,
                  const QString& sourceTargetId, QString* error = nullptr);
    /// Consecutive ShowLayers mutations, then one drawer reconcile.
    bool showLayers(const QVector<PageAction>& actions, const QString& sourcePageId,
                    const QString& sourceTargetId, QString* error = nullptr);
    [[nodiscard]] bool goBack(QString* error = nullptr);

    /// All boards, or only the master page plus the cell that armed mouse-dwell-move.
    enum class GazeScope { All, MasterAndActivator };

    /// One hit-test. `target` is valid until the next rebuild.
    struct GazeHit {
        const PageTarget* target = nullptr;
        bool overBoard = false;
        bool overMaster = false;
        bool overActivator = false;
    };

    [[nodiscard]] GazeHit classifyGaze(const GazePoint& point) const;
    /// Apply dwell using a prior `classifyGaze` result (no second hit-test).
    bool feedGaze(const GazePoint& point, const GazeHit& classified,
                  GazeScope scope = GazeScope::All);
    [[nodiscard]] bool onGaze(const GazePoint& point, GazeScope scope = GazeScope::All);
    /// Hit-test only: true if gaze is over a board/cell/zone (does not run dwell).
    [[nodiscard]] bool hitsChrome(const GazePoint& point) const;
    /// Screen rect of a live target (page id + cell/zone id, or a session key).
    [[nodiscard]] QRect targetScreenRect(const QString& pageId, const QString& targetId) const;
    /// Cell that armed mouse-dwell-move; still dwellable so the action can be cancelled.
    void setAimActivator(const QString& pageId, const QString& targetId);
    void clearAimActivator();
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
    void applyDwellFor(const PageTarget& t);
    void syncExpanded();
    [[nodiscard]] bool anyRootGridShown() const;
    [[nodiscard]] bool drawerMotionShown() const;
    [[nodiscard]] bool nonDrawerRootShown() const;
    void resetDrawerAnim();
    void snapHideDrawerMotion();
    void hideRootAutoClose(bool animate);
    void syncDrawerAnim(bool wasDrawer);
    [[nodiscard]] const PageTarget* findTarget(const QString& id) const;
    [[nodiscard]] const PageTarget* findLiveTarget(const QString& pageId,
                                                   const QString& targetId) const;
    [[nodiscard]] QString xmlPathFor(const QString& id) const;
    void ingest(const PageDocument& doc, bool isMaster, QVector<PageTarget>& targets,
                QVector<PageGridPaint>& gridPaints, bool includeDrawerMotion = false);
    void ensureHost();
    [[nodiscard]] QTransform hitXf() const;
    void armLeaveGate(const QString& pageId);
    void clearLeaveGate();
    [[nodiscard]] bool blockedByLeaveGate(const PageTarget* hit);
    void noteActivity();
    void tickAutoClose();
    void syncAutoClose();
    [[nodiscard]] int autoCloseIdleMs() const;
    void syncDrawerScale();

    struct AttachedPage {
        PageDocument doc;
    };

    struct PageBreadcrumb {
        PageDocument root;
        QVector<PageDocument> attached;
    };

    [[nodiscard]] PageBreadcrumb captureBreadcrumb() const;
    void restoreBreadcrumb(PageBreadcrumb snap);
    [[nodiscard]] bool applyNavPage(PageVerb verb, PageNavScope scope, const QString& id,
                                    const QString& sourcePageId, QString* error);
    [[nodiscard]] bool applyShowLayers(const QVector<int>& layers, const QString& sourcePageId,
                                       QString* error);
    [[nodiscard]] PageNav::Docs navDocs();
    [[nodiscard]] PageDocument* navPage(const QString& sourcePageId);
    void closePagesExcept(const QString& keepId);
    void emitShowChanged();

    PageDocument m_root;
    QVector<PageBreadcrumb> m_crumbs;
    QString m_layoutsDir;
    QHash<QString, PageDocument> m_memory;
    QVector<AttachedPage> m_attached;
    QVariantMap m_props;
    bool m_dwellSuspended = false;
    bool m_shiftHeld = false;
    bool m_autoCollapseMain = false;
    bool m_layoutAutoClose = true;
    int m_layoutAutoCloseIdleMs = 10000;
    QVector<PageTarget> m_targets;
    QVector<PageGridPaint> m_gridPaints;
    std::unique_ptr<PageHostWindow> m_host;
    DwellStateMachine m_dwell;
    QVector<int> m_globalSequence = {800};
    int m_globalGraceMs = 180;
    int m_globalScanGraceMs = 100;
    QVector<int> m_dailySequence = {400, 600, 400, 300, 200, 100};
    int m_dailyScanGraceMs = 100;
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
    QString m_aimActivator;

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
