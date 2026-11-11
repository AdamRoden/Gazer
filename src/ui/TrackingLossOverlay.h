#pragma once

#include <QElapsedTimer>
#include <QRect>
#include <QString>
#include <QTimer>
#include <QVector>
#include <QWidget>

namespace gazer {

/// Gaze dwell board shown when eye tracking is paused.
/// Resume does not run until the user dwells it. Retry and Use mouse are the
/// other cells. A physical mouse can use the tray for the same choices.
class TrackingLossOverlay final : public QWidget {
    Q_OBJECT

public:
    explicit TrackingLossOverlay(QWidget* parent = nullptr);
    ~TrackingLossOverlay() override;

    void showLoss(const QString& status);
    void setStatus(const QString& status);
    void hideLoss();
    [[nodiscard]] bool isActive() const { return isVisible(); }

    /// Logical global aim. Dwell runs only while @p valid.
    void setAimPoint(const QPoint& global, bool valid);

signals:
    void resumeRequested();
    void retryRequested();
    void useMouseRequested();

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
    void tick();
    void fire(const QString& id);

    QVector<Cell> m_cells;
    QRect m_statusRect;
    QString m_status;
    QTimer m_tick;
    QString m_hoverId;
    QElapsedTimer m_dwell;
    QPoint m_aim;
    bool m_aimValid = false;
    bool m_firing = false;
};

} // namespace gazer
