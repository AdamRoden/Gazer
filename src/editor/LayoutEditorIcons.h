#pragma once

#include "editor/LayoutEditorSession.h"
#include "ui/Theme.h"

#include <QIcon>

namespace gazer {

enum class EditorGlyph {
    FileNew,
    FileOpen,
    FileSave,
    Undo,
    Redo,
    Cut,
    Copy,
    Paste,
    Delete,
    TestLive,
    TestCanvas,
    Fit,
    Grid,
    Button,
    Label,
    Toggle,
    Tab,
    Slider,
    Zone,
    GridAdd,
    SubGrid,
    Style,
    Dwell,
    Duplicate,
    Page,
    ZoomIn,
    ZoomOut,
    FitScreen,
    Code
};

[[nodiscard]] QIcon editorGlyphIcon(EditorGlyph glyph, const ThemeColors& theme, int logicalPx = 16);
[[nodiscard]] EditorGlyph glyphForItemKind(EditorItemKind kind);
[[nodiscard]] EditorGlyph glyphForLeaf(const PageLeaf& leaf, bool zone);

} // namespace gazer
