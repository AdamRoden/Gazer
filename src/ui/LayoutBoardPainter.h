#pragma once

#include "layout/LayoutTypes.h"
#include "ui/ProgressVisuals.h"
#include "ui/Theme.h"

#include <QHash>
#include <QRectF>
#include <QSet>
#include <QString>
#include <QVariantMap>

class QPainter;

namespace gazer {

class GlassBackdrop;
class LayoutQuickWindow;

/// Paints a layout board and owns toggle/cluster hit geometry.
class LayoutBoardPainter final {
public:
    explicit LayoutBoardPainter(LayoutQuickWindow& host);

    void paint(QPainter& p);

    [[nodiscard]] static QRectF toggleTrackRect(const QRectF& cell);
    [[nodiscard]] static QRectF toggleHitRect(const QRectF& cell);

private:
    void paintCell(QPainter& p, const LayoutItem& item, const QRectF& r, bool onCard);
    void paintStaticItem(QPainter& p, const LayoutItem& item, const QRectF& r);
    void paintTab(QPainter& p, const LayoutItem& item, const QRectF& r);
    void paintCard(QPainter& p, const QRectF& r);
    void paintToggle(QPainter& p, const LayoutItem& item, const QRectF& r);
    void paintClusterFrame(QPainter& p, const QString& cluster);
    void paintClusterSlot(QPainter& p, const LayoutItem& item, const QRectF& r);
    [[nodiscard]] QRectF clusterBounds(const QString& cluster) const;
    void paintProgressChrome(QPainter& p, const QRectF& r, bool hovered, double progress,
                             const ProgressVisuals& visuals, double radius);
    void paintSliderTrack(QPainter& p, const QRectF& r, const LayoutItem& item, bool hovered,
                          double progress);
    void paintPreviewSwatch(QPainter& p, const QRectF& r, double radius);
    void paintRadioButton(QPainter& p, const QRectF& cell, bool on);
    void fillChrome(QPainter& p, const QRectF& r, double radius, const QColor& bg,
                    const LayoutChromeStyle& st, const QColor& bgBot = QColor());
    [[nodiscard]] ProgressVisuals visualsForItem(const LayoutItem* item) const;
    [[nodiscard]] bool itemShown(const LayoutItem& item) const;
    [[nodiscard]] LayoutItemStyle resolvedItemStyle(const LayoutItem& item) const;

    const LayoutDocument& m_layout;
    const QHash<QString, QRectF>& m_itemLocalRects;
    const QString& m_hoverId;
    double m_hoverProgress = 0.0;
    const ProgressVisuals& m_progressVisuals;
    const ThemeColors& m_theme;
    const QSet<QString>& m_activeItemIds;
    const QVariantMap& m_props;
    const QString& m_flashId;
    double m_boardOpacity = 1.0;
    const QColor& m_previewColor;
    const QString& m_sliderScrubId;
    double m_sliderScrubT = 0.0;
    const QString& m_sliderScrubValue;
    double m_sliderScrubProgress = 0.0;
    const QHash<QString, double>& m_sliderReadoutT;
    const QHash<QString, QString>& m_sliderReadoutValue;
    GlassBackdrop& m_glass;
    int m_width = 0;
    int m_height = 0;
};

} // namespace gazer
