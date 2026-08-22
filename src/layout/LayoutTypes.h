#pragma once

#include <QColor>
#include <QHash>
#include <QString>
#include <QtGlobal>
#include <QVector>
#include <memory>
#include <optional>

namespace gazer {

/// Canonical size/offset unit for layouts and dwell regions (sole runtime geometry).
/// JSON: `x`/`width` = percent of reference (or `"N%"`); `xPx`/`widthPx` = pixels (wins if both).
/// Board-local bare numbers are forced to pixels at load for legacy layouts.
/// After load, only DimSpec is used — no parallel widthPx/heightPx runtime fields.
struct DimSpec {
    enum class Unit { Unset, Percent, Pixels };
    Unit unit = Unit::Unset;
    double value = 0.0;

    [[nodiscard]] bool isSet() const { return unit != Unit::Unset; }

    [[nodiscard]] static DimSpec pixels(double px)
    {
        DimSpec d;
        d.unit = Unit::Pixels;
        d.value = px;
        return d;
    }

    [[nodiscard]] static DimSpec percent(double pct)
    {
        DimSpec d;
        d.unit = Unit::Percent;
        d.value = pct;
        return d;
    }

    /// Resolve against a reference length (screen/board width or height).
    [[nodiscard]] double resolve(double reference) const
    {
        if (unit == Unit::Pixels) {
            return value;
        }
        if (unit == Unit::Percent) {
            return reference * (value / 100.0);
        }
        return 0.0;
    }

    [[nodiscard]] int resolveInt(double reference, int fallback = 0) const
    {
        if (!isSet()) {
            return fallback;
        }
        return qRound(resolve(reference));
    }
};

/// Action attached to a layout item (schema v1+).
struct LayoutAction {
    enum class Type {
        Speak,
        TypeText,    // inject unicode into focused app (no TTS)
        LoadLayout,  // navigate (see ActionDispatcher / session verbs)
        OpenLayout,  // open a simultaneous secondary instance
        CloseLayout, // close the activating instance (not last / not master alone)
        Command,
        Script,
        Unknown
    };

    Type type = Type::Unknown;
    QString text;      // speak / typeText
    QString layoutId;  // loadLayout / openLayout
    QString name;      // command
    QString source;    // script
    /// Optional delay before this step runs (ms). Used in action series / loops.
    int delayMs = 0;
};

/// Reference rect for % size/position: work area (desktop) vs full screen geometry.
enum class BoundsMode {
    Desktop, ///< availableGeometry — typically excludes taskbar (default)
    Screen   ///< full screen geometry
};

/// Visual chrome for a board window or item cell. Full transparency via #AARRGGBB (alpha 0).
struct LayoutChromeStyle {
    std::optional<QColor> background;
    std::optional<QColor> foreground;
    std::optional<QColor> borderColor;
    /// Border thickness in px. Unset → 0 for window chrome; item cells use paint defaults.
    std::optional<double> borderWidth;
    /// Corner radius in px. Unset → theme/paint default.
    std::optional<double> radius;
    /// Frosted-glass blur of content behind this chrome, in px. 0 / unset = off.
    /// JSON: `blur`, `blurRadius`, or `glass` (true → kDefaultBlur, or a number).
    std::optional<double> blur;

    static constexpr double kDefaultBlur = 15.0;
    static constexpr double kMaxBlur = 64.0;

    [[nodiscard]] bool hasBlur() const { return blur.has_value() && *blur > 0.0; }

    [[nodiscard]] bool hasAny() const
    {
        return background.has_value() || foreground.has_value() || borderColor.has_value()
               || borderWidth.has_value() || radius.has_value() || blur.has_value();
    }

