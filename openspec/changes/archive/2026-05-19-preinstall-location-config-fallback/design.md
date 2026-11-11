## Context

SceneSet currently resolves the preinstall directory from PreinstallManager plugin config (`appPreinstallDirectory`) and then compile-time `APP_PREINSTALL_DIRECTORY`. It also uses a persistent marker to distinguish first boot (FSR) from normal boots and decides whether to run preinstall in force-install mode.

The new requirement adds a runtime configuration source in `/etc/sceneset.conf` (`preinstallLocation`).

The design must preserve current RDK-M behavior when `/etc/sceneset.conf` is missing or does not contain `preinstallLocation`, including existing startup preinstall mode decisions.

## Goals / Non-Goals

**Goals:**
- Add `/etc/sceneset.conf` `preinstallLocation` as a high-priority runtime source for the preinstall directory.
- Keep existing fallback behavior unchanged when the new setting is unavailable.
- Preserve existing startup flow, error handling style, and compatibility with existing deployments.

**Non-Goals:**
- No changes to external Thunder interfaces or plugin contracts.
- No redesign of app download monitoring, reference-app launch orchestration, or unrelated configuration keys.
- No changes to package metadata format or package installer protocol.

## Decisions

### 1. Preinstall directory resolution precedence includes `/etc/sceneset.conf`
Decision:
- Introduce parsing of `/etc/sceneset.conf` for key `preinstallLocation`.
- Use the following precedence for preinstall directory resolution:
  1. `/etc/sceneset.conf` `preinstallLocation` (if file exists and value is non-empty)
  2. PreinstallManager dynamic config `appPreinstallDirectory`
  3. Compile-time `APP_PREINSTALL_DIRECTORY`
  4. Fail initialization if still empty (unchanged failure behavior)

Rationale:
- Satisfies the integration requirement while preserving the existing dynamic/compile-time fallbacks.
- Keeps behavior deterministic and easy to reason about.

Alternatives considered:
- Put `/etc/sceneset.conf` below dynamic plugin config: rejected because the requirement says SceneSet should use `preinstallLocation` when present.
- Replace all existing sources with `/etc/sceneset.conf` only: rejected because it breaks existing RDK-M setups.

### 2. Existing startup preinstall mode logic remains unchanged
Decision:
- Do not add firmware-version-based decisioning.
- Keep existing FSR-based startup preinstall mode selection and factory-app-copy behavior as-is.

Rationale:
- Limits change scope to configuration source precedence and reduces regression risk in startup behavior.

Alternatives considered:
- Add firmware marker decisioning: rejected for this change because it is out of scope.

### 3. Existing preinstall flow and error handling remain intact
Decision:
- Keep current behavior for start/wait/cleanup and error continuation semantics.
- Integrate new config selection at initialization only.

Rationale:
- Minimizes regression risk by changing only selection logic, not execution plumbing.

Alternatives considered:
- Refactor full startup-preinstall pipeline: rejected as unnecessary scope and higher risk.

### 4. Test strategy extends existing L1 coverage
Decision:
- Add tests for:
  - `/etc/sceneset.conf` present with `preinstallLocation`.
  - `/etc/sceneset.conf` absent.
  - `/etc/sceneset.conf` present without key.
  - Invalid/unusable `preinstallLocation` preserving fallback behavior.
  - Fallback compatibility with current dynamic and compile-time directory logic.

Rationale:
- Ensures new behavior and backward compatibility are both validated.

Alternatives considered:
- Manual validation only: rejected because behavior is startup-critical and regression-prone.

## Risks / Trade-offs

- [Risk] Misconfigured `/etc/sceneset.conf` may point to non-existent path.
  → Mitigation: validate resolved path and fall back to existing sources when config is invalid or empty; keep initialization failure only when all sources are exhausted.

- [Trade-off] Higher precedence of `/etc/sceneset.conf` can override dynamic plugin config unexpectedly.
  → Mitigation: log selected source at startup for observability and troubleshooting.

## Migration Plan

1. Implement config parser helper for `/etc/sceneset.conf` and integrate it into preinstall directory resolution order.
2. Keep existing startup preinstall mode logic unchanged.
3. Add/adjust tests for configuration precedence and fallback behavior.
4. Rollout with startup logs indicating selected preinstall directory source.

Rollback:
- Revert to previous preinstall directory resolution logic (dynamic + compile-time fallback only).
- Existing runtime configs and plugin contracts remain compatible, so rollback is low-impact.

## Open Questions

- Should an invalid but non-empty `preinstallLocation` be treated as hard failure or soft fallback to current logic?
