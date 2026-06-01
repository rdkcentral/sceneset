## Why

For EntOS, SceneSet needs a deterministic and externally configurable source for selecting the home application at startup. Defining `/etc/sceneset.conf` as the base configuration, with a debug-only optional override layer, enables production-safe defaults while allowing local debug scenarios to override specific keys without replacing the full config, while preserving existing behavior if the config file is absent.

## What Changes

- Add support for reading SceneSet configuration from `/etc/sceneset.conf`.
- Scope `/etc/sceneset.conf` behavior to EntOS integration and keep existing non-EntOS behavior unchanged.
- Introduce `defaultHomeApp` configuration, where EntOS should set this to the EPG appId.
- Update initial SceneSet startup behavior to only load and launch the configured default home app.
- If `ENABLE_CONFIG_OVERRIDE` is enabled and an optional `/opt` config file exists, apply it as an override layer on top of `/etc/sceneset.conf`.
- Ensure override behavior is key-based merge semantics: `/etc` config provides the base, and only keys present in `/opt` replace corresponding base values; all other `/etc` values remain unchanged.
- Accept all valid config values in override files using the same parsing/validation rules as `/etc/sceneset.conf`.
- If `/etc/sceneset.conf` is not present, fall back to current default behavior so existing functionality is not broken.

## Capabilities

### New Capabilities
- `home-app-and-override`: Support base plus debug override configuration resolution for default home app startup.

### Modified Capabilities
- `configuration`: Define required `/etc/sceneset.conf` loading and debug-build `/opt` override merge behavior.
- `app-launch`: Constrain initial launch flow to start only the configured `defaultHomeApp`.

## Impact

- Affected code: SceneSet startup/config path handling (notably SceneSet configuration loading and app launch selection flow).
- Affected behavior: On EntOS, `defaultHomeApp` should be set to EPG appId in `/etc/sceneset.conf`; in debug builds, `/opt` overrides can selectively replace base keys.
- Compatibility expectation: Missing `/etc/sceneset.conf` must not be treated as a fatal condition and should preserve current startup behavior.
- Affected systems: Device startup experience and integration with packaged home app deployment defaults.
- Risk area: Configuration precedence and merge logic correctness; needs clear validation and test coverage for base-only, override-only (debug), and mixed-key scenarios.