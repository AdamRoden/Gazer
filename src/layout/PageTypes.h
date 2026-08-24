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
struct PageDim {
    enum class Unit { Unset, Pixels, Proportion, HeightProportion };

    Unit unit = Unit::Unset;
    double value = 0.0;

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

    /// @p axisRef is width for x / height for y. @p heightRef is the box height
    /// (used by HeightProportion on either axis). One-arg form uses @p axisRef for both.
    [[nodiscard]] double resolve(double axisRef) const { return resolve(axisRef, axisRef); }

    [[nodiscard]] double resolve(double axisRef, double heightRef) const
    {
        if (unit == Unit::Pixels) {
            return value;
        }
        if (unit == Unit::HeightProportion) {
            return heightRef * value;
        }
        if (unit == Unit::Proportion) {
            return axisRef * value;
        }
        return 0.0;
    }
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

    [[nodiscard]] bool hasBlur() const { return blur.has_value() && *blur > 0.0; }

    [[nodiscard]] PageBox resolvedRadius(bool clustered = false) const
    {
        return radius.value_or(PageBox::all(clustered ? 4.0 : 8.0));
    }

    [[nodiscard]] bool hasAny() const
    {
        return background.has_value() || foreground.has_value() || borderColor.has_value()
               || thickness.has_value() || radius.has_value() || blur.has_value()
               || progressStyle.has_value();
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
    Page,
    Click,
    Move,
    MoveAndClick,
    Command,
    Speak,
    Ahk,
    Unknown
};

enum class PageVerb { Open, Close, Toggle };
enum class PageTargetKind { Page, Grid, Zone };
enum class PageMoveMode { Gaze, Absolute, Relative };
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
    QString targetId;

    QString button;
    int clickCount = 1;
    QString clickEdge;
    int speed = 0;

    PageMoveMode moveMode = PageMoveMode::Gaze;
    PageDim moveX;
    PageDim moveY;
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
    bool dwellExempt = false;
    bool actionLoop = false;
    bool visible = true;
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
    bool aboveTaskbar = false;
    bool drawerMotion = false;
    bool autoClose = false;
    int autoCloseIdleMs = -1;
    PageRootSlot rootSlot = PageRootSlot::None;
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
    bool aboveTaskbar = false;
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

    [[nodiscard]] const PageZone* findZone(const QString& zoneId) const
    {
        for (const PageZone& z : zones) {
            if (z.id == zoneId) {
                return &z;
            }
        }
        return nullptr;
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
};

} // namespace gazer
