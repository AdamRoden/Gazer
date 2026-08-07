#pragma once

#include <QColor>
#include <QString>
#include <QtGlobal>
#include <QVector>
#include <optional>

namespace gazer {

/// Dimension: percent of a reference size (screen or board) or absolute pixels.
/// JSON: bare/percent field (`x`, `width`, …) = percent; `xPx`/`widthPx` = pixels (wins if both).
struct DimSpec {
    enum class Unit { Unset, Percent, Pixels };
    Unit unit = Unit::Unset;
    double value = 0.0;

    [[nodiscard]] bool isSet() const { return unit != Unit::Unset; }

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

struct LayoutItemStyle {
    std::optional<QColor> background;
    std::optional<QColor> foreground;
};

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
    /// Position relative to screen anchor (or board origin when None). Prefer DimSpec.
    DimSpec x;
    DimSpec y;
    DimSpec width;
    DimSpec height;
    /// Deprecated: outward gap when x/y unset for screen anchors (compat).
    int marginPx = 4;
    /// Legacy absolute fallbacks when DimSpec unset (loader fills from widthPx etc.).
    int widthPx = 80;
    int heightPx = 80;

    [[nodiscard]] bool isValid() const
    {
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
    /// Optional color overrides (empty = inherit). Hex #RRGGBB or #AARRGGBB.
    QString progressColor;
    QString fillColor;
    QString borderColor;
    QString flashBorderColor;
    QString flashFillColor;
    int flashMs = -1; // -1 = inherit

    /// True when this config was explicitly present in JSON (layout or item).
    bool sectionPresent = false;
    bool hasTiming = false;
    bool hasProgressStyle = false;
    bool hasGrace = false;
    bool hasProgressColor = false;
    bool hasFillColor = false;
    bool hasBorderColor = false;
    bool hasFlashBorderColor = false;
    bool hasFlashFillColor = false;
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
    /// Optional help text (legacy; prefer a separate non-interactive label cell).
    QString tooltip;
    /// Optional AppSettings field key for live value text on label cells.
    QString settingKey;
    /// Runtime toggle key for accent "on" state (e.g. mouse.leftHold, lookToScroll).
    QString activeState;
    /// Built-in glyph key for LayoutWindow (e.g. "leftClick", "moveTo"). Empty = text only.
    QString icon;
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
    /// When true: first activate starts a perpetual series loop; second stops (like magnifier).
    bool actionLoop = false;
    LayoutItemStyle style;
    /// Optional per-item dwell / progress overrides (item > layout > global).
    LayoutDwellConfig dwell;
    /// When true, dwell hit-test is not limited to the board window rect.
    bool unbounded = false;
    /// Custom dwell rect (board-local or screen-edge). Valid when width/height > 0 and set.
    LayoutDwellRegion dwellRegion;
    bool hasDwellRegion = false;

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
    int marginPx = 16;
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
    /// Size: percent of available desktop or pixels. Legacy widthPx/heightPx filled when only px set.
    DimSpec width;
    DimSpec height;
    DimSpec x;
    DimSpec y;
    int widthPx = 0;
    int heightPx = 0;
    int marginPx = 8;
    /// True when the layout JSON included a "window" object.
    bool specified = false;
    /// Explicit "window": { "hidden": true } — force no board chrome.
    bool hidden = false;
};

/// Session role — engine never matches content filenames.
enum class LayoutRole {
    Secondary,   // normal multi-instance board
    MasterShell  // unique instance shared by a masterGroup (main/dock/quit)
};

/// Product-agnostic session metadata from layout JSON "session" object.
struct LayoutSessionMeta {
    LayoutRole role = LayoutRole::Secondary;
    /// Shells with the same non-empty group share one live instance.
    QString masterGroup;
    /// True on the expanded home layout of the group (raiseMaster lands here).
    bool isHome = false;
    /// From home: layout id used when master window is closed while secondaries exist.
    QString collapseLayoutId;
    /// From non-home shell: layout id used when expanding/recalling home.
    QString expandLayoutId;
    /// Dock chip stays hidden until a bottom-edge gaze reveal dwell completes.
    bool hideUntilGazeReveal = false;

    [[nodiscard]] bool isMasterShell() const { return role == LayoutRole::MasterShell; }
    [[nodiscard]] bool isGazeRevealDock() const
    {
        return isMasterShell() && hideUntilGazeReveal;
    }
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
    LayoutSessionMeta session;
    LayoutGrid grid;
    LayoutDwellConfig dwell;
    LayoutWindowPlacement placement;
    LayoutUiStyle uiStyle = LayoutUiStyle::Default;
    QVector<LayoutItem> items;
    /// Run when an instance is first created/opened (not on in-place load).
    QVector<LayoutAction> onOpen;
    /// Run when a document is applied into an instance (open and loadInto).
    QVector<LayoutAction> onLoad;
    /// Run before an instance is closed (instance id still valid).
    QVector<LayoutAction> onClose;

    [[nodiscard]] bool isValid() const
    {
        return !id.isEmpty() && grid.columns > 0 && grid.rows > 0;
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

    [[nodiscard]] bool isMasterShell() const { return session.isMasterShell(); }

    [[nodiscard]] const LayoutItem* findItem(const QString& itemId) const
    {
        for (const LayoutItem& it : items) {
            if (it.id == itemId) {
                return &it;
            }
        }
        return nullptr;
    }
};

} // namespace gazer
