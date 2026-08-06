# input

OS / virtual device injection used by mapping profiles.

| Module | Status |
|--------|--------|
| `KeyboardInjector` | Windows `SendInput` (keys, combos, unicode text) |
| `MouseInjector` | Windows click / relative move / scroll |
| `VirtualGamepad` | Stub (logs); ViGEm later |
| `InputService` | Facade over the above |
