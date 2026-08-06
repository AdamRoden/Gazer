#pragma once

#include "layout/LayoutTypes.h"
#include "ui/ProgressVisuals.h"

#include <QHash>
#include <QRectF>
#include <QSet>
#include <QTimer>
#include <QWidget>

class QCloseEvent;
class QMouseEvent;
class QShowEvent;

namespace gazer {

/// On-screen AAC board: paints grid items and dwell progress.
class LayoutWindow final : public QWidget {
    Q_OBJECT

public:
    explicit LayoutWindow(QWidget* parent = nullptr);

    void setLayout(const LayoutDocument& layout);
    void clearLayout();

    [[nodiscard]] bool hasLayout() const { return m_layout.isValid(); }
    [[nodiscard]] const LayoutDocument& layout() const { return m_layout; }
    [[nodiscard]] QHash<QString, QRectF> itemLocalRects() const { return m_itemLocalRects; }
    [[nodiscard]] QPoint boardTopLeftGlobal() const { return mapToGlobal(QPoint(0, 0)); }

    [[nodiscard]] QString hitTestGlobal(const QPointF& screenPoint) const;

    void setHoverState(const QString& itemId, double progress);
    void setProgressVisuals(const ProgressVisuals& visuals);
    /// Item ids that are currently "on" (held / toggled / selected).
    void setActiveItemIds(const QSet<QString>& activeIds);
    void flashItem(const QString& itemId);
    void showAndRaise();

public slots:
    void onActiveLayoutChanged(const gazer::LayoutDocument& layout);

signals:
    void closeRequested();
    void itemClicked(const QString& itemId);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void rebuildCellGeometry();
    void applyTopmost();
    void paintDefault(QPainter& p);
    void paintFluent(QPainter& p);
    void paintProgressChrome(QPainter& p, const QRectF& r, bool hovered, double progress,
                             const ProgressVisuals& visuals);
    [[nodiscard]] ProgressVisuals visualsForItem(const LayoutItem* item) const;

    LayoutDocument m_layout;
    QHash<QString, QRectF> m_itemLocalRects;
    QString m_hoverId;
    double m_hoverProgress = 0.0;
    ProgressVisuals m_progressVisuals; // global base from AppSettings
    QSet<QString> m_activeItemIds;
    QString m_flashId;
    QTimer m_flashTimer;
};

} // namespace gazer
