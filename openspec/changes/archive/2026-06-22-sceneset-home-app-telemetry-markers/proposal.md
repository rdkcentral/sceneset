## Why

SceneSet currently launches and restarts the home app without emitting reliable telemetry for launch latency, relaunch count, or termination context. This blocks platform diagnostics and performance tracking for T2 analytics, especially when launch quality degrades or repeated restarts occur.

## What Changes

- Add telemetry marker emission in SceneSet using the same underlying T2 telemetry mechanism already used by preinstall/package manager components.
- Measure and emit launch timing fields when the home app reaches `ACTIVE`, including:
- Total duration from SceneSet startup to home app `ACTIVE`.
- Preinstall phase duration.
- Exclusive home app launch-to-`ACTIVE` duration.
- Emit cumulative relaunch count whenever the home app eventually reaches `ACTIVE` after prior termination/restart cycles.
- Include termination nature classification in telemetry context (`crash` vs intentional `kill`) for relaunch-related publication.
- Keep telemetry compile-time gating aligned with existing telemetry patterns, with conditional compilation isolated at the final telemetry call layer.

## Capabilities

### New Capabilities
- `sceneset-telemetry`: Telemetry marker contract for SceneSet home-app launch timing, cumulative relaunch count, and termination-nature reporting.

### Modified Capabilities
- (none)

## Impact

- Affected code: SceneSet startup/lifecycle flow (`SceneSet.cpp`), plus any shared telemetry utility layer introduced for marker publishing.
- Build/config impact: Reuse existing telemetry compile-time guards and linkage conventions; avoid spreading `#ifdef` checks across business logic.
- Operational impact: New analytics data points for startup latency and relaunch behavior, including crash vs kill context.