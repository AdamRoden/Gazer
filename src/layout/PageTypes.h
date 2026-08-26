#pragma once

#include "layout/PageBox.h"
#include "layout/ProgressStyle.h"

#include <QColor>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QStringView>
#include <QVector>
#include <optional>

namespace gazer {

/// One offset/size component. Proportion iff the token contained `.` or `/`; else pixels.
/// Suffix `h` (`0.25h`, `1/4h`) is a proportion of the reference *height* on either axis
/// (square boards: `size="0.25h,0.25h"`).
/// `Expression` is absolute pixels from `A_ScreenWidth` / `A_ScreenHeight` arithmetic
/// (`A_ScreenHeight/9*16`).
struct PageDim {
    enum class Unit { Unset, Pixels, Proportion, HeightProportion, Expression };

    Unit unit = Unit::Unset;
    double value = 0.0;
    /// Original expression text when `unit == Expression`.
    QString expr;

    [[nodiscard]] bool isSet() const { return unit != Unit::Unset; }

    [[nodiscard]] static PageDim pixels(double px)
    {
        PageDim d;
        d.unit = Unit::Pixels;
        d.value = px;
        return d;
    }

    [[nodiscard]] static PageDim proportion(double p)
    {
        PageDim d;
        d.unit = Unit::Proportion;
        d.value = p;
        return d;
    }

    [[nodiscard]] static PageDim heightProportion(double p)
    {
        PageDim d;
        d.unit = Unit::HeightProportion;
        d.value = p;
        return d;
    }

    [[nodiscard]] static PageDim expression(const QString& text)
    {
        PageDim d;
        d.unit = Unit::Expression;
        d.expr = text.trimmed();
        return d;
    }

    /// @p axisRef is width for x / height for y. @p heightRef is the box height
    /// (used by HeightProportion on either axis). One-arg form uses @p axisRef for both.
    /// Expressions use @p screenWidth / @p screenHeight (`A_ScreenWidth` / `A_ScreenHeight`).
    [[nodiscard]] double resolve(double axisRef) const { return resolve(axisRef, axisRef); }

    [[nodiscard]] double resolve(double axisRef, double heightRef) const
    {
        return resolve(axisRef, heightRef, 0.0, 0.0);
    }

    [[nodiscard]] double resolve(double axisRef, double heightRef, double screenWidth,
                                 double screenHeight) const;
};

struct PageDimPair {
    PageDim x;
    PageDim y;

    [[nodiscard]] bool isSet() const { return x.isSet() || y.isSet(); }
};

/// PageNotes anchors. Top pins top-center to top-center of the reference.
enum class PageAnchor {
    TopLeft,
    Top,
    TopRight,
    Left,
    Center,
    Right,
    BottomLeft,
    Bottom,
    BottomRight
};

struct PageChrome {
    std::optional<QColor> background;
    std::optional<QColor> foreground;
    std::optional<QColor> borderColor;
    std::optional<PageBox> thickness;
    std::optional<PageBox> radius;
    std::optional<double> blur;
    std::optional<ProgressStyle> progressStyle;
    std::optional<QColor> progressColor;

    [[nodiscard]] bool hasBlur() const { return blur.has_value() && *blur > 0.0; }

    static constexpr double kDefaultThickness = 1.0;
    static constexpr double kDefaultRadius = 8.0;
    static constexpr double kClusteredRadius = 4.0;

    [[nodiscard]] static PageChrome defaults()
    {
        PageChrome c;
        c.thickness = PageBox::all(kDefaultThickness);
        return c;
    }

    [[nodiscard]] PageBox resolvedRadius(bool clustered = false) const
    {
        return radius.value_or(PageBox::all(clustered ? kClusteredRadius : kDefaultRadius));
    }

    [[nodiscard]] PageBox resolvedThickness() const
    {
        return thickness.value_or(PageBox::all(kDefaultThickness));
    }

    [[nodiscard]] bool hasAny() const
    {
        return background.has_value() || foreground.has_value() || borderColor.has_value()
               || thickness.has_value() || radius.has_value() || blur.has_value()
               || progressStyle.has_value() || progressColor.has_value();
    }

