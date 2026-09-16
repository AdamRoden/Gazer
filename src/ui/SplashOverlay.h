#pragma once

#include "assist/GazeDwellTracker.h"
#include "core/GazePoint.h"
#include "ui/OverlaySurface.h"
#include "ui/ProgressVisuals.h"
#include "ui/Theme.h"

#include <QColor>
#include <QElapsedTimer>
#include <QPixmap>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QString>
#include <QTimer>
#include <QVector>

class QPainter;
class QPaintEvent;

namespace gazer {

class PageSession;

namespace splash {
enum class Mark;
}

/// Full-desktop frosted tour of the dock. Next / Previous / Skip are dwell cells.
class SplashOverlay final : public OverlaySurface {
    Q_OBJECT

public:
    explicit SplashOverlay(QWidget* parent = nullptr);

    void setTheme(const ThemeColors& theme);
    void setAccent(const QColor& c);
    void setProgressVisuals(const ProgressVisuals& v);
    void setSession(PageSession* pages);

    void start();
    void skip();
    void cancel();
    [[nodiscard]] bool isActive() const { return m_active; }

    void onGaze(const GazePoint& point);

signals:
    void finished();
    void activeChanged(bool active);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    enum class Phase {
        Welcome,
        Menu,
        OpenMenu,
        Sleep,
        Suspend,
        Callouts,
        Fade,
        Done
    };
    enum class NavChip { None, Prev, Next, Skip };

    void tick();
    void enter(Phase phase);
    void finish();
    void goNext();
    void goPrev();
    void refreshGeometry();
    void captureFrost();
    void scheduleRecapture(int ms);
    void setMasterLayers(const QVector<int>& layers);
    void setSuspended(bool on);
    [[nodiscard]] QRect targetRect(const QString& id) const;
    [[nodiscard]] QRect dwellRect(const QString& id) const;
    [[nodiscard]] QRect menuRect() const;
    [[nodiscard]] QRect menuDwell() const;
    [[nodiscard]] QRect sleepRect() const;
    [[nodiscard]] QRect sleepDwell() const;
    [[nodiscard]] QRect markRect(splash::Mark mark) const;
    [[nodiscard]] QPointF markPoint(splash::Mark mark) const;
    [[nodiscard]] QRect navClusterGlobal() const;
    [[nodiscard]] QRect skipRectGlobal() const;
    [[nodiscard]] QRect prevRectGlobal() const;
    [[nodiscard]] QRect nextRectGlobal() const;
    [[nodiscard]] bool prevEnabled() const;
    [[nodiscard]] bool nextEnabled() const;
    [[nodiscard]] double fittedScale() const;
    [[nodiscard]] double viewScale() const;
    [[nodiscard]] int miniTopPad() const;
    [[nodiscard]] QRectF miniScreenLocal() const;
    [[nodiscard]] QPointF mapGlobalToLocal(QPointF global) const;
    [[nodiscard]] QRectF mapGlobalToLocal(const QRect& global) const;
    [[nodiscard]] QPointF gazePos() const;
    [[nodiscard]] double phaseT() const;
    [[nodiscard]] double enterFade() const;
    [[nodiscard]] double fadeAlpha() const;
    [[nodiscard]] QRect localRect(const QRect& global) const;
    [[nodiscard]] NavChip hitNav(const QPoint& global) const;
    void paintBackdrop(QPainter& p) const;
    void paintMiniScreen(QPainter& p) const;
    void paintSuspendFrame(QPainter& p) const;
    void paintSleepActive(QPainter& p) const;
    [[nodiscard]] QRectF captionAnchor(splash::Mark mark) const;
    void paintDwellZone(QPainter& p, const QRect& global, double pulse) const;
    void paintHoleRing(QPainter& p, const QRectF& local, double pulse) const;
    void paintGaze(QPainter& p, const QPointF& local, double progress) const;
    void paintWelcome(QPainter& p, double opacity) const;
    void paintCaption(QPainter& p, const QRectF& nearLocal, const QString& title,
                      const QString& body, double opacity, bool below = false) const;
    void paintNav(QPainter& p) const;
    void paintNavChip(QPainter& p, const QRect& global, const QString& icon, const QString& label,
                      double progress, bool enabled) const;

    ThemeColors m_theme;
    QColor m_accent = ThemeColors::defaultProgressColor();
    ProgressVisuals m_progress;
    PageSession* m_pages = nullptr;

    bool m_active = false;
    Phase m_phase = Phase::Done;
    int m_phaseElapsedMs = 0;
    QTimer m_timer;
    QTimer m_recapture;
    QElapsedTimer m_clock;
    qint64 m_lastMs = 0;
    qint64 m_navLastMs = -1;

    QPixmap m_frost;
    QPixmap m_screenGrab;
    QPixmap m_logo;
    QPointF m_gaze;
    bool m_gazeValid = false;

    NavChip m_navHit = NavChip::None;
    GazeDwellTracker m_navDwell;
};

} // namespace gazer
