## MODIFIED Requirements

### Requirement: Preinstall Directory Resolution
SceneSet SHALL resolve the preinstall directory using this precedence order at startup:
1. `preinstallLocation` from `/etc/sceneset.conf` when the file exists and the key has a non-empty value.
2. `appPreinstallDirectory` from `org.rdk.PreinstallManager` dynamic plugin config.
3. Compile-time `APP_PREINSTALL_DIRECTORY`.

If all sources are unavailable or empty after resolution, SceneSet SHALL fail initialization.

#### Scenario: Config file provides preinstall location
- **WHEN** `/etc/sceneset.conf` exists and contains a non-empty `preinstallLocation`
- **THEN** SceneSet SHALL use that value as the preinstall directory and SHALL NOT override it with dynamic or compile-time values

#### Scenario: Config file is absent
- **WHEN** `/etc/sceneset.conf` does not exist
- **THEN** SceneSet SHALL continue using existing fallback behavior (dynamic plugin config, then compile-time fallback)

#### Scenario: Config file does not provide key
- **WHEN** `/etc/sceneset.conf` exists but `preinstallLocation` is missing or empty
- **THEN** SceneSet SHALL continue using existing fallback behavior (dynamic plugin config, then compile-time fallback)

#### Scenario: Config file provides unusable location
- **WHEN** `/etc/sceneset.conf` contains `preinstallLocation` but the value is invalid for runtime use
- **THEN** SceneSet SHALL ignore that value and continue using existing fallback behavior (dynamic plugin config, then compile-time fallback)
