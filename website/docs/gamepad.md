# Gamepad

`example_gamepad` is the dwell Xbox pad (drawer **Gamepad**). It injects a virtual Xbox 360 controller through ViGEm.

You need the [ViGEmBus](https://github.com/nefarius/ViGEmBus) kernel driver. Settings → **Assist** can download the official setup and launch it (UAC). Gazer ships the user-mode `ViGEmClient.dll` next to `Gazer.exe` (the bus installer does not). Without the driver, button cells fail closed and toast.

| Cluster | What it sends |
|---------|----------------|
| A / B / X / Y, LB / RB, View / Menu / Xbox, L3 / R3, D-pad | Button tap (`gamepadButton` press then release) |
| LT / RT | Analog trigger to full. Stays down until **Release analog** |
| Left / right stick hats | Stick to that direction (cardinals full; diagonals 0.707). Stays until another hat cell or **Release analog** |
| **LS** / **RS** hubs | [Look-to left / right stick](assist.md): dwell to place an origin, then gaze drives that analog stick |
| **Release analog** | Quit both look-to sticks and zero lx/ly/rx/ry/lt/rt |

The board stays up while you look at a game (it does not idle-close). Close it from the top-right cell.
