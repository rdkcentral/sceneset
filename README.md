# SceneSet

SceneSet is an application launcher service for RDK based Set-Top Boxes that automatically launches the RDK reference application during system boot up. It provides a reliable mechanism to start the reference application using the WPEFramework AppManager interface, manage preinstallation of app bundles, and monitor for over-the-air reference app updates.

## Overview

SceneSet is designed to run as a systemd service that:
- Initializes communication with Thunder/WPEFramework
- Registers for reference application lifecycle events via AppManager
- Automatically launches a specified reference application
- Manages app bundle preinstallation via PreinstallManager, including first-boot factory app copying
- Monitors a download directory for new reference app packages and triggers over-the-air updates
- Restarts the reference app when a new version is installed
- Monitors reference application state changes and restarts the app if it crashes
- Handles graceful shutdown via signal handling

## Features

- **Automatic App Launch**: Launches the RDK reference application specified by the compile-time `SCENESET_DEFAULT_APPNAME` setting, with optional runtime override via `/opt/sceneset_app.conf`
- **PreinstallManager Integration**: Triggers app bundle preinstallation at boot and waits for completion before launching the reference app
- **Factory Settings Reset (FSR) Support**: Detects first boot via a marker file and copies factory app bundles to the preinstall directory using force-install mode; subsequent boots use normal (version-aware) install mode
- **Over-the-Air Update Monitoring**: Watches a configured download directory for new RALF packages using inotify; verifies them with libralf and stages them for installation via PreinstallManager
- **Reference App Update & Restart**: Detects when a new version of the reference app is installed and automatically kills and restarts it
- **PackageInstaller Event Monitoring**: Tracks per-package installation status from `org.rdk.AppPackageManager` to confirm successful preinstall before cleaning up staged bundles
- **Crash Recovery**: Automatically restarts the reference app on an ABORT lifecycle error
- **Launch Telemetry Marker**: Emits `ENTS_INFO_Sceneset_LaunchTime` telemetry with launch timing, cumulative relaunch count, and termination nature context when the reference app reaches `ACTIVE`
- **Signal Handling**: Graceful shutdown on SIGTERM/SIGINT signals
- **Systemd Integration**: Reports readiness via `sd_notify` and runs as a `Type=notify` systemd service
and·‌AppPackageManager·‌communication- **Thunder Integration**: Uses WPEFramework COMRPC for AppManager, PreinstallManager, and AppPackageManager communication

## Configuration

### Runtime Environment Variables

- **`THUNDER_ACCESS`**: Optional path to Thunder communicator socket
  - Default: `/tmp/communicator`

- **`SCENESET_INITIAL_DOWNLOAD_SWEEP`**: Process packages already present in the download directory at monitor startup
  - Default: `0` (disabled). Accepted enabled values: `1`, `true`, `yes`, `on`. Accepted disabled values: `0`, `false`, `no`, `off`

### Build-Time Defaults

- **`SCENESET_DEFAULT_APPNAME`**: Compile-time default application ID set through the CMake variable of the same name (can be overridden at runtime via `/opt/sceneset_app.conf`)

### Runtime Configuration File

- **`/opt/sceneset_app.conf`**: Optional plain-text file whose first line overrides `SCENESET_DEFAULT_APPNAME` at runtime. Useful for changing the launched app without rebuilding.

Sample /opt/sceneset_app.conf

```text
root@ipstb-mediabox-rtd1325:/opt# cat /opt/sceneset_app.conf
com.rdkcentral.Netflix
```

## Build Instructions

The project uses CMake for building.

### Dependencies

- **WPEFramework**: Core framework and interfaces (AppManager, PreinstallManager, AppPackageManager)
- **libralf**: RALF package verification and metadata extraction
- **libsystemd**: systemd integration (`sd_notify`)
- **gtest/gmock**: For unit testing

### CMake Build Variables

