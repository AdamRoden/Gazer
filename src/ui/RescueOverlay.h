#pragma once

#include <QElapsedTimer>
#include <QRect>
#include <QString>
#include <QTimer>
#include <QVector>
#include <QWidget>

namespace gazer {

/// Tiny dwell board used only by `Gazer.exe --guard`. Not PageSession.
class RescueOverlay final : public QWidget {
    Q_OBJECT

public:
    enum class Mode { Hidden, Yield, CrashLoop };

    explicit RescueOverlay(QWidget* parent = nullptr);

    void setMode(Mode mode);
    [[nodiscard]] Mode mode() const { return m_mode; }
    /// Logical global aim. When @p valid, dwell uses this instead of the cursor.
    void setAimPoint(const QPoint& global, bool valid);

signals:
    void restartHost();
    void restartSafe();
    void quitGuard();
    void yieldDesktop();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    struct Cell {
        QString id;
        QString label;
        QRect rect;
    };

    void layoutCells();
    void tickGaze();
    void fire(const QString& id);

    Mode m_mode = Mode::Hidden;
    QVector<Cell> m_cells;
    QTimer m_tick;
    QString m_hoverId;
    QElapsedTimer m_dwell;
    QPoint m_aim;
    bool m_aimValid = false;
};

} // namespace gazer
