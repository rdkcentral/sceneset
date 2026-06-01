# Spec: Configuration

## Overview

SceneSet's behavior is controlled through a layered configuration system: compile-time CMake variables, runtime environment variables, a runtime config file, and dynamic values resolved from Thunder plugin configs at startup.

---

## Configuration Sources (in resolution order)

### 1. Runtime Config File — `referenceAppId`

| Property | Value |
|---|---|
| Path | `/opt/sceneset_app.conf` |
| Format | Plain text; first non-empty line is used as the app ID |
| Priority | Highest — overrides the compile-time default |

If the file exists and its first line is non-empty, that value is used as the reference app ID.  
If the file is absent, unreadable, or empty, the compile-time default (`SCENESET_DEFAULT_APPNAME`) is used.

### 1.1 Runtime Config File — `defaultHomeApp`

| Property | Value |
|---|---|
| Path | `/etc/sceneset.conf` |
| Format | `key=value` lines; `defaultHomeApp=<app-id>` |
| Priority | Highest for final reference app selection when non-empty, when system-config feature is enabled |

When built with `ENABLE_SYSTEM_CONFIG=ON`, if `/etc/sceneset.conf` exists and contains a non-empty `defaultHomeApp`, SceneSet uses that value as the final reference app ID.  
When `ENABLE_SYSTEM_CONFIG=OFF`, `defaultHomeApp` is ignored and SceneSet keeps the previously resolved reference app ID from `/opt/sceneset_app.conf` or `SCENESET_DEFAULT_APPNAME`.

### 1.2 Runtime Config File — `preinstallLocation`

| Property | Value |
|---|---|
| Path | `/etc/sceneset.conf` |
| Format | `key=value` lines; `preinstallLocation=<absolute-path>` |
| Priority | Highest for preinstall directory resolution |

If `/etc/sceneset.conf` exists and contains a non-empty, usable `preinstallLocation`, SceneSet uses that value as the preinstall directory.  
If the file is absent, unreadable, missing `preinstallLocation`, or the value is unusable, SceneSet falls back to existing resolution behavior.

---

### 2. Environment Variables

| Variable | Type | Default | Description |
|---|---|---|---|
| `THUNDER_ACCESS` | String (path) | `/tmp/communicator` | Path to the Thunder COMRPC socket used for all plugin connections |
| `SCENESET_INITIAL_DOWNLOAD_SWEEP` | Boolean flag | `0` (disabled) | Enable an initial sweep of the download directory at monitor startup |

**Boolean flag accepted values:**
- Enabled: `1`, `true`, `yes`, `on` (case-insensitive)
- Disabled: `0`, `false`, `no`, `off` (case-insensitive)
- Any other value: logs a warning and uses the default.

---

### 3. Dynamic Values Resolved from Thunder Plugin Config

Resolved at startup via `PluginHost::IShell::ConfigLine()` through the Thunder Controller:

| Config source | Key | Member populated |
|---|---|---|
| `org.rdk.PackageManagerRDKEMS` | `downloadDir` | Download directory for OTA monitoring |
| `org.rdk.PreinstallManager` | `appPreinstallDirectory` | Preinstall directory for bundle staging |

Dynamic lookup is attempted first. If lookup fails or returns an empty string, the compile-time CMake fallback is used for `appPreinstallDirectory`. There is no runtime fallback for `downloadDir` — if it cannot be resolved, download monitoring is disabled.

For preinstall directory selection, dynamic `appPreinstallDirectory` is used only when `/etc/sceneset.conf` does not provide a usable `preinstallLocation` value.

---

### 4. Compile-Time CMake Variables

| Variable | Default | Description |
|---|---|---|
| `SCENESET_DEFAULT_APPNAME` | `""` | Default reference application ID; used when the runtime config file is absent or empty |
| `FACTORY_APP_PATH` | `""` | Source directory for factory app bundles copied on the first boot |
| `APP_PREINSTALL_DIRECTORY` | `""` | Fallback preinstall directory if dynamic resolution fails |
| `DAC_APP_CERT_PATH` | `/etc/rdk/certs` | Directory containing DAC certificates used for RALF package verification |
| `ENABLE_SYSTEM_CONFIG` | `OFF` | Enables reading `/etc/sceneset.conf` for system config values, including `defaultHomeApp` and `preinstallLocation` |
| `ENABLE_CONFIG_OVERRIDE` | `OFF` | Enables optional `/opt/sceneset.conf` key-level overlay on top of `/etc/sceneset.conf` when system config is enabled |
| `RESTART_HOMEAPP_ALWAYS` | `OFF` | Enables restart behavior on every `TERMINATING` → `UNLOADED` transition (instead of only `APP_ERROR_ABORT`) |
| `DISABLE_REFERENCE_APP_UPDATE` | `OFF` | Set to `ON` to compile out all download monitoring and OTA update support |

**CMake constraints:**
- If `FACTORY_APP_PATH` is set, `APP_PREINSTALL_DIRECTORY` must also be set (enforced at CMake configure time with `FATAL_ERROR`).
- If `SCENESET_DEFAULT_APPNAME` is not set, a `WARNING` is emitted at configure time.
- If `FACTORY_APP_PATH` is not set, a `WARNING` is emitted at configure time.
- If `DAC_APP_CERT_PATH` is not set, a `WARNING` is emitted and the default `/etc/rdk/certs` is used.

---

### 5. Required Configuration for Startup

SceneSet will fail to start (returns false from `initialize()`) if:
- The preinstall directory cannot be determined (dynamic lookup returned empty **and** `APP_PREINSTALL_DIRECTORY` is empty).

SceneSet will skip the reference app flow entirely (but still start) if:
- `referenceAppId` is empty after all resolution steps.

---

## Summary Diagram

```
referenceAppId resolution:
  /opt/sceneset_app.conf (first line)
        │ not found/empty
        ▼
  SCENESET_DEFAULT_APPNAME (compile-time)
                        │ then if ENABLE_SYSTEM_CONFIG=ON and non-empty defaultHomeApp exists
                        ▼
      /etc/sceneset.conf → defaultHomeApp (overrides final referenceAppId)

preinstallDirectory resolution:
  /etc/sceneset.conf → preinstallLocation
        │ missing/empty/unusable
        ▼
  PreinstallManager plugin config → appPreinstallDirectory
        │ empty
        ▼
  APP_PREINSTALL_DIRECTORY (compile-time)
        │ still empty → initialize() returns false

downloadDirectory resolution:
  PackageManagerRDKEMS plugin config → downloadDir
        │ empty
        ▼
  Download monitoring disabled (no fallback)

THUNDER_ACCESS resolution:
  THUNDER_ACCESS env var
        │ absent or empty
        ▼
  /tmp/communicator (hardcoded default)
```
