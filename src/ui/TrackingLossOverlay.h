#pragma once

#include <QRect>
#include <QString>
#include <QWidget>

namespace gazer {

/// Notice shown while a running tracker has no gaze.
/// A valid gaze sample hides it. It has no buttons.
class TrackingLossOverlay final : public QWidget {
    Q_OBJECT

public:
    explicit TrackingLossOverlay(QWidget* parent = nullptr);
    ~TrackingLossOverlay() override;

    void showLoss(const QString& status);
    void hideLoss();
    [[nodiscard]] bool isActive() const { return isVisible(); }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void layoutNotice();

    QRect m_statusRect;
    QString m_status;
};

} // namespace gazer
