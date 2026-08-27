#pragma once

#include "layout/PageHit.h"
#include "ui/GlassBackdrop.h"
#include "ui/ProgressVisuals.h"
#include "ui/Theme.h"

#include <QColor>
#include <QRectF>
#include <QQuickWindow>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QTransform>
#include <QVector>

class QCloseEvent;
class QKeyEvent;
class QPainter;
class QQuickPaintedItem;

namespace gazer {

/// Single never-destroyed Page surface. Grids and Zones are regions, not child HWNDs.
class PageHostWindow final : public QQuickWindow {
    Q_OBJECT

public:
    explicit PageHostWindow(QWindow* parent = nullptr);

    void setTheme(const ThemeColors& theme);
    void setProgressVisuals(const ProgressVisuals& visuals);
    void commit(QVector<PageTarget> targets, QVector<PageGridPaint> grids, double drawerScale,
                QRectF reserved = {});
    void setDrawerScale(double scale);
    void setActiveIds(QSet<QString> ids);
    void setLockedIds(QSet<QString> ids);
    void setShiftHeld(bool on);
    void setHover(const QString& id, double progress, bool revealProgress = false);
    void flash(const QString& id);
    void setPreviewColor(const QColor& color);
    void setSliderScrub(const QString& itemId, double t, const QString& valueText,
                        double dwellProgress);
    void clearSliderScrub();
    void setInputFocusEnabled(bool on);
    void showHost();
    void raiseHost();

    [[nodiscard]] QString mouseHit(const QPointF& global) const;
    [[nodiscard]] const QVector<PageTarget>& targets() const { return m_targets; }
    [[nodiscard]] QPoint origin() const { return m_origin; }
    [[nodiscard]] const QTransform& drawerXf() const { return m_drawerXf; }

signals:
    void targetClicked(const QString& targetId);
    void keyPressed(int key, const QString& text);

protected:
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void syncBoardSize();
    void syncGlass();
    void fitToChrome();
    void applyChrome();
    void applyInputFocusChrome();
    void cacheDrawerXf();
    void syncFrost(bool grabNow);
    [[nodiscard]] QPoint paintOrigin() const;
    enum class ChromePass { Live, Underlay };
    void paintScene(QPainter& p, ChromePass pass);
    void refreshUnderlay();

    QQuickPaintedItem* m_board = nullptr;
    GlassBackdrop m_glass;
    ThemeColors m_theme;
    ProgressVisuals m_progress;
    QVector<PageTarget> m_targets;
    QVector<PageGridPaint> m_gridPaints;
    QRectF m_reserved;
    double m_drawerScale = 1.0;
    QTransform m_drawerXf;
    double m_blurMax = 0.0;
    QSet<QString> m_activeIds;
    QSet<QString> m_lockedIds;
    bool m_shiftHeld = false;
    QString m_hoverId;
    double m_hoverProgress = 0.0;
    bool m_revealProgress = false;
    QString m_flashId;
    QTimer m_flashTimer;
    QTimer m_raiseTimer;
    QRectF m_flashRect;
    PageBox m_flashRadii;
    QPoint m_origin;
    QColor m_previewColor;
    QString m_sliderScrubId;
    double m_sliderScrubT = 0.0;
    QString m_sliderScrubValue;
    double m_sliderScrubProgress = 0.0;
    bool m_inputFocus = false;
    friend class PageHostItem;
};

} // namespace gazer
