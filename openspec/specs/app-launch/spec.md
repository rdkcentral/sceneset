# Spec: Reference App Launch

## Overview

SceneSet automatically launches a single configured reference application at boot and keeps it running. It handles the full lifecycle: initial launch after preinstall completes, crash recovery, and restart after an OTA update installs a new version.

Home app launch and relaunch telemetry requirements are defined in the [SceneSet Telemetry spec](../sceneset-telemetry/spec.md).

---

## Behaviors

### 1. App Identity

- The reference app is identified by a single application ID string (`referenceAppId`).
- If `referenceAppId` is empty at startup, SceneSet skips preinstall, app launch, and download monitoring entirely.
- See the [Configuration spec](../configuration/spec.md) for how `referenceAppId` is resolved.

---

### 2. Initial Launch

- After the startup preinstall phase completes (see [Preinstall Management spec](../preinstall-management/spec.md)), SceneSet calls `AppManager.IsInstalled(referenceAppId)`.
- If the app is already installed, SceneSet launches it via `AppManager.LaunchApp(referenceAppId, "", "")` from a dedicated launch thread.
- If the app is not installed, SceneSet does not launch it and waits for an `OnAppInstalled` event.
- Launch is performed on a background thread so it does not block the main event loop.

**Failure mode:** If `LaunchApp` returns a non-zero error code, SceneSet logs the error but does not retry automatically.

---

### 3. App Lifecycle State Tracking

SceneSet tracks whether the reference app is currently running using an `m_appLaunched` flag, updated in response to `AppManager.OnAppLifecycleStateChanged` events for the reference app:

| New state | Effect on `m_appLaunched` |
|---|---|
| `RUNNING` or `ACTIVE` | Set to `true` |
| `TERMINATING` | Set to `false` |
| `UNLOADED` | Set to `false` (and triggers restart logic, see below) |

---

### 4. Crash Recovery

When the reference app transitions to `UNLOADED` from `TERMINATING`:

- Default builds automatically restart the app only when the error reason is `APP_ERROR_ABORT`.
- Builds with `RESTART_HOMEAPP_ALWAYS=ON` automatically restart the app for any `TERMINATING` → `UNLOADED` transition.

**Not triggered when:** the app was intentionally killed (e.g. for an OTA restart) — that case is handled by the pending-restart flag (see §5).

---

### 5. OTA Update Restart

When `AppManager.OnAppInstalled` fires for the reference app while the app is running (`m_appLaunched == true`):

1. SceneSet sets a `pendingRestart` flag.
2. Calls `AppManager.KillApp(referenceAppId)`.
3. Waits for the app to reach `UNLOADED` state.
4. On `UNLOADED`, clears `pendingRestart` and launches the new version via a fresh launch thread.

**Failure mode:** If `KillApp` fails, SceneSet logs an error and clears `pendingRestart` without restarting.

---

### 6. Shutdown

On service shutdown (`SIGTERM` / `SIGINT`):

- If the app is running (`m_appLaunched == true`), SceneSet calls `KillApp` to stop the reference app before exiting.
- Any in-progress launch thread is stopped before the process exits.

---

## Constraints

- Only a single reference app is managed per SceneSet instance.
- SceneSet does not launch the app before the startup preinstall phase completes.
- There is no automatic retry policy for `LaunchApp` failures.
- Launch always uses empty `intent` and `source` parameters.
