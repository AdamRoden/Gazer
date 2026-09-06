#pragma once

#include "layout/PageBox.h"
#include "layout/ProgressStyle.h"

#include <QColor>
#include <QHash>
#include <QSet>
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

/// One authored chrome color: empty, `#RRGGBB` / `#AARRGGBB`, a theme role, or a brand key.
/// Resolved at paint from the live theme.
struct PageColor {
    QString token;

    PageColor() = default;
    PageColor(const QColor& c) { *this = c; }

    [[nodiscard]] bool isSet() const { return !token.isEmpty(); }
    [[nodiscard]] explicit operator bool() const { return isSet(); }

    void reset() { token.clear(); }

    void overlay(const PageColor& ovr)
    {
        if (ovr.isSet()) {
            *this = ovr;
        }
    }

    PageColor& operator=(const QColor& c)
    {
        if (!c.isValid()) {
            token.clear();
        } else if (c.alpha() < 255) {
            token = c.name(QColor::HexArgb);
        } else {
            token = c.name(QColor::HexRgb);
        }
        return *this;
    }

    [[nodiscard]] QColor parsed() const { return token.isEmpty() ? QColor() : QColor(token); }
};

struct PageChrome {
    PageColor background;
    PageColor foreground;
    PageColor borderColor;
    std::optional<PageBox> thickness;
    std::optional<PageBox> radius;
    std::optional<double> blur;
    std::optional<ProgressStyle> progressStyle;
    PageColor progressColor;

    [[nodiscard]] bool hasBlur() const { return blur.has_value() && *blur > 0.0; }

    static constexpr double kDefaultThickness = 1.0;
    static constexpr double kDefaultRadius = 8.0;

    [[nodiscard]] static PageChrome defaults()
    {
        PageChrome c;
        c.thickness = PageBox::all(kDefaultThickness);
        return c;
    }

    [[nodiscard]] PageBox resolvedRadius() const
    {
        return radius.value_or(PageBox::all(kDefaultRadius));
    }

    [[nodiscard]] PageBox resolvedThickness() const
    {
        return thickness.value_or(PageBox::all(kDefaultThickness));
    }

    [[nodiscard]] bool hasAny() const
    {
        return background.isSet() || foreground.isSet() || borderColor.isSet()
               || thickness.has_value() || radius.has_value() || blur.has_value()
               || progressStyle.has_value() || progressColor.isSet();
    }

    /// Grids paint a surface only. Labels, dwell rings, and progress never use these.
    [[nodiscard]] PageChrome withoutItemPaint() const
    {
        PageChrome out = *this;
        out.foreground.reset();
        out.progressStyle.reset();
        out.progressColor.reset();
        return out;
    }

    [[nodiscard]] PageChrome withOverrides(const PageChrome& ovr) const
    {
        PageChrome out = *this;
        out.background.overlay(ovr.background);
        out.foreground.overlay(ovr.foreground);
        out.borderColor.overlay(ovr.borderColor);
        out.progressColor.overlay(ovr.progressColor);
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
    Click,
    Move,
    MoveAndClick,
    Command,
    Nav,
    ShowLayers,
    GoBack,
    Speak,
    Ahk,
    Unknown
};

enum class PageVerb { Open, Close, Toggle };
enum class PageNavScope { Id, All, Self, Others };
enum class PageZoomMode { Off, Settings, Level, Foresight, ForesightBonus };
enum class PageMoveMode { Gaze, Absolute, Relative, Direction };
enum class PageClickKind { Default, Double, Down, Up, Toggle };

struct PageAction {
    PageActionType type = PageActionType::Unknown;
    QString value;
    QString args;
    QString ahkSource;

    QString sendKey;
    QString sendEdge; // Down / Up / empty = tap
    int sendDurationMs = 0;

    PageVerb verb = PageVerb::Open;
    PageNavScope targetScope = PageNavScope::Id;
    QString targetId;
    bool breadcrumb = false;

