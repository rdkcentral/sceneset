## ADDED Requirements

### Requirement: EntOS home app configuration resolution
SceneSet SHALL resolve home app configuration for EntOS by loading `/etc/sceneset.conf` as the base source and applying a debug-only optional override file on top when enabled.

#### Scenario: Base configuration is loaded from /etc
- **WHEN** SceneSet starts on EntOS and `/etc/sceneset.conf` is present with valid keys
- **THEN** SceneSet uses values from `/etc/sceneset.conf` as the base runtime configuration

#### Scenario: Debug override overlays matching keys
- **WHEN** `ENABLE_CONFIG_OVERRIDE` is enabled and the optional `/opt` override file is present with a subset of keys
- **THEN** SceneSet replaces only matching keys from the base `/etc` configuration and preserves all other base keys

#### Scenario: Missing /etc config preserves compatibility
- **WHEN** `/etc/sceneset.conf` is absent
- **THEN** SceneSet falls back to existing startup behavior without treating the missing file as a fatal error
