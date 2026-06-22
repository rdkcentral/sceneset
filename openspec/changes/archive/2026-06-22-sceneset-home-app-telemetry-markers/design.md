## Context

SceneSet launches the home app after preinstall completion and handles lifecycle-driven relaunch behavior, but it does not currently emit structured telemetry for launch latency or relaunch progression. The platform already uses T2 telemetry plumbing in adjacent components (preinstall/package manager), and SceneSet must publish via the same mechanism with compile-time gating that matches existing practice. A key constraint is to keep compile-time guards localized at the final telemetry publish layer, not scattered through startup and lifecycle logic.

## High Level Architecture

Startup and lifecycle code records timestamps and state, while a single telemetry publish layer emits the marker:

1. `initialize()` resets telemetry state and records SceneSet start timestamp.
2. `startPreinstall()` records preinstall start timestamp.
3. `completeStartupAfterPreinstall()` records preinstall end timestamp.
4. `launchDefaultApp()` records launch-request timestamp and marks `ACTIVE` telemetry as pending.
5. `OnAppLifecycleStateChanged()`:
- on `UNLOADED`, updates termination nature and cumulative relaunch count when a restart path is selected.
- on `ACTIVE`, publishes the launch marker with duration fields and relaunch context.
6. `publishHomeAppActiveTelemetry()` builds payload fields and delegates to `publishTelemetryMarker()`.
7. `publishTelemetryMarker()` is the only location with telemetry compile-time guard and T2 call.

## Goals / Non-Goals

**Goals:**
- Emit a launch marker when home app reaches `ACTIVE` with three timing fields:
- Total duration from SceneSet startup to home app `ACTIVE`.
- Preinstall duration.
- Home launch-exclusive duration (`launch request` to `ACTIVE`).
- Emit relaunch marker context with cumulative retry count and termination nature (`crash` vs `intentional_kill`) when app returns to `ACTIVE` after termination.
- Reuse existing T2 telemetry mechanism and naming conventions used by related app-management components.
- Keep `#ifdef`/compile-time telemetry gating only in a last-level telemetry publisher helper.

**Non-Goals:**
- Introducing new launch timeout/retry policies for initial launch failure.
- Changing current restart eligibility rules (e.g., `RESTART_HOMEAPP_ALWAYS` behavior).
- Building a new metrics transport or replacing the existing T2 telemetry path.

## Decisions

1. Introduce a SceneSet-local telemetry publisher abstraction
- Decision: Add a small helper (for example, `SceneSetTelemetry`) that exposes typed publish methods for launch and relaunch markers.
- Rationale: Centralizes marker names, payload formatting, and compile-time gating.
- Alternative considered: Inline telemetry publish calls in SceneSet lifecycle handlers.
- Why not: Would spread conditional compilation and formatting logic across unrelated business paths.

2. Capture timing anchors from existing lifecycle points
- Decision: Record explicit timestamps for SceneSet process start, preinstall start/end, and home launch request time.
- Rationale: Computes required metrics deterministically without changing control flow.
- Alternative considered: Derive preinstall/launch durations indirectly from logs.
- Why not: Not reliable and not testable.

3. Track cumulative relaunch count and last termination nature in lifecycle state machine
- Decision: Maintain in-memory counters/state updated on termination and relaunch transitions, then publish on successful `ACTIVE`.
- Rationale: Matches acceptance criteria for cumulative retry count and crash-vs-kill context.
- Alternative considered: Emit only per-event counters.
- Why not: Does not satisfy cumulative retry visibility requirement.

4. Publish telemetry on `ACTIVE` transition
- Decision: Launch metric publication is triggered when reference app reaches `ACTIVE`; relaunch telemetry is emitted on subsequent successful `ACTIVE` transitions after one or more terminations.
- Rationale: `ACTIVE` is the success milestone required by acceptance criteria.
- Alternative considered: Publish on `RUNNING`.
- Why not: `RUNNING` can precede full readiness and does not satisfy requirement wording.

## Risks / Trade-offs

- [Risk] Lifecycle event ordering differences could misclassify termination nature. -> Mitigation: Base classification on explicit transition context (`TERMINATING -> UNLOADED`) and known intentional kill paths.
- [Risk] Missing timestamps if startup path short-circuits (e.g., empty reference app). -> Mitigation: Guard publication with required-field validation and skip telemetry when flow is not applicable.
- [Risk] Metric cardinality drift if payload keys are renamed ad hoc. -> Mitigation: Define constants for marker and field names in one telemetry helper header.
- [Trade-off] In-memory cumulative retry count resets on service restart. -> Mitigation: Document scope as process-lifetime cumulative until persistence is explicitly required.

## Migration Plan

1. Add telemetry helper and compile-time-gated final publish implementation.
2. Wire timestamp and relaunch-state capture into existing SceneSet startup/lifecycle methods.
3. Validate marker emission in local/system logs and telemetry pipeline with app active, kill, and crash scenarios.
4. Rollback strategy: disable telemetry by build flag or revert helper wiring without changing launch/restart behavior.

## Open Questions

- Confirm exact canonical marker names and field keys expected by downstream T2 dashboards.
- Confirm whether termination nature should be encoded as string values (`crash`, `kill`) or numeric enums.
- Confirm whether relaunch-count marker should emit on every `ACTIVE` after first termination or only when count increments.