    /// Copy, then replace any field set on `ovr` (item over layout, layout over unset).
    [[nodiscard]] LayoutChromeStyle withOverrides(const LayoutChromeStyle& ovr) const
    {
        LayoutChromeStyle out = *this;
        if (ovr.background) {
            out.background = ovr.background;
        }
        if (ovr.foreground) {
            out.foreground = ovr.foreground;
        }
        if (ovr.borderColor) {
            out.borderColor = ovr.borderColor;
        }
        if (ovr.borderWidth) {
            out.borderWidth = ovr.borderWidth;
        }
        if (ovr.radius) {
            out.radius = ovr.radius;
        }
        if (ovr.blur) {
            out.blur = ovr.blur;
        }
        return out;
    }
};

using LayoutItemStyle = LayoutChromeStyle;

/// Runtime widget kind. JSON `role` / `cluster` are mapped once at load.
enum class LayoutItemKind {
    Button,
    Label,
    Tab,
    Toggle,
    Slider,
    Preview,
    Stepper,
    Segment
};

/// Hit geometry for free items. JSON: `screenAnchor` plus `x`/`xPx`/… on the item
/// (nested `dwellRegion` still loads). `screenAnchor` omitted / empty = cell.
struct LayoutDwellRegion {
    enum class ScreenAnchor {
        None, // cell (row/col on the board); not a free item
        Top,
        Bottom,
        Left,
        Right,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight,
        TopCenter,
        BottomCenter,
        LeftCenter,
        RightCenter
    };

    ScreenAnchor screenAnchor = ScreenAnchor::None;
    /// Offset from the screen edge when `screenAnchor` is set.
    DimSpec x;
    DimSpec y;
    DimSpec width;
    DimSpec height;
    /// Deprecated: outward gap when x/y unset for screen anchors (compat).
    int marginPx = 4;
    /// Optional override for anchor/% resolution reference.
    bool hasBoundsMode = false;
    BoundsMode boundsMode = BoundsMode::Desktop;

    [[nodiscard]] bool isValid() const
    {
        // Unset size is allowed (runtime defaults apply). Invalid only if set to non-positive px.
        if (width.isSet() && width.unit == DimSpec::Unit::Pixels && width.value <= 0) {
            return false;
        }
        if (height.isSet() && height.unit == DimSpec::Unit::Pixels && height.value <= 0) {
            return false;
        }
        return true;
    }
    [[nodiscard]] bool usesScreenAnchor() const { return screenAnchor != ScreenAnchor::None; }
};

/// Dwell / progress settings. Used globally (AppSettings), per-layout, or per-item.
/// Timing/grace resolution: item (`has*`) → layout (`has*`) → AppSettings → built-in default.
struct LayoutDwellConfig {
    bool enabled = true;
    /// First step / legacy single value (kept for older code paths).
    int ms = 800;
    /// Progressive dwell steps in ms. Empty → {ms}. Last step repeats activation
    /// at that interval until gaze leaves (does not cycle earlier steps).
    QVector<int> msSequence;
    /// Obsolete — ignored when sequence is used.
    int repeatMs = 0;
    /// Comma-separated styles: radial, fill, border (any combination).
    QString progressStyle = QStringLiteral("radial");
    /// Optional blink grace override (ms). -1 = inherit parent.
    int graceMs = -1;
    /// Optional scan grace override (ms): time on-target before dwell progress starts.
    /// -1 = inherit parent / AppSettings (default 100).
    int scanGraceMs = -1;
    /// Optional color overrides (empty = inherit). Hex #RRGGBB or #AARRGGBB.
    QString progressColor;
    QString fillColor;
    QString borderColor;
    QString flashColor;
    int flashMs = -1; // -1 = inherit

    /// True when this config was explicitly present in JSON (layout or item).
    bool sectionPresent = false;
    bool hasTiming = false;
    bool hasProgressStyle = false;
    bool hasGrace = false;
    bool hasScanGrace = false;
    bool hasProgressColor = false;
    bool hasFillColor = false;
    bool hasBorderColor = false;
    bool hasFlashColor = false;
    bool hasFlashMs = false;

    [[nodiscard]] QVector<int> effectiveSequence() const
    {
        if (!msSequence.isEmpty()) {
            return msSequence;
        }
        return {ms > 0 ? ms : 800};
    }

