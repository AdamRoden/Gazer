# input

OS / virtual device injection used by mapping profiles.

| Module | Status |
|--------|--------|
| `KeyboardInjector` | Windows `SendInput` (keys, combos, unicode text). Elevated windows need UIAccess (installed/signed MSI). |
| `KeyNames` | Modifier alias table (`shift`/`ctrl`/`alt`/`win`) |
| `KeyGlyphs` | US QWERTY label overlay and OEM stroke mapping |
| `KeyStateManager` | Modifier cycle Up → Down → LockedDown; Down auto-releases after a standard key |
| `MouseInjector` | Windows click / relative move / scroll (button/wheel: board HTTRANSPARENT for the gesture) |
| `PixelScroller` | Pixel scroll for LTS (native bars + UIA, wheel fallback) |
| `VirtualGamepad` | Stub (logs); ViGEm later |
| `InputService` | Facade over the above |
