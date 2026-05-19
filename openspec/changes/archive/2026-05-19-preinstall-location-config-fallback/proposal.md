## Why

SceneSet needs a stable, runtime-configurable way to locate preinstall bundles without breaking existing RDK-M behavior. Adding support for `/etc/sceneset.conf` `preinstallLocation` allows platform integrations to override the preinstall path while preserving the current fallback flow when the setting is absent.

## What Changes

- Add support for reading `preinstallLocation` from `/etc/sceneset.conf` and using it as the highest-priority preinstall directory source when present and non-empty.
- Keep current preinstall directory resolution behavior when `/etc/sceneset.conf` is missing, unreadable, or does not define `preinstallLocation`.
- Validate that existing preinstall flows (copy, preinstall start, completion handling, and cleanup behavior) continue to work with and without the config override.

## Capabilities

### New Capabilities
- None.

### Modified Capabilities
- `configuration`: Extend runtime configuration sources to include `/etc/sceneset.conf` `preinstallLocation` with higher priority than dynamic plugin config and compile-time fallback for preinstall directory selection.

## Impact

- Affected code paths in SceneSet startup and preinstall directory resolution logic.
- Runtime behavior depends on presence and content of `/etc/sceneset.conf`.
- Existing integrations remain compatible because fallback behavior is preserved when the setting is absent.
- No external API contract changes; behavior change is configuration-driven.