    QString button;
    PageClickKind clickKind = PageClickKind::Default;

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
    /// ShowLayers: replace the page's visible set with these numbers.
    QVector<int> layers;
};

/// Default dwell for keys, mouse inject, composer typing, AHK, and mapping commands.
/// Settings / nav / assist toggles use designer dwell instead.
[[nodiscard]] inline bool isDailyDriverCommand(QStringView name)
{
    const QString n = name.toString();
    if (n.isEmpty()) {
        return false;
    }
    if (n.startsWith(QLatin1String("compose.removeWord."))
        || n == QLatin1String("compose.backspace")
        || n == QLatin1String("compose.deleteWord")) {
        return true;
    }
    if (n.startsWith(QLatin1String("settings.")) || n.startsWith(QLatin1String("theme."))
        || n.startsWith(QLatin1String("speech.")) || n.startsWith(QLatin1String("history."))
        || n.startsWith(QLatin1String("soundboard.")) || n.startsWith(QLatin1String("compose."))
        || n.startsWith(QLatin1String("lts.")) || n.startsWith(QLatin1String("gazer."))
        || n.startsWith(QLatin1String("toggle"))) {
        return false;
    }
    static const QSet<QString> kChrome{
        QStringLiteral("quitApp"),
        QStringLiteral("openPageEditor"),
        QStringLiteral("openPreview"),
        QStringLiteral("suspendDwell"),
        QStringLiteral("resumeDwell"),
        QStringLiteral("stopAllActionLoops"),
    };
    return !kChrome.contains(n);
}

[[nodiscard]] inline bool usesDailyDriverDwell(const QVector<PageAction>& actions)
{
    for (const PageAction& a : actions) {
        switch (a.type) {
        case PageActionType::Send:
        case PageActionType::Click:
        case PageActionType::Move:
        case PageActionType::MoveAndClick:
        case PageActionType::Ahk:
            return true;
        case PageActionType::Command:
            if (isDailyDriverCommand(a.command)) {
                return true;
            }
            break;
        default:
            break;
        }
    }
    return false;
}

/// label / value / display / slider / preview / scrollbar are not dwell targets.
[[nodiscard]] inline bool pageRoleIsPassive(QStringView role)
{
    const QString r = role.toString().trimmed().toLower();
    return r == QLatin1String("label") || r == QLatin1String("value")
           || r == QLatin1String("display") || r == QLatin1String("slider")
           || r == QLatin1String("preview") || r == QLatin1String("scrollbar");
}

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
    QString textStyle;
    QString visibleWhen;
    bool suspendExempt = false;
    bool actionLoop = false;
    /// Zones only. Cells take shell from their grid.
    bool shell = false;
    QVector<PageAction> actions;

    /// Dwell/click target. Passive roles are not. A tab with no actions is the
    /// current tab (selected, not a navigation target).
    [[nodiscard]] bool isInteractive() const
    {
        if (role.compare(QLatin1String("slider"), Qt::CaseInsensitive) == 0 && !actions.isEmpty()) {
            return true;
        }
        if (pageRoleIsPassive(role)) {
            return false;
        }
        if (role.compare(QLatin1String("tab"), Qt::CaseInsensitive) == 0 && actions.isEmpty()) {
            return false;
        }
        return true;
    }
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
    /// Membership in page layers (`layers="1,2"`). Default `{1}`.
    QVector<int> layers{1};
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

/// False if any token is not an integer >= 1. Empty input yields an empty list (true).
[[nodiscard]] inline bool parseLayerListStrict(QStringView csv, QVector<int>& out)
{
    out.clear();
    for (QString part : csv.toString().split(QLatin1Char(','))) {
        part = part.trimmed();
        if (part.isEmpty()) {
            continue;
        }
        bool ok = false;
        const int n = part.toInt(&ok);
        if (!ok || n < 1) {
            out.clear();
            return false;
        }
        if (!out.contains(n)) {
            out.push_back(n);
        }
    }
    return true;
}

[[nodiscard]] inline QVector<int> parseLayerList(QStringView csv)
{
    QVector<int> out;
    (void)parseLayerListStrict(csv, out);
    return out;
}

[[nodiscard]] inline QString layerListCsv(const QVector<int>& layers)
{
    QStringList parts;
    parts.reserve(layers.size());
    for (int n : layers) {
        parts.push_back(QString::number(n));
    }
    return parts.join(QLatin1Char(','));
}

[[nodiscard]] inline QVector<int> defaultLayers()
{
    return {1};
}

[[nodiscard]] inline bool isDefaultLayerList(const QVector<int>& layers)
{
    return layers.isEmpty() || (layers.size() == 1 && layers[0] == 1);
}

/// Empty means layer 1. A page always has a visible set.
[[nodiscard]] inline QVector<int> normalizedLayers(const QVector<int>& layers)
{
    return layers.isEmpty() ? defaultLayers() : layers;
}

[[nodiscard]] inline bool layersVisible(const QVector<int>& itemLayers, const QVector<int>& shown)
{
    const QVector<int> item = normalizedLayers(itemLayers);
    const QVector<int> vis = normalizedLayers(shown);
    for (int n : item) {
        if (vis.contains(n)) {
            return true;
        }
    }
    return false;
}

struct PageZone : PageLeaf {
    bool desktopMode = false;
    PageAnchor anchor = PageAnchor::TopLeft;
    PageDimPair offset;
    PageDimPair size;
    PageDimPair dwellOffset;
    PageDimPair dwellSize;
    /// Membership in page layers (`layers="1,2"`). Default `{1}`.
    QVector<int> layers{1};
};

struct PageDocument {
    QString id;
    QString name;
    bool master = false;
    bool autoClose = false;
    /// Layers visible when the page opens. Default `{1}`. Live session mutates this.
    QVector<int> showLayers{1};
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