    /// Hold-time sequence: item → layout → global → layout default ({800}).
    [[nodiscard]] static QVector<int> resolveSequence(const LayoutDwellConfig* item,
                                                      const LayoutDwellConfig& layout,
                                                      const QVector<int>& global)
    {
        if (item && item->hasTiming) {
            return item->effectiveSequence();
        }
        if (layout.hasTiming) {
            return layout.effectiveSequence();
        }
        if (!global.isEmpty()) {
            return global;
        }
        return layout.effectiveSequence();
    }

    /// Optional ms field (scan / blink grace): item → layout → global → fallback.
    [[nodiscard]] static int resolveOverrideMs(bool itemHas, int itemVal, bool layoutHas,
                                               int layoutVal, bool globalHas, int globalVal,
                                               int fallback)
    {
        if (itemHas && itemVal >= 0) {
            return itemVal;
        }
        if (layoutHas && layoutVal >= 0) {
            return layoutVal;
        }
        if (globalHas) {
            return globalVal;
        }
        return fallback;
    }
};

struct LayoutItem {
    QString id;
    QString label;
    /// Secondary line under the label (e.g. muted description).
    QString caption;
    /// Optional AppSettings field key for live value text on label cells.
    QString settingKey;
    /// Runtime toggle key for accent "on" state (e.g. mouse.leftHold, lookToScroll).
    QString activeState;
    /// Built-in glyph key for the board painter (e.g. "leftClick", "moveTo"). Empty = text only.
    QString icon;
    /// JSON role string. Maps to `kind` (`label`, `tab`, `toggle`, …). Not placement.
    QString role;
    LayoutItemKind kind = LayoutItemKind::Button;
    /// Fluent type ramp for labels: caption, body, bodyStrong, subtitle, title, section.
    QString textStyle;
    /// Shared group id for stepper/segment members (JSON `cluster`).
    QString cluster;
    /// Slot inside the group: dec / value / inc / edit.
    QString clusterSlot;
    /// When false, cell is visual-only (not dwell/click hit-tested).
    bool interactive = true;
    /// When true, remains dwellable while global dwell suspend is on.
    bool dwellExempt = false;
    int row = 0;
    int col = 0;
    int rowSpan = 1;
    int colSpan = 1;
    /// Voice-style relative width within the row (letter unit = 1). 0 = use equal col cells.
    double widthUnits = 0.0;
    /// Single action (legacy). Prefer `actions` when non-empty.
    LayoutAction action;
    /// Ordered series of actions. When non-empty, used instead of `action`.
    QVector<LayoutAction> actions;
    /// Sticky series loop: first activate starts ActionLoopService; re-activate stops
    /// (shared sticky policy with assist modes like gaze click loop).
    bool actionLoop = false;
    LayoutItemStyle style;
    /// Optional per-item dwell / progress overrides (item > layout > global).
    LayoutDwellConfig dwell;
    /// Free-item placement. `screenAnchor == None` means the item is a cell.
    LayoutDwellRegion dwellRegion;
    /// Item kind: empty/"cell" (default) or "layout" (embed another file in this cell).
    QString type;
    /// When type is "layout", catalog id to embed.
    QString embedLayoutId;
    /// Static visibility (default true). Combined with visibleWhen.
    bool visible = true;
    /// Visibility predicate: empty, `ident`, or `!ident` (expanded, quitConfirm, dwellSuspend).
    QString visibleWhen;

    void applyKind()
    {
        const QString r = role.toLower();
        if (cluster.startsWith(QLatin1String("segment"), Qt::CaseInsensitive)) {
            kind = LayoutItemKind::Segment;
        } else if (cluster.startsWith(QLatin1String("stepper"), Qt::CaseInsensitive)) {
            kind = LayoutItemKind::Stepper;
        } else if (r == QLatin1String("tab")) {
            kind = LayoutItemKind::Tab;
        } else if (r == QLatin1String("toggle")) {
            kind = LayoutItemKind::Toggle;
        } else if (r == QLatin1String("slider")) {
            kind = LayoutItemKind::Slider;
        } else if (r == QLatin1String("preview")) {
            kind = LayoutItemKind::Preview;
        } else if (r == QLatin1String("label") || r == QLatin1String("value")
                   || r == QLatin1String("display") || r == QLatin1String("input")) {
            kind = LayoutItemKind::Label;
        } else {
            kind = LayoutItemKind::Button;
        }
    }

