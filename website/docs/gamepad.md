# Gamepad

`example_gamepad` is the dwell Xbox pad (drawer **Gamepad**). It injects a virtual Xbox 360 controller through ViGEm.

You need the [ViGEmBus](https://github.com/nefarius/ViGEmBus) kernel driver. Settings → **Assist** can download the official setup and launch it (UAC). Gazer ships the user-mode `ViGEmClient.dll` next to `Gazer.exe` (the bus installer does not). Without the driver, button cells fail closed and toast.

Cells are `command="gamepad.…"` names in `resources/mappings/default.json`. Those names are **not** builtins: `CommandRegistry` falls through to mapping inject (`gamepadButton` press-then-release, or `gamepadAxis` that **stays**). Catalog: [Commands](reference/commands.md#mapping-only-names-in-defaultjson).

| Cluster | Command | What it sends |
|---------|---------|----------------|
| A / B / X / Y, LB / RB, View / Menu / Xbox, L3 / R3, D-pad | `gamepad.a` … `.guide`, `gamepad.dpad.*` | Button tap (`gamepadButton` press then release) |
| LT / RT | `gamepad.lt` / `.rt` | Analog trigger to 1.0. Stays until **Release analog** |
| Left / right stick hats | `gamepad.ls.n` … `.sw`, `gamepad.rs.*` | Stick to that direction (cardinals ±1; diagonals ±0.707). Stays until another hat cell or **Release analog** |
| **LS** / **RS** hubs | `lookToLeftStick` / `lookToRightStick` | [Look-to left / right stick](assist.md): dwell to place an origin, then gaze drives that analog stick |
| **Release analog** | `gamepad.analog.center` plus look-to quit | Quit both look-to sticks and zero lx/ly/rx/ry/lt/rt |

The board stays up while you look at a game (it does not idle-close). Close it from the top-right cell.
