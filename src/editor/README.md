# editor

Qt Widgets designer for Page XML (same files the runtime loads).

| File | Role |
|------|------|
| `LayoutEditorWindow` | Three-pane shell, menus, F5/F6; file dialogs in `LayoutEditorWindowFile.cpp` |
| `LayoutEditorStyle` | Themed QSS for the designer chrome |
| `LayoutEditorCodeView` | XML source pane; highlighter in `LayoutEditorXmlHighlight` |
| `LayoutEditorSession*` | Document + undo; `Io` load/save; `Edit` mutations |
| `LayoutEditorCanvas` | Virtual-display canvas; `Paint` / `Interact` split the widget. Toolbar layer combo: Shown (`showLayers`) or one layer. |
| `LayoutEditorMenu` | Shared item context menu (canvas + tree) |
| `LayoutEditorToolbox` | Add palette, element tree, context menu |
| `LayoutEditorIcons` | Themed painter glyphs for toolbar / palette / tree |
| `LayoutEditorProperties` | Inspector tabs; `PropertiesFill.cpp` builds forms |
| `LayoutEditorFields` | Form widgets; `FieldsGroups.cpp` chrome/dwell/action |
| `LayoutEditorKeyboard` | Blank / keyboard / row / chip templates |

Shipped save writes `%AppData%\Gazer\layouts` and leaves `resources/` unchanged.
