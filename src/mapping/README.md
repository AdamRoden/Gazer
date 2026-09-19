# mapping

JSON **profiles** map layout `command` names to
`InputService` outputs. Builtins win; see `src/app/Commands.md`.
`gamepadButton` / `gamepadAxis` need ViGEm (`VirtualGamepad`).

- Profile: `resources/mappings/default.json`
- Loader: `MappingLoader`
- Runtime: `MappingEngine::runCommand`

Head-pose analog maps (yaw/pitch/roll/x/y/z → mouse velocity, scroll, gaze offset, joystick, command trigger) live in `AppSettings` and `HeadPoseCurve` / `HeadPoseMapper`, not in the JSON profile.