    [[nodiscard]] PageChrome withOverrides(const PageChrome& ovr) const
    {
        PageChrome out = *this;
        if (ovr.background) {
            out.background = ovr.background;
        }
        if (ovr.foreground) {
            out.foreground = ovr.foreground;
        }
        if (ovr.borderColor) {
            out.borderColor = ovr.borderColor;
        }
        if (ovr.thickness) {
            out.thickness = ovr.thickness;
        }
        if (ovr.radius) {
            out.radius = ovr.radius;
        }
        if (ovr.blur) {
            out.blur = ovr.blur;
        }
        if (ovr.progressStyle) {
            out.progressStyle = ovr.progressStyle;
        }
        if (ovr.progressColor) {
            out.progressColor = ovr.progressColor;
        }
        return out;
    }
};

struct PageDwell {
    std::optional<int> scanGrace;
    std::optional<int> dwellGrace;
    /// Present even when the sequence contains 0 (immediate fire after scan grace).
    std::optional<QVector<int>> activation;

    [[nodiscard]] bool hasAny() const
    {
        return scanGrace.has_value() || dwellGrace.has_value() || activation.has_value();
    }

    [[nodiscard]] PageDwell withOverrides(const PageDwell& ovr) const
    {
        PageDwell out = *this;
        if (ovr.scanGrace) {
            out.scanGrace = ovr.scanGrace;
        }
        if (ovr.dwellGrace) {
            out.dwellGrace = ovr.dwellGrace;
        }
        if (ovr.activation) {
            out.activation = ovr.activation;
        }
        return out;
    }
};

enum class PageActionType {
    Send,
    Click,
    Move,
    MoveAndClick,
    Command,
    Nav,
    GoBack,
    Speak,
    Ahk,
    Unknown
};

enum class PageVerb { Open, Close, Toggle };
enum class PageTargetKind { Page, Grid, Zone, Cell };
enum class PageNavScope { Id, All, Self, Others };
enum class PageZoomMode { Off, Settings, Level };
enum class PageMoveMode { Gaze, Absolute, Relative, Direction };
/// Exclusive root-shell slot. None = ordinary grid (not Docked/Drawer/Quit).
enum class PageRootSlot { None, Drawer, Quit };

struct PageAction {
    PageActionType type = PageActionType::Unknown;
    QString value;
    QString args;
    QString ahkSource;

    QString sendKey;
    QString sendEdge; // Down / Up / empty = tap
    int sendDurationMs = 0;

    PageVerb verb = PageVerb::Open;
    PageTargetKind targetKind = PageTargetKind::Page;
    PageNavScope targetScope = PageNavScope::Id;
    QString targetId;
    bool breadcrumb = false;

    QString button;
    int clickCount = 1;
    QString clickEdge;
    int speed = 0;

    PageMoveMode moveMode = PageMoveMode::Gaze;
    PageDim moveX;
    PageDim moveY;
    PageAnchor moveDirection = PageAnchor::Top;
    /// -1 = use the mouse-assist step amount.
    int moveAmount = -1;
    PageZoomMode zoomMode = PageZoomMode::Off;
    int zoomLevel = 0;

