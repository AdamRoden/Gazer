#pragma once

#include "editor/LayoutEditorSession.h"
#include "ui/Theme.h"

#include <QPoint>
#include <QRectF>
#include <QString>
#include <QWidget>

namespace gazer {

/// Board preview (Fit = edit view). Click/shift-click to select, drag to move, edges to resize.
class LayoutEditorCanvas final : public QWidget {
    Q_OBJECT

public:
    explicit LayoutEditorCanvas(LayoutEditorSession& session, QWidget* parent = nullptr);

    void setTheme(const ThemeColors& theme);
    void setShowGrid(bool on);
    void setFitBoard(bool on);
    void setTestMode(bool on);
    [[nodiscard]] bool fitBoard() const { return m_fitBoard; }
    [[nodiscard]] bool showGrid() const { return m_showGrid; }
    [[nodiscard]] bool testMode() const { return m_testMode; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    enum class Drag { None, Move, ResizeW, ResizeH, Rubber, Window };

    struct ScreenMap {
        QRectF bezel;
        QRectF glass;
        QRectF screen;
        QRectF board;
        double scale = 1.0;
        QSize virtualScreen{1920, 1080};
    };

    [[nodiscard]] ScreenMap map() const;
    [[nodiscard]] QString hitItem(const QPoint& pos, const ScreenMap& m) const;
    [[nodiscard]] bool hitBoard(const QPoint& pos, const ScreenMap& m) const;
    [[nodiscard]] Drag hitHandle(const QPoint& pos, const ScreenMap& m) const;
    void paintMonitor(class QPainter& p, const ScreenMap& m) const;
    void paintPlacementPip(class QPainter& p, const ScreenMap& m) const;
    void paintBoard(class QPainter& p, const ScreenMap& m) const;
    void paintCell(class QPainter& p, const LayoutItem& item, const QRectF& r, bool selected,
                   bool hovered) const;
    [[nodiscard]] QPoint cellAt(const QPoint& pos, const ScreenMap& m) const;
    void applyResize(const QPoint& pos, const ScreenMap& m);
    void commitDrag(const QPoint& pos, const ScreenMap& m);
    void updateDragCursor(const ScreenMap& m, const QPoint& pos);
    void tickTestDwell();

    LayoutEditorSession& m_session;
    ThemeColors m_theme = ThemeColors::darkPreset();
    bool m_showGrid = true;
    bool m_fitBoard = true;
    bool m_testMode = false;
    QString m_hoverId;
    QString m_dragId;
    QPoint m_pressPos;
    QPoint m_dragPos;
    QRectF m_pressRect;
    QRect m_rubber;
    Drag m_drag = Drag::None;
    class QTimer* m_testTimer = nullptr;
    double m_testProgress = 0.0;
};

} // namespace gazer
