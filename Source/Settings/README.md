# Data-driven settings

The settings UI and persistence format are generated from JSON schemas.
Adding a value does not require another hand-written editor control.

## Scopes

- `ProjectSettings.schema.json` defines settings shared by a project.
  Values are stored in `<Project>/Settings/ProjectSettings.json`.
- `EditorSettings.schema.json` defines per-user editor preferences.
  Values are stored in the platform preference directory returned by SDL.

A project may override the bundled project schema by providing
`Settings/Schemas/ProjectSettings.schema.json`. If it does not, the fallback
project schema is used.

## Supported types

- `boolean`
- `integer`
- `number`
- `string`
- `enum`

Numeric definitions may contain `Min`, `Max`, and `Step`. Enum definitions
contain an `Options` array. Every definition requires an `Id`, `Label`, `Type`,
and correctly typed `Default`.

Runtime systems read values by stable `Id` through `SettingsService`.
Unknown values are ignored and missing values use schema defaults.
