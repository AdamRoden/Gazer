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

    [[nodiscard]] bool hasAny() const
    {
        return background.has_value() || foreground.has_value() || borderColor.has_value()
               || borderWidth.has_value() || radius.has_value();
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
        return out;
    }
};

using LayoutItemStyle = LayoutChromeStyle;

/// Optional dwell region for unbounded items (not clipped to board / can sit off-screen).
struct LayoutDwellRegion {
    enum class ScreenAnchor {
        None, // use board-local x,y (may extend outside the window)
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
    /// Position relative to screen anchor (or board origin when None).
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
/// When a field's "has*" flag is true, it overrides the parent level.
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
    /// Optional role: label / display / value / slider / preview / swatch.
    QString role;
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
    /// When true, dwell hit-test is not limited to the board window rect.
    bool unbounded = false;
    /// Custom dwell rect (board-local or screen-edge). Valid when width/height > 0 and set.
    LayoutDwellRegion dwellRegion;
    bool hasDwellRegion = false;
    /// Item kind: empty/"cell" (default) or "layout" (embed another file in this cell).
    QString type;
    /// When type is "layout", catalog id to embed.
    QString embedLayoutId;
    /// Static visibility (default true). Combined with visibleWhen.
    bool visible = true;
    /// Visibility predicate: empty, `ident`, or `!ident` (expanded, quitConfirm, dwellSuspend).
    QString visibleWhen;

    [[nodiscard]] bool isEmbed() const
    {
        return type.compare(QLatin1String("layout"), Qt::CaseInsensitive) == 0
               || !embedLayoutId.isEmpty();
    }

    /// Board-less affordances (edge/off-screen dwell) are not painted or hit-tested as grid cells.
    [[nodiscard]] bool participatesInBoardGrid() const
    {
        return !unbounded && !hasDwellRegion;
    }

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
    int marginPx = 0;
    /// When true (or any item has widthUnits > 0), each row is laid out by widthUnits.
    bool unitRows = false;
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

/// Visual chrome for the board window.
enum class LayoutUiStyle {
    Default,
    Fluent // Material/Fluent-inspired cards, captions, tooltips
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
    LayoutUiStyle uiStyle = LayoutUiStyle::Default;
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
    /// Secondaries: idle without dwell → instant 50% hold → suck to bottom-center → close.
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
