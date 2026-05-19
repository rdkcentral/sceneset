## 1. Configuration Source Integration

- [x] 1.1 Add helper logic to read `/etc/sceneset.conf` and parse `preinstallLocation` as a non-empty value.
- [x] 1.2 Update preinstall directory resolution order to: `/etc/sceneset.conf` `preinstallLocation` -> dynamic `appPreinstallDirectory` -> `APP_PREINSTALL_DIRECTORY`.
- [x] 1.3 Preserve existing initialization failure behavior when all preinstall directory sources resolve to empty.
- [x] 1.4 Add startup logging for selected preinstall directory source and resolved path.

## 2. Backward Compatibility and Fallback Behavior

- [x] 2.1 Ensure missing `/etc/sceneset.conf` preserves existing dynamic/compile-time fallback behavior.
- [x] 2.2 Ensure present `/etc/sceneset.conf` without `preinstallLocation` preserves existing fallback behavior.
- [x] 2.3 Define and implement behavior for invalid/unusable `preinstallLocation` so existing RDK-M logic does not regress.
- [x] 2.4 Ensure existing FSR/factory-app-copy and startup preinstall mode logic remain unchanged.

## 3. Tests and Validation

- [x] 3.1 Add/extend tests for config precedence when `/etc/sceneset.conf` contains `preinstallLocation`.
- [x] 3.2 Add/extend tests for fallback behavior when `/etc/sceneset.conf` is absent or key is missing.
- [x] 3.3 Add/extend tests for invalid/unusable `preinstallLocation`.
- [ ] 3.4 Run L1 tests and verify no regression in existing preinstall startup flow and cleanup behavior.

## 4. Implementation Readiness

- [ ] 4.1 Verify build and static checks pass with the new configuration logic.
- [x] 4.2 Capture rollout notes including startup log expectations and rollback approach for this change.