    /// Free item (not a cell). Determined only by `dwellRegion.screenAnchor`.
    [[nodiscard]] bool isUnbounded() const { return dwellRegion.usesScreenAnchor(); }

    void setAnchor(LayoutDwellRegion::ScreenAnchor a)
    {
        dwellRegion.screenAnchor = a;
        if (a == LayoutDwellRegion::ScreenAnchor::None) {
            return;
        }
        if (!dwellRegion.width.isSet()) {
            dwellRegion.width = DimSpec::pixels(160);
        }
        if (!dwellRegion.height.isSet()) {
            dwellRegion.height = DimSpec::pixels(48);
        }
    }

    [[nodiscard]] bool isClustered() const
    {
        return kind == LayoutItemKind::Stepper || kind == LayoutItemKind::Segment;
    }

    [[nodiscard]] bool isPageChrome() const
    {
        if (kind == LayoutItemKind::Tab) {
            return true;
        }
        if (kind != LayoutItemKind::Label) {
            return false;
        }
        const QString ts = textStyle.toLower();
        return ts == QLatin1String("section") || ts == QLatin1String("title");
    }

    [[nodiscard]] bool isEmbed() const
    {
        return type.compare(QLatin1String("layout"), Qt::CaseInsensitive) == 0
               || !embedLayoutId.isEmpty();
    }

    /// Cells only. Free items are laid out in screen space.
    [[nodiscard]] bool participatesInBoardGrid() const { return !isUnbounded(); }

    /// Survives global dwell suspend (set via dwellExempt in JSON / loader).
    [[nodiscard]] bool isDwellExempt() const { return dwellExempt; }

    /// Actions to run on activate: `actions` if non-empty, else single `action` if known.
    [[nodiscard]] QVector<LayoutAction> effectiveActions() const
    {
        if (!actions.isEmpty()) {
            return actions;
        }
        if (action.type != LayoutAction::Type::Unknown) {
            return {action};
        }
        return {};
    }

    /// Key used for loop active-state accent (activeState, or auto from item id).
    [[nodiscard]] QString loopActiveStateKey() const
    {
        if (!activeState.isEmpty()) {
            return activeState;
        }
        if (actionLoop) {
            return QStringLiteral("loop.%1").arg(id);
        }
        return {};
    }
};

struct LayoutGrid {
    int columns = 1;
    int rows = 1;
    int gapPx = 8;
    /// Uniform pixel inset when a side DimSpec is unset.
    int marginPx = 0;
    /// Left/right inset. Bare JSON number = percent of board width.
    DimSpec marginX;
    /// Top/bottom inset. Bare JSON number = percent of board height.
    DimSpec marginY;
    /// When true (or any item has widthUnits > 0), each row is laid out by widthUnits.
    bool unitRows = false;

    struct Insets {
        double left = 0;
        double top = 0;
        double right = 0;
        double bottom = 0;
    };

    [[nodiscard]] Insets insets(double boardW, double boardH) const
    {
        Insets i;
        const double hx = marginX.isSet() ? marginX.resolve(boardW) : double(marginPx);
        const double vy = marginY.isSet() ? marginY.resolve(boardH) : double(marginPx);
        i.left = hx;
        i.right = hx;
        i.top = vy;
        i.bottom = vy;
        return i;
    }
};

/// Initial board placement on the available desktop (above taskbar).
struct LayoutWindowPlacement {
    enum class Anchor {
        Default,
        TopLeft,
        TopCenter,
        TopRight,
        Center,
        LeftCenter,
        RightCenter,
        BottomLeft,
        BottomCenter,
        BottomRight
    };

