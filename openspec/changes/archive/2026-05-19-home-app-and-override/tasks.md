## 1. Configuration Sources and Precedence

- [x] 1.1 Add runtime loading for `/etc/sceneset.conf` in SceneSet startup config resolution
- [x] 1.2 Parse and validate `defaultHomeApp` from `/etc/sceneset.conf` using existing accepted config value semantics
- [x] 1.3 Implement compile-flag gating so `/opt` override is evaluated only when `ENABLE_CONFIG_OVERRIDE` is enabled
- [x] 1.4 Implement key-level overlay merge where `/opt` overrides matching keys from `/etc` and preserves non-overridden `/etc` keys
- [x] 1.5 Ensure missing or unreadable `/etc/sceneset.conf` is non-fatal and falls back to current behavior

## 2. Home App Launch Behavior

- [x] 2.1 Wire resolved `defaultHomeApp` into initial startup launch selection for EntOS behavior
- [x] 2.2 Constrain initial startup flow to launch only the resolved `defaultHomeApp`
- [x] 2.3 Preserve existing launch behavior when `defaultHomeApp` is missing or invalid

## 3. EntOS Scope and Build Safety

- [x] 3.1 Add EntOS-specific scoping for `/etc/sceneset.conf` behavior so non-EntOS behavior remains unchanged
- [x] 3.2 Verify non-debug builds ignore `/opt` override input even when the file is present
- [x] 3.3 Add logging for config source selection and fallback decisions to aid verification without changing runtime semantics

## 4. Test Coverage

- [x] 4.1 Add/update unit tests for base `/etc` config resolution with valid `defaultHomeApp`
- [x] 4.2 Add/update unit tests for partial `/opt` override merge in debug builds (for example key `b` overridden, key `a` preserved)
- [x] 4.3 Add/update unit tests verifying `/opt` override is ignored in non-debug builds
- [x] 4.4 Add/update unit tests verifying missing `/etc/sceneset.conf` preserves existing startup behavior and is non-fatal
- [x] 4.5 Add/update launch-flow tests verifying only `defaultHomeApp` is launched during initial startup

## 5. Validation and Integration

- [x] 5.1 Validate EntOS integration expectations for provisioning `/etc/sceneset.conf` with EPG appId as `defaultHomeApp`
- [ ] 5.2 Run build and test suites for SceneSet and confirm no regressions in existing startup and app-launch behavior
- [x] 5.3 Document final behavior and precedence order in change notes and readiness summary