    QString command;
    QString speakText;
};

struct PageLeaf {
    QString id;
    QString styleId;
    QString dwellId;
    PageChrome style;
    PageDwell dwell;
    QString label;
    QString icon;
    QString caption;
    QString settingKey;
    QString activeState;
    QString role;
    QString cluster;
    QString clusterSlot;
    QString textStyle;
    QString visibleWhen;
    bool interactive = true;
    bool suspendExempt = false;
    bool actionLoop = false;
    /// Omitted from the live session when false. Default shown.
    bool show = true;
    bool shell = false;
    QVector<PageAction> actions;
};

struct PageCell : PageLeaf {
    int row = 0;
    int col = 0;
    int rowSpan = 1;
    int colSpan = 1;
};

struct PageGrid {
    QString id;
    bool nested = false;
    bool desktopMode = false;
    PageAnchor anchor = PageAnchor::TopLeft;
    PageDimPair offset;
    PageDimPair size;
    int rows = 1;
    int columns = 1;
    int row = 0;
    int col = 0;
    int rowSpan = 1;
    int colSpan = 1;
    int gapPx = 0;
    int marginPx = 0;
    /// Relative row heights. Missing / non-positive entries count as 1.
    QVector<double> rowWeights;
    bool drawerMotion = false;
    bool autoClose = false;
    int autoCloseIdleMs = -1;
    PageRootSlot rootSlot = PageRootSlot::None;
    /// Ordinary grids with show=false are omitted from the live session.
    /// Chrome-slot grids ignore this and follow root chrome.
    bool show = true;
    /// Root chrome: painted and hit above every non-shell Grid/Zone.
    bool shell = false;
    QString styleId;
    QString dwellId;
    PageChrome style;
    PageDwell dwell;
    QVector<PageGrid> subGrids;
    QVector<PageCell> cells;
};

[[nodiscard]] inline QVector<double> parseRowWeights(QStringView csv)
{
    QVector<double> out;
    for (QString part : csv.toString().split(QLatin1Char(','))) {
        part = part.trimmed();
        if (part.isEmpty()) {
            continue;
        }
        bool ok = false;
        const double n = part.toDouble(&ok);
        if (ok && n > 0.0) {
            out.push_back(n);
        }
    }
    return out;
}

struct PageZone : PageLeaf {
    bool desktopMode = false;
    PageAnchor anchor = PageAnchor::TopLeft;
    PageDimPair offset;
    PageDimPair size;
    PageDimPair dwellOffset;
    PageDimPair dwellSize;
};

struct PageDocument {
    QString id;
    QString name;
    bool master = false;
    bool autoClose = false;
    int autoCloseIdleMs = -1;
    PageChrome style;
    PageDwell dwell;
    QHash<QString, PageChrome> styles;
    QHash<QString, PageDwell> dwells;
    QVector<PageGrid> grids;
    QVector<PageZone> zones;

    [[nodiscard]] bool isValid() const { return !id.isEmpty(); }

    [[nodiscard]] const PageGrid* findGrid(const QString& gridId) const
    {
        return findGridIn(grids, gridId);
    }

    [[nodiscard]] PageGrid* findGrid(const QString& gridId)
    {
        return const_cast<PageGrid*>(static_cast<const PageDocument*>(this)->findGrid(gridId));
    }

    [[nodiscard]] const PageCell* findCell(const QString& cellId) const
    {
        return findCellIn(grids, cellId);
    }

    [[nodiscard]] PageCell* findCell(const QString& cellId)
    {
        return const_cast<PageCell*>(static_cast<const PageDocument*>(this)->findCell(cellId));
    }

    [[nodiscard]] const PageZone* findZone(const QString& zoneId) const
    {
        for (const PageZone& z : zones) {
            if (z.id == zoneId) {
                return &z;
            }
        }
        return nullptr;
    }

    [[nodiscard]] PageZone* findZone(const QString& zoneId)
    {
        return const_cast<PageZone*>(static_cast<const PageDocument*>(this)->findZone(zoneId));
    }

private:
    [[nodiscard]] static const PageGrid* findGridIn(const QVector<PageGrid>& nodes,
                                                    const QString& gridId)
    {
        for (const PageGrid& g : nodes) {
            if (!gridId.isEmpty() && g.id == gridId) {
                return &g;
            }
            if (const PageGrid* nested = findGridIn(g.subGrids, gridId)) {
                return nested;
            }
        }
        return nullptr;
    }

    [[nodiscard]] static const PageCell* findCellIn(const QVector<PageGrid>& nodes,
                                                    const QString& cellId)
    {
        if (cellId.isEmpty()) {
            return nullptr;
        }
        for (const PageGrid& g : nodes) {
            for (const PageCell& c : g.cells) {
                if (c.id == cellId) {
                    return &c;
                }
            }
            if (const PageCell* nested = findCellIn(g.subGrids, cellId)) {
                return nested;
            }
        }
        return nullptr;
    }
};

} // namespace gazer
