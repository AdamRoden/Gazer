#pragma once

#include "assist/LookToMap.h"
#include "ui/OverlaySurface.h"

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QRgb>

class QPaintEvent;
class QPainter;

namespace gazer {

/// Assist-band HUD for a look-to map. Live paints the stretching inner-deadzone
/// orb (or the pause hub). Preview paints analog rings and the gaze line.
class LookToOverlay final : public OverlaySurface {
public:
    enum class Kind { Live, Preview };

    explicit LookToOverlay(Kind kind = Kind::Live);

    void setKind(Kind kind) { m_kind = kind; }
    void setAccent(const QColor& c);
    void setHighlight(LookToRing ring);

    void setState(const LookToMapSettings& cfg, const QPointF& gaze, double gain,
                  double centerProg, double hubRadius, bool paused);
    void placeCenter(const QPoint& screenCenter);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    [[nodiscard]] static double orbThickness(int deadzonePx);
    [[nodiscard]] double highlightRadius() const;
    void paintOrb(QPainter& p, const QPointF& c, const QColor& cyan);
    void paintRings(QPainter& p, const QPointF& c, const QColor& cyan);
    void paintGaze(QPainter& p, const QPointF& c, const QColor& cyan);
    void paintActivator(QPainter& p, const QPointF& c, const QColor& accent);
    [[nodiscard]] QImage renderOrbBlur(const QPointF& c, double stretch, double ang,
                                       const QColor& cyan, int alpha, bool hollow) const;

    Kind m_kind = Kind::Live;
    LookToMapSettings m_cfg;
    QPointF m_gaze;
    double m_gain = 0.0;
    double m_centerProg = 0.0;
    double m_hubR = 27.0;
    int m_box = 260;
    bool m_paused = false;
    LookToRing m_highlight = LookToRing::None;
    QColor m_accent;

    struct OrbKey {
        int w = 0;
        int h = 0;
        int deadzone = 0;
        int alpha = 0;
        QRgb rgb = 0;
        int style = 0;
        int stretchQ = 0;
        int angQ = 0;
        bool operator==(const OrbKey&) const = default;
    };
    OrbKey m_orbKey;
    QImage m_orbBlur;
};

} // namespace gazer
