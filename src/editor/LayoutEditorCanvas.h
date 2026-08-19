#pragma once

#include "editor/LayoutEditorSession.h"
#include "ui/Theme.h"

#include <QPoint>
#include <QRectF>
#include <QString>
#include <QWidget>

namespace gazer {

/// Monitor bezel + board preview. Click to select, drag cells to a new row/col.
class LayoutEditorCanvas final : public QWidget {
    Q_OBJECT

public:
    explicit LayoutEditorCanvas(LayoutEditorSession& session, QWidget* parent = nullptr);

    void setTheme(const ThemeColors& theme);
    void setShowGrid(bool on);
    void setFitBoard(bool on);
    [[nodiscard]] bool fitBoard() const { return m_fitBoard; }
    [[nodiscard]] bool showGrid() const { return m_showGrid; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    struct ScreenMap {
        QRectF bezel;
        QRectF glass;
        QRectF screen;
        QRectF board; // in widget coords
        double scale = 1.0;
        QSize virtualScreen{1920, 1080};
    };

    [[nodiscard]] ScreenMap map() const;
    [[nodiscard]] QString hitItem(const QPoint& pos) const;
    [[nodiscard]] bool hitBoard(const QPoint& pos) const;
    void paintMonitor(class QPainter& p, const ScreenMap& m) const;
    void paintBoard(class QPainter& p, const ScreenMap& m) const;
    void paintCell(class QPainter& p, const LayoutItem& item, const QRectF& r,
                   bool selected, bool hovered) const;
    [[nodiscard]] QPoint cellAt(const QPoint& pos, const ScreenMap& m) const;

    LayoutEditorSession& m_session;
    ThemeColors m_theme = ThemeColors::darkPreset();
    bool m_showGrid = true;
    bool m_fitBoard = false;
    QString m_hoverId;
    QString m_dragId;
    QPoint m_pressPos;
    bool m_dragging = false;
};

} // namespace gazer
