#pragma once

#include <QRect>
#include <QString>
#include <QVector>
#include <QWidget>

namespace gazer {

/// Shown when gaze has been missing. A valid gaze sample hides it.
/// Restart and Close are pointer clicks. The tray still has tracker retry.
class TrackingLossOverlay final : public QWidget {
    Q_OBJECT

public:
    explicit TrackingLossOverlay(QWidget* parent = nullptr);
    ~TrackingLossOverlay() override;

    void showLoss(const QString& status);
    void hideLoss();
    [[nodiscard]] bool isActive() const { return isVisible(); }

signals:
    void restartRequested();
    void closeRequested();

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
    void fire(const QString& id);

    QVector<Cell> m_cells;
    QRect m_statusRect;
    QString m_status;
    bool m_firing = false;
};

} // namespace gazer
