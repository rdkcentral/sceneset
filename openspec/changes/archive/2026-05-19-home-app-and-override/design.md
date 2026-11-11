## Context

SceneSet currently resolves startup behavior without a mandatory external configuration source for selecting the initial home app. This change introduces EntOS-specific configuration through `/etc/sceneset.conf` and requires a non-breaking fallback when that file is absent. In debug builds only, an optional `/opt` configuration file must override individual keys from `/etc` using the same accepted value format.

Key constraints:
- EntOS-focused behavior must not regress non-EntOS or legacy startup flows.
- Missing `/etc/sceneset.conf` is non-fatal and should preserve existing behavior.
- Override behavior is gated by `ENABLE_CONFIG_OVERRIDE` and optional file presence.

## Goals / Non-Goals

**Goals:**
- Add deterministic base config loading from `/etc/sceneset.conf` for EntOS startup behavior.
- Support `defaultHomeApp` configuration to drive initial app launch.
- Implement deterministic precedence: base `/etc` values, then debug-only `/opt` overrides by key.
- Reuse existing config parsing semantics so override files accept the same value set as base config.
- Preserve current behavior when `/etc/sceneset.conf` is missing.

**Non-Goals:**
- Redesign SceneSet app lifecycle or launch manager architecture.
- Introduce runtime hot-reload for configuration files.
- Expand override support to production builds.
- Define packaging policy for who writes these files beyond EntOS integration expectations.

## Decisions

1. Configuration source order and gating
- Decision: Resolve configuration in this order: existing defaults/legacy behavior as baseline, apply `/etc/sceneset.conf` if present (system config path), then apply `/opt` overrides only when `ENABLE_CONFIG_OVERRIDE` is enabled and the file exists.
- Rationale: This provides predictable precedence while preserving backward compatibility and keeping debug behavior isolated.
- Alternatives considered:
  - Require `/etc/sceneset.conf` unconditionally: rejected due to compatibility risk.
  - Allow `/opt` override in all builds: rejected due to production safety and policy concerns.

2. Merge semantics
- Decision: Use key-based overlay merge. Keys in `/opt` replace corresponding keys from `/etc`; keys not present in `/opt` retain `/etc` values.
- Rationale: Matches user expectation for partial overrides (for example, override only one key while preserving others).
- Alternatives considered:
  - Full replacement of `/etc` with `/opt`: rejected because it discards unrelated base settings.
  - Deep custom merge with special cases: rejected as unnecessary complexity for initial scope.

3. Parsing and validation compatibility
- Decision: Parse `/opt` override content using the same parser and accepted value rules as `/etc/sceneset.conf`.
- Rationale: Ensures consistency, avoids dual behavior, and reduces implementation risk.
- Alternatives considered:
  - Separate debug parser: rejected due to drift risk and additional maintenance.

4. Startup launch behavior
- Decision: For the initial implementation, launch only `defaultHomeApp` when present from resolved config.
- Rationale: Aligns with requested initial product behavior and simplifies rollout.
- Alternatives considered:
  - Keep multi-app startup behavior in parallel: rejected for initial phase to avoid ambiguity.

5. EntOS scope boundary
- Decision: Treat `/etc/sceneset.conf` handling as EntOS integration behavior and do not make missing file fatal.
- Rationale: Prevents regressions in environments where the file is not provisioned.
- Alternatives considered:
  - Hard-fail on missing EntOS file: rejected because it breaks existing functional startup paths.

## Risks / Trade-offs

- [Risk] Incorrect precedence implementation can launch the wrong home app. -> Mitigation: Add targeted tests for base-only, debug-only, and merged-key precedence cases.
- [Risk] EntOS scoping may be applied too broadly and affect other deployments. -> Mitigation: Gate behavior by platform/integration checks and preserve legacy fallback.
- [Risk] Invalid config values can create startup ambiguity. -> Mitigation: Reuse existing validation paths and log parse failures with explicit fallback to prior behavior.
- [Trade-off] Restricting override to debug builds reduces flexibility in production troubleshooting. -> Mitigation: Keep this as an intentional safety boundary; revisit only with explicit requirements.

## Migration Plan

1. Introduce config resolution flow with explicit precedence and build gating.
2. Enable EntOS deployment to provision `/etc/sceneset.conf` with `defaultHomeApp` set to EPG appId.
3. Validate behavior across three scenarios:
   - `/etc` present, no `/opt`
   - `/etc` plus debug `/opt` with partial key overrides
   - `/etc` missing (fallback behavior)
4. Rollback strategy: disable new config path or remove file provisioning to return to legacy startup behavior.

## Open Questions

- Should EntOS scoping be compile-time, runtime platform detection, or both?
- What is the exact `/opt` override file path and naming convention to standardize in implementation and tests?
- If `defaultHomeApp` is invalid or unavailable, should SceneSet skip launch, fallback to legacy default, or retry policy?