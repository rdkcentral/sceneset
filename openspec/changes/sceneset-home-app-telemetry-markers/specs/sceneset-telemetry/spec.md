## ADDED Requirements

### Requirement: Home App Active Launch Marker
SceneSet SHALL publish a telemetry marker through the existing T2 telemetry mechanism when the configured home app transitions to `ACTIVE` for the launch sequence.

#### Scenario: Emit marker on initial successful active
- **WHEN** SceneSet starts and the home app reaches `ACTIVE`
- **THEN** SceneSet publishes one launch marker for that successful launch sequence

#### Scenario: No marker before active
- **WHEN** the home app has not yet reached `ACTIVE`
- **THEN** SceneSet MUST NOT publish the launch-success marker

### Requirement: Launch Marker Timing Fields
The launch-success marker SHALL include all of the following timing fields for the same launch sequence: total time from SceneSet start to home app `ACTIVE`, preinstall phase duration, and exclusive home-launch-to-`ACTIVE` duration.

#### Scenario: Compute total start-to-active duration
- **WHEN** SceneSet publishes the launch-success marker
- **THEN** the marker includes `total_start_to_active_ms` measured from SceneSet startup timestamp to home app `ACTIVE` timestamp

#### Scenario: Compute preinstall duration
- **WHEN** preinstall is executed in the startup flow
- **THEN** the marker includes `preinstall_duration_ms` measured from preinstall start to preinstall completion

#### Scenario: Compute launch-exclusive duration
- **WHEN** SceneSet triggers home app launch and the app later reaches `ACTIVE`
- **THEN** the marker includes `launch_to_active_ms` measured from launch request timestamp to `ACTIVE` timestamp

### Requirement: Cumulative Relaunch and Termination Context
SceneSet SHALL publish telemetry for successful post-termination recovery that includes cumulative relaunch count within the current SceneSet process lifetime and the last termination nature classification (`crash` or `intentional_kill`).

#### Scenario: Publish cumulative relaunch count on successful recovery
- **WHEN** home app has been terminated at least once and later reaches `ACTIVE`
- **THEN** SceneSet publishes telemetry containing cumulative relaunch count since SceneSet process start

#### Scenario: Publish crash classification
- **WHEN** the most recent termination is classified as crash-driven recovery
- **THEN** telemetry includes termination nature value `crash`

#### Scenario: Publish intentional-kill classification
- **WHEN** the most recent termination is an intentional kill path
- **THEN** telemetry includes termination nature value `intentional_kill`

### Requirement: Compile-Time Telemetry Gating Isolation
SceneSet SHALL keep compile-time telemetry feature guards localized to the final telemetry publish implementation layer rather than distributing `#ifdef` branches across startup and lifecycle decision logic.

#### Scenario: Business logic remains guard-free
- **WHEN** SceneSet launch and lifecycle handlers are implemented
- **THEN** handler control flow is independent of telemetry compile-time branches and calls a single publish abstraction

#### Scenario: Telemetry disabled build
- **WHEN** telemetry compile-time option is disabled
- **THEN** the final telemetry publish layer becomes a no-op without changing launch/restart functional behavior
