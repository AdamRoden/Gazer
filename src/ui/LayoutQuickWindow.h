#pragma once

#include "layout/LayoutTypes.h"
#include "ui/ProgressVisuals.h"
#include "ui/Theme.h"

#include <QHash>
#include <QQuickWindow>
#include <QSet>
#include <QTimer>
#include <QVariantMap>

class QCloseEvent;
class QPainter;

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
    void setActiveItemIds(const QSet<QString>& activeIds);
    void setPropertyContext(const QVariantMap& props);
    void flashItem(const QString& itemId);
    void showAndRaise();
    void assertAboveTaskbar();
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

signals:
    void closeRequested();
    void itemClicked(const QString& itemId);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void applyTopmost();
    void syncBoardSize();
    void paintBoard(QPainter& p);
    void paintDefault(QPainter& p);
    void paintFluent(QPainter& p);
    void paintCell(QPainter& p, const LayoutItem& item, const QRectF& r, bool fluent);
    void paintProgressChrome(QPainter& p, const QRectF& r, bool hovered, double progress,
                             const ProgressVisuals& visuals, double radius);
    [[nodiscard]] ProgressVisuals visualsForItem(const LayoutItem* item) const;
    [[nodiscard]] bool itemShown(const LayoutItem& item) const;

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
    LayoutBoardItem* m_board = nullptr;

    friend class LayoutBoardItem;
};

} // namespace gazer
