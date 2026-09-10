#pragma once

#include "assist/ComboMouseHit.h"
#include "ui/OverlaySurface.h"
#include "ui/Theme.h"

#include <QColor>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QString>

namespace gazer {

/// Shared ComboMouse / LTS command pie: wedges, inner ring, optional hub label.
class PieOverlay final : public OverlaySurface {
public:
    struct Appearance {
        ComboMouseHit::Layout layout;
        ComboMouseHit::Band band = ComboMouseHit::Band::None;
        ComboMouseHit::Slice slice = ComboMouseHit::Slice::Right;
        double dwellProg = 0.0;
        QColor accent;
        ThemeColors theme = ThemeColors::darkPreset();
        QColor innerColor = ComboMouseHit::kDefaultInnerFill;
        QColor outerColor = ComboMouseHit::kDefaultOuterFill;
        const char* sliceIcons[ComboMouseHit::kSliceCount] = {};
        QString hubLabel;
        bool innerActive = false;
        QPointF driftDir;
        bool armed = false;
        ComboMouseHit::Slice armedSlice = ComboMouseHit::Slice::Drag;
    };

    PieOverlay();

    void setAppearance(const Appearance& a);
    void place(const QPoint& origin, const QRectF& screen);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    void paintCommands(QPainter& p, const QPointF& c, const QColor& cyan);
    void paintWedge(QPainter& p, const QPointF& c, const QColor& cyan, const ComboMouseHit::Wedge& w);
    void paintInnerRing(QPainter& p, const QPointF& c, const QColor& cyan);
    void paintHubLabel(QPainter& p, const QPointF& c);

    Appearance m_a;
    QPointF m_originLocal;
};

} // namespace gazer
