# Spec: Preinstall Management

## Overview

At every boot, SceneSet triggers preinstallation of app bundles via PreinstallManager before launching the reference app. On the first boot (Factory Settings Reset / FSR), SceneSet first copies factory app bundles from a compile-time-configured source directory into the preinstall directory, then starts preinstall with force-install mode. On subsequent boots, it uses normal (version-aware) install mode.

---

## Behaviors

### 1. First Boot / Factory Settings Reset (FSR) Detection

- SceneSet detects a first boot by checking for the absence of a marker file at `/opt/persistent/.sceneset_factory_apps_copied`.
- If the marker file is absent, the boot is treated as an FSR.
- If the marker file is present, the boot is treated as a normal (non-FSR) boot.

---

### 2. Factory App Copying (FSR only)

On an FSR boot:

1. SceneSet copies all regular files from the compile-time `FACTORY_APP_PATH` directory into the configured preinstall directory.
2. Subdirectories and non-regular files within `FACTORY_APP_PATH` are skipped.
3. Existing files in the preinstall directory are overwritten.
4. After copying (even if no files were found), SceneSet creates the marker file at `/opt/persistent/.sceneset_factory_apps_copied`.
5. If the marker file cannot be created, SceneSet logs an error but continues.

**Failure modes:**
- If `FACTORY_APP_PATH` does not exist, SceneSet logs an error and continues to the preinstall step anyway.
- Individual file copy failures are logged but do not stop the overall copy operation.
- If directory iteration fails, SceneSet logs an error and returns false, but still proceeds to the preinstall step.

---

### 3. Starting Preinstall

After the factory app copying step (or on a normal boot, skipping it):

- SceneSet calls `PreinstallManager.StartPreinstall(forceInstall)`:
  - `forceInstall = true` on FSR (reinstall all packages regardless of version).
  - `forceInstall = false` on normal boot (only install if a newer version is present).
- If `StartPreinstall` returns an error, SceneSet logs the error and continues the startup flow immediately (skipping the preinstall completion wait, not deleting preinstall files).

---

### 4. Waiting for Preinstall Completion

SceneSet waits for `PreinstallManager.OnPreinstallationComplete` before continuing startup. During this wait, SceneSet monitors per-package installation status events from `PackageManagerRDKEMS.OnAppInstallationStatus`.

Each package status event is parsed as a JSON array of objects with `packageId`, `state`, and optional `version` fields. A package is considered to have failed if its `state` is not `INSTALLED` or `INSTALLING`.

If any package reports a failure state before `OnPreinstallationComplete`, the preinstall phase is recorded as failed.

---

### 5. Post-Preinstall Actions

When `OnPreinstallationComplete` fires:

| Preinstall result | Action |
|---|---|
| All packages succeeded (`INSTALLED` or `INSTALLING`) | Clean up the preinstall directory; then check if reference app is installed and launch it |
| Any package failed | Preserve the preinstall directory (for retry on next boot); still check if app is installed and launch it |

**Failure mode:** If `OnPreinstallationComplete` is never received (e.g. PreinstallManager crashes), the startup flow stalls. SceneSet does not have a timeout or fallback for this case.

---

### 6. Preinstall Directory Cleanup

When cleanup is triggered:

- All files and subdirectories within the preinstall directory are removed.
- Individual removal failures are logged but do not stop the cleanup operation.
- If the preinstall directory does not exist, cleanup is a no-op.

---

### 7. Preinstall Directory Resolution

The preinstall directory is resolved in the following order of priority:

1. `preinstallLocation` from `/etc/sceneset.conf`, when present with a non-empty absolute path.
2. Dynamically fetched from `PreinstallManager` plugin config key `appPreinstallDirectory` at startup via the Thunder Controller.
3. Compile-time `APP_PREINSTALL_DIRECTORY` CMake variable (fallback if dynamic lookup returns empty).
4. If all sources are unavailable or unusable after initialization, SceneSet fails to start.

---

## Constraints

- SceneSet does not implement a retry mechanism for preinstall failures beyond preserving files for the next boot.
- There is no timeout on the wait for `OnPreinstallationComplete`.
- Factory app copying is always triggered exactly once per device lifetime (controlled by the marker file).
- `FACTORY_APP_PATH` must be set at build time if factory app copying is required; it cannot be configured at runtime.
- If `FACTORY_APP_PATH` is set at build time, `APP_PREINSTALL_DIRECTORY` must also be set (enforced at CMake configure time).
