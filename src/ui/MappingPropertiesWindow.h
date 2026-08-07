#pragma once

#include "mapping/CurveMapping.h"

#include <QWidget>

class QComboBox;
class QLabel;

namespace gazer {

/// OpenTrack-style response curve editor: input X, output Y, editable control points.
class MappingPropertiesWindow final : public QWidget {
    Q_OBJECT

public:
    explicit MappingPropertiesWindow(QWidget* parent = nullptr);

    void setStore(CurveProfileStore* store);
    void setLiveInput(double value);
    void selectCurve(const QString& id);
    void showAndRaise();

signals:
    void curvesChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    [[nodiscard]] QRect graphRect() const;
    [[nodiscard]] QPointF toScreen(double in, double out) const;
    [[nodiscard]] QPointF toData(const QPoint& screen) const;
    void onCurvePicked(int index);
    void rebuildCombo();

    CurveProfileStore* m_store = nullptr;
    QString m_curveId = QStringLiteral("head.yaw");
    double m_liveIn = 0.0;
    bool m_hasLive = false;
    int m_dragIndex = -1;
    QComboBox* m_combo = nullptr;
    QLabel* m_hint = nullptr;
};

} // namespace gazer
