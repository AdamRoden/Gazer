# mapping

JSON **profiles** map layout `command` names (and optional speak typing) to
`InputService` outputs. Builtins win; see `src/app/Commands.md`.

- Profile: `resources/mappings/default.json`
- Loader: `MappingLoader`
- Runtime: `MappingEngine::runCommand` / `handleSpeak`
