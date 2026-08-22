#pragma once

#include "core/GazePoint.h"
#include "layout/DwellStateMachine.h"
#include "layout/PageHit.h"
#include "layout/PageTypes.h"
#include "ui/PageHostWindow.h"
#include "ui/ProgressVisuals.h"
#include "ui/Theme.h"

#include <QElapsedTimer>
#include <QObject>
#include <QRect>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QVariantMap>
#include <QVector>
#include <functional>
#include <memory>

namespace gazer {

class PageSession final : public QObject {
    Q_OBJECT

public:
    using DispatchFn = std::function<void(const QVector<PageAction>& actions, const QString& pageId,
                                          const QString& targetId)>;
    using DecorateFn = std::function<void(PageDocument&)>;
    using ActiveFn = std::function<bool(const QString& activeStateKey)>;

    enum class RootChrome { Docked, Drawer, Quit };

    explicit PageSession(QObject* parent = nullptr);

    [[nodiscard]] bool openRoot(const QString& xmlPath, QString* error = nullptr);
    void setLayoutsDirectory(const QString& dir) { m_layoutsDir = dir; }
    [[nodiscard]] bool openPage(const QString& id, QString* error = nullptr);
    void closePage(const QString& id);
    int closeAttached();
    [[nodiscard]] bool hasRoot() const { return m_root.isValid(); }
    [[nodiscard]] bool hasPage(const QString& id) const;
    [[nodiscard]] const PageDocument& root() const { return m_root; }
    [[nodiscard]] PageHostWindow* window() const { return m_host.get(); }
    [[nodiscard]] const QVector<PageTarget>& targets() const { return m_targets; }
    /// Attach or refresh an in-memory page (live editors). Does not decorate.
    [[nodiscard]] bool attachDocument(PageDocument doc, QString* error = nullptr);

    void setDispatch(DispatchFn fn) { m_dispatch = std::move(fn); }
    void setDecorate(DecorateFn fn) { m_decorate = std::move(fn); }
    void setActiveResolver(ActiveFn fn) { m_active = std::move(fn); }
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

    bool applyPageAction(PageVerb verb, PageTargetKind kind, const QString& id,
                         QString* error = nullptr);

    [[nodiscard]] bool onGaze(const GazePoint& point);
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
    void noteActivity();
    void tickAutoClose();
    [[nodiscard]] int autoCloseIdleMs() const;
    void syncDrawerScale();

    struct AttachedPage {
        PageDocument doc;
    };

    PageDocument m_root;
    QString m_layoutsDir;
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
    QTimer m_autoCloseTimer;
    QElapsedTimer m_idleClock;
    bool m_idleClockRunning = false;

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
