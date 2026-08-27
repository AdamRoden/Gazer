# editor

Qt Widgets designer for Page XML (same files the runtime loads).

| File | Role |
|------|------|
| `LayoutEditorWindow` | Three-pane shell, menus, F5/F6 |
| `LayoutEditorSession*` | Document + undo; `Io` load/save; `Edit` mutations |
| `LayoutEditorCanvas` | Fit-grid / virtual-display canvas |
| `LayoutEditorToolbox` | Add tree, context menu |
| `LayoutEditorProperties` / `LayoutEditorFields` | Right-pane forms |
| `LayoutEditorKeyboard` | Layer combo / shift labels |

Shipped save writes `%AppData%\Gazer\layouts` and leaves `resources/` unchanged.
