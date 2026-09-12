#pragma once

#include "layout/PageHit.h"
#include "mapping/HeadPoseTypes.h"
#include "ui/GlassBackdrop.h"
#include "ui/ProgressVisuals.h"
#include "ui/Theme.h"

#include <QColor>
#include <QImage>
#include <QRectF>
#include <QString>
#include <QVector>

class QPainter;

namespace gazer {
namespace BoardPaint {

/// Live extras for role paint (color picker, sliders, head-pose curve/preview).
struct Live {
    QColor previewColor;
    QString sliderScrubId;
    double sliderScrubT = 0.0;
    QString sliderScrubValue;
    double sliderScrubProgress = 0.0;
    QImage headPreview;
    QVector<HeadPoseCurvePoint> curvePoints;
    int curveSelected = -1;
    double curveLiveIn = 0.0;
    bool curveLiveOn = false;
};

[[nodiscard]] QString segoeFamily();
[[nodiscard]] int fontPxToFit(const QString& family, int weight, int startPx, int minPx,
                              const QString& text, const QRectF& box, int flags);

void fillRound(QPainter& p, const QRectF& r, double radius, const QColor& bg);
void fillRound(QPainter& p, const QRectF& r, const PageBox& radii, const QColor& bg);
void strokeRound(QPainter& p, const QRectF& r, double radius, const QColor& color, double width);
void strokeRound(QPainter& p, const QRectF& r, const PageBox& radii, const QColor& color,
                 const PageBox& width);

void paintLabel(QPainter& p, const PageTarget& t, const QRectF& r, const ThemeColors& theme);
void paintTab(QPainter& p, const PageTarget& t, const QRectF& r, const ThemeColors& theme,
              bool hovered, bool selected, double progress);
void paintIconAndText(QPainter& p, const PageTarget& t, const QRectF& r, const QColor& fg,
                      const ThemeColors& theme);
void paintSurface(QPainter& p, const QRectF& r, const PageChrome& chrome, const ThemeColors& theme,
                  GlassBackdrop* glass, bool grid, bool hovered, bool active, bool interactive);
void paintTarget(QPainter& p, const PageTarget& t, const QRectF& r, const ThemeColors& theme,
                 GlassBackdrop* glass, bool hovered, double progress, bool flashing, bool active,
                 const ProgressVisuals& pv, const Live& live = {}, bool locked = false);

} // namespace BoardPaint
} // namespace gazer