| Variable | Default | Description |
| --- | --- | --- |
| `SCENESET_DEFAULT_APPNAME` | empty/unset | Default application ID to launch when explicitly provided by the build |
| `FACTORY_APP_PATH` | empty/unset | Path to factory app bundles copied on first boot when explicitly provided by the build |
| `APP_PREINSTALL_DIRECTORY` | empty/unset | Fallback preinstall directory when explicitly provided by the build (also resolved dynamically from the PreinstallManager plugin config key `appPreinstallDirectory`) |
| `DAC_APP_CERT_PATH` | `/etc/rdk/certs` | Directory containing DAC certificates for RALF package verification |
| `ENABLE_SYSTEM_CONFIG` | `OFF` | Enables reading `/etc/sceneset.conf` for system-config keys such as `defaultHomeApp` and `preinstallLocation` |
| `ENABLE_CONFIG_OVERRIDE` | `OFF` | Enables optional `/opt/sceneset.conf` key-level override on top of `/etc/sceneset.conf` |
| `RESTART_HOMEAPP_ALWAYS` | `OFF` | When `ON`, reference app restarts for any `TERMINATING` to `UNLOADED` transition (not only `APP_ERROR_ABORT`) |
| `SCENESET_TELEMETRY_METRICS_SUPPORT` | `OFF` | When `ON`, enables SceneSet T2 telemetry marker emission for home-app launch/relaunch observability |
| `DISABLE_REFERENCE_APP_UPDATE` | `OFF` | Set to `ON` to disable download monitoring and OTA update support |

> **Note:** If `FACTORY_APP_PATH` is set, `APP_PREINSTALL_DIRECTORY` must also be set.
>
> Typical integrator-provided values may include `SCENESET_DEFAULT_APPNAME=com.rdkcentral.refui`, `FACTORY_APP_PATH=/etc/rdk/factoryapps`, and `APP_PREINSTALL_DIRECTORY=/media/apps`, but these are not the built-in CMake defaults unless passed explicitly.

## Startup Flow

1. Connects to AppManager, PreinstallManager, and AppPackageManager via COMRPC
2. Registers for events from all three interfaces
3. Detects first boot (FSR) by checking for marker file `/opt/persistent/.sceneset_factory_apps_copied`
4. On first boot: copies factory app bundles from `FACTORY_APP_PATH` to the preinstall directory
5. Starts preinstall (force mode on FSR, normal mode on subsequent boots) and waits for `OnPreinstallationComplete`
6. If preinstall succeeds, cleans up the preinstall directory; otherwise preserves files for retry on next boot
7. Checks if the reference app is already installed and launches it
8. Unless built with `-DDISABLE_REFERENCE_APP_UPDATE=ON`, starts a download directory monitor for OTA updates

## Telemetry Marker Contract

When built with `SCENESET_TELEMETRY_METRICS_SUPPORT=ON`, SceneSet initializes T2 with component name `sceneset` and emits marker `ENTS_INFO_Sceneset_LaunchTime` when the reference app reaches `ACTIVE`.

Marker payload fields:
- Always present:
  - `appId`: configured reference app ID
  - `launchToActiveMs`: duration from `LaunchApp` request to app `ACTIVE`
- Initial launch only (first successful `ACTIVE` in the SceneSet process lifetime):
  - `totalStartToActiveMs`: total duration from SceneSet startup to app `ACTIVE`
  - `preinstallDurationMs`: preinstall duration (`StartPreinstall` to preinstall completion path)
- Relaunch only:
  - `cumulativeRelaunchCount`: process-lifetime relaunch count
  - `terminationNature`: `none`, `crash`, or `intentional_kill`

## Service Dependencies

SceneSet connects to the following WPEFramework plugins at runtime:
- **`org.rdk.AppManager`** — app lifecycle management and launch
- **`org.rdk.PreinstallManager`** — bundle preinstallation and completion notification
- **`org.rdk.AppPackageManager`** — per-package installation status events and download directory configuration

The systemd service unit requires:
- **Requires/After**: `wpeframework-appmanager.service`
- **ConditionPathExists**: `/opt/ai2managers`

## Unit Tests

L1 unit tests are located in `Tests/L1Tests/` and use gtest/gmock alongside libralf. The test executable is `SceneSetL1TestsIN`.


## License

Licensed under the Apache License, Version 2.0. See the source files for full license text.

## Copyright

Copyright 2024 RDK Management
