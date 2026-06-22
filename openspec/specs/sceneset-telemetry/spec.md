# Spec: SceneSet Telemetry

## Overview

SceneSet emits a telemetry marker when the configured home app reaches `ACTIVE`. The marker captures launch timing and relaunch context so operators can track startup performance and restart behavior.

---

## Behaviors

### 1. Marker Identity

- SceneSet uses T2 component name `sceneset`.
- SceneSet publishes marker `SCENESET_HOME_APP_ACTIVE` for successful home-app `ACTIVE` transitions.
- SceneSet does not publish this marker before the app reaches `ACTIVE`.

---

### 2. Timing Fields

For each published marker, SceneSet includes:

- `totalStartToActiveMs`: time from SceneSet startup to home app `ACTIVE`.
- `preinstallDurationMs`: time from preinstall start to preinstall completion path.
- `launchToActiveMs`: time from home app launch request to home app `ACTIVE`.

---

### 3. Relaunch Context Fields

For each published marker, SceneSet includes:

- `appId`: configured reference app ID.
- `cumulativeRelaunchCount`: relaunch count accumulated during the current SceneSet process lifetime.
- `terminationNature`: one of `none` (no prior termination context), `crash`, `intentional_kill`.

---

### 4. Compile-Time Gating

- Telemetry publication is controlled by `SCENESET_TELEMETRY_METRICS_SUPPORT`.
- Compile-time telemetry guards are isolated to the final telemetry publish layer.
- Launch/restart business logic remains unchanged when telemetry is disabled.

---

## Constraints

- `cumulativeRelaunchCount` is process-lifetime only and resets when SceneSet restarts.
- The telemetry contract applies only to the single configured reference app managed by SceneSet.

