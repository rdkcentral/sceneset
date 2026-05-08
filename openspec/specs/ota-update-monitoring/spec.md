# Spec: OTA Update Monitoring

## Overview

After the startup preinstall phase completes, SceneSet optionally monitors a configured download directory for newly arrived RALF packages. When a package matching the reference app is found, SceneSet stages it in the preinstall directory so that `PackageManagerRDKEMS` can install it, after which the reference app is restarted with the new version.

This feature can be disabled at build time with `-DDISABLE_REFERENCE_APP_UPDATE=ON`.

---

## Behaviors

### 1. Download Directory Source

- The download directory is resolved dynamically from the `PackageManagerRDKEMS` plugin config key `downloadDir` via the Thunder Controller during initialization.
- If the dynamic lookup returns an empty value, download monitoring is disabled for the session (logged as a warning, not an error).
- The download directory is **not** configurable at runtime via environment variables or a config file.

---

### 2. Monitor Startup

The download monitor starts after `OnPreinstallationComplete` is received (and post-preinstall actions complete). It does **not** start before preinstall completes.

Before starting, SceneSet validates that the download directory:
- Exists on the filesystem.
- Is a directory (not a file or symlink).

If either check fails, the monitor is disabled for the session with a logged error.

---

### 3. File Detection — inotify

SceneSet watches the download directory using `inotify` for `IN_CLOSE_WRITE` and `IN_MOVED_TO` events.

For each detected filename:
- Filenames beginning with `.` (hidden files / dotfiles) are silently ignored.
- Directory entries (`IN_ISDIR`) are ignored.
- All other files are enqueued for processing after a settle delay (see §4).

---

### 4. Settle Delay

Each detected file is held in a settle queue for **1 second** before processing. This guards against processing partially-written files that trigger `IN_CLOSE_WRITE` before they are fully flushed by the downloader.

The settle queue dispatches files in earliest-deadline-first order on a dedicated worker thread.

---

### 5. Initial Directory Sweep

If the environment variable `SCENESET_INITIAL_DOWNLOAD_SWEEP` is set to an enabled value (`1`, `true`, `yes`, `on`), SceneSet scans the download directory at monitor startup and enqueues any pre-existing non-hidden regular files for immediate processing (settle delay = 0 ms).

This covers packages that were downloaded before SceneSet started or before the monitor was started.

Default: disabled.

---

### 6. File Validation

Before processing, each candidate file is validated:
- Must exist on the filesystem.
- Must not be a symlink.
- Must be a regular file.
- Must have a non-zero size.

Files failing any check are silently skipped.

Additionally, SceneSet calls `fsync` on the file before RALF verification. If `fsync` fails, a warning is logged but processing continues.

---

### 7. RALF Package Verification and App ID Matching

SceneSet calls `ralf_support::ExtractPackageMetadata(packagePath, appId, version)` to:
1. Verify the package signature against certificates in the configured `DAC_APP_CERT_PATH` directory.
2. Extract the `appId` and `version` embedded in the package.

If metadata extraction fails (invalid package, bad cert, etc.), the file is skipped with a logged error.

If the extracted `appId` does not match `referenceAppId`, the file is silently ignored (it belongs to a different app).

---

### 8. Version Deduplication

If the reference app is currently running (`m_appLaunched == true`), SceneSet queries the installed version via `AppManager.GetInstalledApps`. If the installed version matches the downloaded package version, the package is skipped (already running this version).

If the app is not running, or the installed version cannot be determined, version deduplication is skipped and the package is staged unconditionally.

---

### 9. Staging to Preinstall Directory

When a valid reference app package is found:

1. SceneSet attempts `fs::rename(source, preinstallDir/filename)` — atomic move within the same filesystem.
2. If rename fails due to a cross-device link or existing file, SceneSet falls back to `fs::copy_file` (overwrite) followed by `fs::remove(source)`.
3. After a successful move or copy, SceneSet calls `fsync` on the destination file. Failure is logged as a warning but does not fail the staging operation.
4. If the source file disappears before staging, the operation is aborted with a logged error.
5. If the preinstall directory does not exist, SceneSet creates it (including parent directories) before staging.

**Failure modes:**
- If the preinstall directory path is not configured, staging is skipped with a logged error.
- If copy fails (after rename fallback), the operation fails and the source file may still be present.

---

### 10. Shutdown

On service shutdown:
- The stop flag for the download monitor thread is set.
- The inotify `poll` loop checks the stop flag on each 500 ms timeout.
- The settle worker thread is signalled to stop and joined before the monitor exits.
- The inotify watch and file descriptor are closed cleanly.

---

## Constraints

- Only packages matching `referenceAppId` are staged; all other packages are silently discarded.
- The feature is entirely disabled when built with `-DDISABLE_REFERENCE_APP_UPDATE=ON`.
- There is no retry mechanism for failed staging operations.
- The settle delay (1 second) is a compile-time constant, not configurable at runtime.
- SceneSet does not clean up the staged file from the preinstall directory itself; that is handled by the post-preinstall cleanup flow (see [Preinstall Management spec](../preinstall-management/spec.md)).
