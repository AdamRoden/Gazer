#pragma once

#include "editor/LayoutEditorSession.h"
#include "layout/PageHit.h"
#include "ui/Theme.h"

#include <QHash>
#include <QLineF>
#include <QPoint>
#include <QRect>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

namespace gazer {

/// Page preview on a virtual display. Wheel zooms, middle-drag pans, Fit frames the grid or screen.
class LayoutEditorCanvas final : public QWidget {
    Q_OBJECT

public:
    explicit LayoutEditorCanvas(LayoutEditorSession& session, QWidget* parent = nullptr);

    void setTheme(const ThemeColors& theme);
    void setLayoutSearchDirs(const QStringList& dirs);
    void setShowGrid(bool on);
    /// 0 = page showLayers. Otherwise only that layer.
    void setLayerFilter(int layer);
    void setTestMode(bool on);
    void fitGrid();
    void fitScreen();
    void zoomBy(double factor);
    void zoomAt(const QPoint& canvasPos, double factor);
    [[nodiscard]] double zoom() const { return m_scale; }
    [[nodiscard]] bool showGrid() const { return m_showGrid; }
    [[nodiscard]] bool testMode() const { return m_testMode; }

signals:
    void zoomChanged(double scale);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    enum class Drag { None, Move, Resize, Rubber, Grid, Pan };

    struct ScreenMap {
        QRectF bezel;
        QRectF glass;
        QRectF screen;
        QRectF board;
        QRectF boardVirt;
        double scaleX = 1.0;
        double scaleY = 1.0;
        QSize virtualScreen{1920, 1080};
        QRect virtualDesktop{0, 0, 1920, 1080};
        QVector<QRect> taskbars;
        QSize virtBoard{1, 1};
        QVector<PageGridPaint> grids;
        QVector<PageTarget> targets;

        [[nodiscard]] double px() const { return scaleX; }

        [[nodiscard]] PageFrame pageFrame() const
        {
            PageFrame f;
            f.screen = QRectF(0, 0, virtualScreen.width(), virtualScreen.height());
            f.desktop = QRectF(virtualDesktop);
            return f;
        }

        [[nodiscard]] QPointF fromVirt(QPointF v) const
        {
            return {screen.left() + v.x() * scaleX, screen.top() + v.y() * scaleY};
        }
        [[nodiscard]] QRectF fromVirt(const QRect& r) const
        {
            return QRectF(fromVirt(QPointF(r.x(), r.y())),
                          QSizeF(r.width() * scaleX, r.height() * scaleY));
        }
    };

    [[nodiscard]] ScreenMap map() const;
    [[nodiscard]] static QRectF targetRect(const PageTarget& t);
    [[nodiscard]] QRectF gridVisual(const ScreenMap& m, const QString& gridId) const;
    [[nodiscard]] QString hitItem(const QPoint& pos, const ScreenMap& m) const;
    [[nodiscard]] bool hitBoard(const QPoint& pos, const ScreenMap& m) const;
    [[nodiscard]] Qt::Edges hitHandle(const QPoint& pos, const ScreenMap& m) const;
    void paintMonitor(class QPainter& p, const ScreenMap& m) const;
    void paintTaskbar(class QPainter& p, const ScreenMap& m) const;
    void paintPlacementPip(class QPainter& p, const ScreenMap& m) const;
    void paintBoard(class QPainter& p, const ScreenMap& m) const;
    [[nodiscard]] QHash<QString, QRectF> boardItemRects(const ScreenMap& m) const;
    void paintSelectionOverlay(class QPainter& p, const QRectF& r, bool handles) const;
    [[nodiscard]] QPoint cellAt(const QPoint& pos, const ScreenMap& m, QString* gridId = nullptr) const;
    [[nodiscard]] QRectF toCanvas(const QRectF& virt, const ScreenMap& m) const;
    [[nodiscard]] QPointF toVirt(const QPoint& pos, const ScreenMap& m) const;
    void applyResize(const QPoint& pos, const ScreenMap& m);
    void commitDrag(const QPoint& pos, const ScreenMap& m);
    void updateDragCursor(const ScreenMap& m, const QPoint& pos);
    void tickTestDwell();
    void paintHandles(class QPainter& p, const QRectF& r) const;
    void paintGuides(class QPainter& p, const ScreenMap& m) const;
    void updateDragPreview(const QPoint& pos, const ScreenMap& m);
    void clearDragPreview();
    [[nodiscard]] QRectF snapMoveDelta(QPointF* delta, const QRectF& virt, const ScreenMap& m);
    [[nodiscard]] QRectF snapRect(const QRectF& virt, Qt::Edges edges, const ScreenMap& m);
    [[nodiscard]] QRectF selectedRect(const ScreenMap& m) const;
    [[nodiscard]] QPoint virtFromCanvas(const QPoint& pos, const ScreenMap& m) const;
    void fitToVirt(const QRectF& virt);
    [[nodiscard]] QRectF virtContentRect(const ScreenMap& m) const;
    void ensureCamera();

    LayoutEditorSession& m_session;
    QStringList m_layoutSearchDirs;
    ThemeColors m_theme = ThemeColors::darkPreset();
    bool m_showGrid = true;
    int m_layerFilter = 0;
    bool m_testMode = false;
    double m_scale = 0.0;
    QPointF m_lookAt;
    bool m_spaceHeld = false;
    QString m_hoverId;
    QString m_dragId;
    QPoint m_pressPos;
    QPoint m_dragPos;
    QRectF m_pressRect;
    QRect m_rubber;
    QRectF m_ghostVirt;
    QVector<QLineF> m_guides;
    Drag m_drag = Drag::None;
    Qt::Edges m_resizeEdges;
    class QTimer* m_testTimer = nullptr;
    double m_testProgress = 0.0;
};

} // namespace gazer
