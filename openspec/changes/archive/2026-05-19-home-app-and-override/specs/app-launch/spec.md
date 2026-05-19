## ADDED Requirements

### Requirement: Initial launch targets configured defaultHomeApp
During initial startup flow for this EntOS behavior, SceneSet SHALL attempt to load and launch only the configured `defaultHomeApp`.

#### Scenario: defaultHomeApp is resolved
- **WHEN** configuration resolution produces a valid `defaultHomeApp`
- **THEN** SceneSet initiates launch only for that app identifier during initial startup

#### Scenario: Other apps are not launched by initial flow
- **WHEN** initial startup launch flow executes with a valid `defaultHomeApp`
- **THEN** SceneSet does not initiate additional app launches from the initial flow

### Requirement: Missing /etc config does not break launch compatibility
If `/etc/sceneset.conf` is missing, SceneSet SHALL preserve existing startup behavior and SHALL NOT fail solely because the file is absent.

#### Scenario: /etc config is absent
- **WHEN** SceneSet starts and `/etc/sceneset.conf` is not present
- **THEN** SceneSet continues startup using legacy/default behavior instead of treating the condition as fatal
