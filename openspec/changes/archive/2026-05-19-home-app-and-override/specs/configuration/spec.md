## ADDED Requirements

### Requirement: EntOS defaultHomeApp source in /etc/sceneset.conf
For EntOS integration, SceneSet SHALL read `defaultHomeApp` from `/etc/sceneset.conf` using the same configuration parsing and validation rules used for other accepted config values.

#### Scenario: defaultHomeApp is provided in /etc
- **WHEN** `/etc/sceneset.conf` contains a valid `defaultHomeApp` value
- **THEN** SceneSet resolves that value as the configured home app identifier

#### Scenario: defaultHomeApp is absent or invalid
- **WHEN** `/etc/sceneset.conf` is missing `defaultHomeApp` or the value is invalid
- **THEN** SceneSet preserves existing behavior and does not fail startup solely due to this condition

### Requirement: Debug-only override precedence
SceneSet SHALL apply optional `/opt` configuration as a key-level override over `/etc/sceneset.conf` only when compiled with `ENABLE_CONFIG_OVERRIDE`.

#### Scenario: Debug build with override file present
- **WHEN** `ENABLE_CONFIG_OVERRIDE` is enabled and the `/opt` override file exists with key `b` set
- **THEN** the resolved configuration value for key `b` is the `/opt` value and non-overridden keys continue using `/etc` values

#### Scenario: Non-debug build ignores /opt override
- **WHEN** `ENABLE_CONFIG_OVERRIDE` is not enabled and `/opt` override file exists
- **THEN** SceneSet ignores the `/opt` override file and uses base resolution behavior
