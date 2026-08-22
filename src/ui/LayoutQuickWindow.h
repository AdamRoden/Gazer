#pragma once

#include "layout/LayoutTypes.h"
#include "ui/GlassBackdrop.h"
#include "ui/ProgressVisuals.h"
#include "ui/Theme.h"

#include <QHash>
#include <QtGlobal>
#include <QQuickWindow>
#include <QSet>
#include <QTimer>
#include <QVariantMap>

class QCloseEvent;
class QKeyEvent;

namespace gazer {

class LayoutBoardItem;

/// Frameless topmost Qt Quick board.
class LayoutQuickWindow final : public QQuickWindow {
    Q_OBJECT

public:
    explicit LayoutQuickWindow(QWindow* parent = nullptr);

    void setLayout(const LayoutDocument& layout);
    void clearLayout();

    [[nodiscard]] bool hasLayout() const { return m_layout.isValid(); }
    [[nodiscard]] const LayoutDocument& layout() const { return m_layout; }
    [[nodiscard]] QHash<QString, QRectF> itemLocalRects() const { return m_itemLocalRects; }
    [[nodiscard]] QPoint boardTopLeftGlobal() const { return mapToGlobal(QPoint(0, 0)); }

    [[nodiscard]] QString hitTestGlobal(const QPointF& screenPoint) const;

    void setHoverState(const QString& itemId, double progress);
    void setProgressVisuals(const ProgressVisuals& visuals);
    void setTheme(const ThemeColors& theme);
    [[nodiscard]] const ThemeColors& theme() const { return m_theme; }
    void setActiveItemIds(const QSet<QString>& activeIds);
    void setPropertyContext(const QVariantMap& props);
    void setPreviewColor(const QColor& color);
    /// Color-slider scrub overlay: enlarged value ring that tracks gaze along the track.
    void setSliderScrub(const QString& itemId, double t, const QString& valueText,
                        double dwellProgress);
    void clearSliderScrub();
    void setSliderReadout(const QString& itemId, double t, const QString& valueText);
    void flashItem(const QString& itemId);
    void showAndRaise();
    /// HWND_TOPMOST only — no restack dip. No-op unless window.aboveTaskbar.
    void keepAboveTaskbar();
    void setBoardOpacity(double opacity);

    void setMinimumSize(int w, int h) { QQuickWindow::setMinimumSize(QSize(w, h)); }
    void setMaximumSize(int w, int h) { QQuickWindow::setMaximumSize(QSize(w, h)); }
    void resize(int w, int h) { QQuickWindow::resize(QSize(w, h)); }
    void move(int x, int y) { setPosition(x, y); }
    void move(const QPoint& p) { setPosition(p); }
    void setWindowTitle(const QString& title) { setTitle(title); }

    void rebuildCellGeometry();
    /// Live editors need key events; boards stay NOACTIVATE otherwise.
    void setInputFocusEnabled(bool on);

signals:
    void closeRequested();
    void itemClicked(const QString& itemId);
    void keyPressed(int key, const QString& text);

protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void applyTopmost();
    void applyInputFocusChrome();
    void syncBoardSize();
    [[nodiscard]] ProgressVisuals visualsForItem(const LayoutItem* item) const;
    [[nodiscard]] bool itemShown(const LayoutItem& item) const;
    [[nodiscard]] LayoutItemStyle resolvedItemStyle(const LayoutItem& item) const;

    friend class LayoutBoardPainter;

    LayoutDocument m_layout;
    QHash<QString, QRectF> m_itemLocalRects;
    QString m_hoverId;
    double m_hoverProgress = 0.0;
    ProgressVisuals m_progressVisuals;
    ThemeColors m_theme = ThemeColors::darkPreset();
    QSet<QString> m_activeItemIds;
    QVariantMap m_props;
    QString m_flashId;
    QTimer m_flashTimer;
    double m_boardOpacity = 1.0;
    QColor m_previewColor = ThemeColors::defaultProgressColor();
    QString m_sliderScrubId;
    double m_sliderScrubT = 0.0;
    QString m_sliderScrubValue;
    double m_sliderScrubProgress = 0.0;
    QHash<QString, double> m_sliderReadoutT;
    QHash<QString, QString> m_sliderReadoutValue;
    LayoutBoardItem* m_board = nullptr;
    GlassBackdrop m_glass;
    bool m_inputFocus = false;

    friend class LayoutBoardItem;
};

} // namespace gazer