    Anchor anchor = Anchor::Default;
    /// Size / position as DimSpec only (loader maps legacy widthPx/heightPx into these).
    DimSpec width;
    DimSpec height;
    DimSpec x;
    DimSpec y;
    int marginPx = 0;
    /// True when the layout JSON included a "window" object.
    bool specified = false;
    /// Explicit "window": { "hidden": true } — force no board chrome.
    bool hidden = false;
    /// Size/position against available desktop vs full screen geometry.
    bool hasBoundsMode = false;
    BoundsMode boundsMode = BoundsMode::Desktop;
    /// Window panel chrome (fill, border, radius). Alpha 0 = fully transparent board.
    LayoutChromeStyle style;
    /// Keep this board in the topmost band above the taskbar while visible.
    bool aboveTaskbar = false;
    /// Bottom-anchored scale 0→1 on show and 1→0 on hide.
    bool drawerMotion = false;
};

/// Declared child of a parent layout (owned instance; show/hide, not document swap).
struct LayoutChildRef {
    QString id;
    QString layoutId;
    bool visible = true;
    QString visibleWhen;
};

/// Parsed layout document (resources/layouts/*.json).
struct LayoutDocument {
    int schemaVersion = 1;
    QString id;
    QString name;
    QString description;
    /// True on the never-destroyed process-lifetime root (main_master).
    bool master = false;
    /// Dock chips stay hidden until a bottom-edge gaze reveal dwell completes.
    bool hideUntilGazeReveal = false;
    QVector<LayoutChildRef> children;
    /// Nested documents keyed by host item id (filled when the catalog resolves embeds).
    QHash<QString, std::shared_ptr<LayoutDocument>> embeds;
    LayoutGrid grid;
    LayoutDwellConfig dwell;
    LayoutWindowPlacement placement;
    /// Default item chrome. Item `style` wins per field; remaining unset fields use the theme.
    LayoutChromeStyle style;
    QVector<LayoutItem> items;
    /// Run when an instance is first created/opened (not on in-place load).
    QVector<LayoutAction> onOpen;
    /// Run when a document is applied into an instance (open and loadInto).
    QVector<LayoutAction> onLoad;
    /// Run before an instance is closed (instance id still valid).
    QVector<LayoutAction> onClose;
    /// Layout-level default for window + item placement when not overridden.
    bool hasBoundsMode = false;
    BoundsMode boundsMode = BoundsMode::Desktop;
    /// Secondaries: idle without dwell → instant 50% hold → 500ms dismiss shrink → close.
    /// Master shells default false.
    bool autoClose = true;
    /// -1 = use AppSettings defaults.
    int autoCloseIdleMs = -1;
    int autoCloseFadeMs = -1;

    [[nodiscard]] bool isValid() const
    {
        return !id.isEmpty() && grid.columns > 0 && grid.rows > 0;
    }

    [[nodiscard]] BoundsMode effectiveBoundsMode() const
    {
        if (placement.hasBoundsMode) {
            return placement.boundsMode;
        }
        if (hasBoundsMode) {
            return boundsMode;
        }
        return BoundsMode::Desktop;
    }

    /// Show board HWND? Explicit hidden → no. Explicit window → yes.
    /// No window block: show if any grid cell item exists; hide items-only layouts.
    [[nodiscard]] bool showsBoardWindow() const
    {
        if (placement.hidden) {
            return false;
        }
        if (placement.specified) {
            return true;
        }
        for (const LayoutItem& item : items) {
            if (item.participatesInBoardGrid()) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] double maxChromeBlur() const
    {
        double r = qMax(placement.style.blur.value_or(0.0), style.blur.value_or(0.0));
        for (const LayoutItem& item : items) {
            r = qMax(r, style.withOverrides(item.style).blur.value_or(0.0));
        }
        return r;
    }

    [[nodiscard]] bool isMaster() const { return master; }
    [[nodiscard]] bool isGazeRevealDock() const { return master && hideUntilGazeReveal; }

    [[nodiscard]] const LayoutItem* findItem(const QString& itemId) const
    {
        for (const LayoutItem& it : items) {
            if (it.id == itemId) {
                return &it;
            }
        }
        for (auto e = embeds.cbegin(); e != embeds.cend(); ++e) {
            if (e.value() && e.value().get() != this) {
                if (const LayoutItem* nested = e.value()->findItem(itemId)) {
                    return nested;
                }
            }
        }
        return nullptr;
    }
};

} // namespace gazer
