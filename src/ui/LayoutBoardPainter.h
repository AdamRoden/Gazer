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
    struct Input {
        const LayoutDocument* layout = nullptr;
        const QHash<QString, QRectF>* itemLocalRects = nullptr;
        const ThemeColors* theme = nullptr;
        int width = 0;
        int height = 0;
        QString hoverId;
        double hoverProgress = 0.0;
        ProgressVisuals progressVisuals;
        QSet<QString> activeItemIds;
        QVariantMap props;
        QString flashId;
        double boardOpacity = 1.0;
        QColor previewColor = ThemeColors::defaultProgressColor();
        GlassBackdrop* glass = nullptr;
        bool eraseBackground = true;
        bool paintWindowChrome = true;
        QString sliderScrubId;
        double sliderScrubT = 0.0;
        QString sliderScrubValue;
        double sliderScrubProgress = 0.0;
        const QHash<QString, double>* sliderReadoutT = nullptr;
        const QHash<QString, QString>* sliderReadoutValue = nullptr;
    };

    explicit LayoutBoardPainter(LayoutQuickWindow& host);
    explicit LayoutBoardPainter(Input in);

    void paint(QPainter& p);

    [[nodiscard]] static QRectF toggleTrackRect(const QRectF& cell);
    [[nodiscard]] static QRectF toggleHitRect(const QRectF& cell);

private:
    void paintCell(QPainter& p, const LayoutItem& item, const QRectF& r, bool onCard, qreal iconSide);
    void paintStaticItem(QPainter& p, const LayoutItem& item, const QRectF& r);
    void paintTab(QPainter& p, const LayoutItem& item, const QRectF& r);
    void paintCard(QPainter& p, const QRectF& r);
    void paintToggle(QPainter& p, const LayoutItem& item, const QRectF& r, qreal iconSide);
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

    [[nodiscard]] const LayoutDocument& layout() const { return *m.layout; }
    [[nodiscard]] const QHash<QString, QRectF>& rects() const { return *m.itemLocalRects; }
    [[nodiscard]] const ThemeColors& theme() const { return *m.theme; }
    [[nodiscard]] const QHash<QString, double>& readoutT() const
    {
        static const QHash<QString, double> empty;
        return m.sliderReadoutT ? *m.sliderReadoutT : empty;
    }
    [[nodiscard]] const QHash<QString, QString>& readoutV() const
    {
        static const QHash<QString, QString> empty;
        return m.sliderReadoutValue ? *m.sliderReadoutValue : empty;
    }

    Input m;
};

} // namespace gazer
