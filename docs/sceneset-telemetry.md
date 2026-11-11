# SceneSet Telemetry Guide

## 1. Overview

When built with `SCENESET_TELEMETRY_METRICS_SUPPORT=ON`, SceneSet emits T2 telemetry for home app launch and relaunch behavior.

- **T2 Component Name:** `sceneset`
- **Marker Name:** `ENTS_INFO_Sceneset_LaunchTime`
- **Publish Trigger:** Reference app reaches `ACTIVE`

This telemetry is intended to provide startup latency visibility and relaunch diagnostics without changing launch/restart behavior.

---

## 2. Marker Contract

### Always-Present Fields

| Field | Type | Meaning |
|-------|------|---------|
| `appId` | String | Configured reference app ID |
| `launchToActiveMs` | Integer | Duration from `LaunchApp` request to `ACTIVE` |

### Initial Launch Fields (First Successful `ACTIVE` in Process Lifetime)

| Field | Type | Meaning |
|-------|------|---------|
| `totalStartToActiveMs` | Integer | Duration from SceneSet startup to `ACTIVE` |
| `preinstallDurationMs` | Integer | Duration from preinstall start to preinstall completion path |

### Relaunch-Only Context Fields

| Field | Type | Meaning |
|-------|------|---------|
| `cumulativeRelaunchCount` | Integer | Relaunch count accumulated in current SceneSet process lifetime |
| `terminationNature` | String | One of `none`, `crash`, `intentional_kill` |

---

## 3. Emission Behavior

### Initial Launch

- Marker is emitted when the home app first reaches `ACTIVE`.
- Payload includes timing fields for startup and preinstall phases.

### Relaunch After Termination

- Marker is emitted on subsequent successful `ACTIVE` transitions.
- Payload includes relaunch context fields.

### No Early Emission

- SceneSet does not emit the launch marker before the app reaches `ACTIVE`.

---

## 4. Build-Time Gating

- Telemetry is controlled by `SCENESET_TELEMETRY_METRICS_SUPPORT`.
- Compile-time telemetry guards are isolated to the final publish implementation layer.
- Startup/lifecycle business logic remains telemetry-guard free.

---

## 5. Notes and Constraints

- `cumulativeRelaunchCount` resets when SceneSet restarts.
- Contract applies to the single configured reference app managed by SceneSet.
- For normative behavior definitions, see OpenSpec:
  - `openspec/specs/sceneset-telemetry/spec.md`
  - `openspec/specs/app-launch/spec.md`