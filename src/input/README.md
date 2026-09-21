# input

OS / virtual device injection used by mapping profiles.

| Module | Status |
|--------|--------|
| `KeyboardInjector` | Windows `SendInput` (key down/up, unicode text). Combos go through `KeyStateManager`. Elevated windows need UIAccess (installed/signed MSI). |
| `KeyNames` | Modifier alias table (`shift`/`ctrl`/`alt`/`win`) |
| `KeyGlyphs` | US QWERTY label overlay and OEM stroke mapping |
| `KeyStateManager` | Modifier cycle Up → Down → LockedDown; Down auto-releases after a standard key |
| `MouseInjector` | Windows click / relative move / scroll (button/wheel: board HTTRANSPARENT for the gesture) |
| `PixelScroller` | Pixel scroll for look-to-scroll (classify HWND, leftover wheel; no board punch) |
| `VirtualGamepad` | Xbox 360 pad via ViGEmClient.dll + ViGEmBus (`LoadLibrary`, same pattern as Tobii). Mapping `gamepadButton` / `gamepadAxis` and head-pose joy dests. Fails closed if the DLL or bus is missing. |
| `InputService` | Keyboard / mouse / gamepad inject (`KeyStateManager`, `MouseInjector`, `VirtualGamepad`). Pixel scroll is LTS-only. |
