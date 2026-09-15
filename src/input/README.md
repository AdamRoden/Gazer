# input

OS / virtual device injection used by mapping profiles.

| Module | Status |
|--------|--------|
| `KeyboardInjector` | Windows `SendInput` (key down/up, unicode text). Combos go through `KeyStateManager`. Elevated windows need UIAccess (installed/signed MSI). |
| `KeyNames` | Modifier alias table (`shift`/`ctrl`/`alt`/`win`) |
| `KeyGlyphs` | US QWERTY label overlay and OEM stroke mapping |
| `KeyStateManager` | Modifier cycle Up → Down → LockedDown; Down auto-releases after a standard key |
| `MouseInjector` | Windows click / relative move / scroll (button/wheel: board HTTRANSPARENT for the gesture) |
| `PixelScroller` | Pixel scroll for LTS (classify HWND, leftover wheel; no board punch) |
| `VirtualGamepad` | Stub (fails until ViGEm) |
| `InputService` | Keyboard / mouse / gamepad inject (`KeyStateManager`, `MouseInjector`, `VirtualGamepad`). Pixel scroll is LTS-only. |
