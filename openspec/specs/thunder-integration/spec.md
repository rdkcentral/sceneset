# Spec: Thunder Integration

## Overview

SceneSet communicates with three WPEFramework (Thunder) plugins exclusively via COMRPC (out-of-process RPC). All connections are established at startup through `RPC::CommunicatorClient` against the Thunder COMRPC socket, and are torn down cleanly on shutdown.

---

## Plugins

| Callsign | Interface | Purpose |
|---|---|---|
| `org.rdk.AppManager` | `Exchange::IAppManager` | Launch, kill, and query app installation; receive lifecycle events |
| `org.rdk.PreinstallManager` | `Exchange::IPreinstallManager` | Trigger bundle preinstallation; receive completion notification |
| `org.rdk.PackageManagerRDKEMS` | `Exchange::IPackageInstaller` | Receive per-package installation status events; source of download directory config |

---

## Behaviors

### 1. Connection Setup

During `initialize()`, SceneSet opens a separate `RPC::CommunicatorClient` for each plugin:

1. `AppManager` client → `IAppManager` interface.
2. `PreinstallManager` client → `IPreinstallManager` interface.
3. `PackageManagerRDKEMS` client → `IPackageInstaller` interface.

Additionally, a fourth COMRPC client is opened **on demand** (inside `fetchPluginConfigValue`) to query plugin configuration via the `Controller` / `Controller.1` shell. This client is short-lived and released after the config query.

**Failure mode:** If any of the three primary interface opens fail, initialization aborts. Any previously opened interfaces are released before returning failure. SceneSet does not retry the connection.

---

### 2. Event Registration

After interfaces are opened, SceneSet registers notification handlers:

| Interface | Notification handler | Events received |
|---|---|---|
| `IAppManager` | `AppManagerEventHandler` | `OnAppInstalled`, `OnAppUninstalled`, `OnAppLifecycleStateChanged`, `OnAppLaunchRequest`, `OnAppUnloaded` |
| `IPreinstallManager` | `PreinstallManagerEventHandler` | `OnPreinstallationComplete`, `OnAppInstallationStatus` |
| `IPackageInstaller` | `PackageInstallerEventHandler` | `OnAppInstallationStatus` |

Registration is performed by calling `Register(handler)` on each interface.

**Failure mode:** If registration fails (interface is null), SceneSet logs a warning and continues. Depending on which handler fails to register:
- No `AppManager` handler → app lifecycle events will not be received; crash recovery and OTA restart will not function.
- No `PreinstallManager` handler → `OnPreinstallationComplete` will never arrive; startup will stall waiting for preinstall completion.
- No `PackageInstaller` handler → per-package install status cannot be tracked; preinstall files will be preserved rather than cleaned up (conservative default).

---

### 3. Dynamic Config Queries

At startup, SceneSet queries two plugin configuration values via the Thunder Controller:

1. Opens a `CommunicatorClient` to the COMRPC socket.
2. Opens `PluginHost::IShell` for `"Controller"` (falls back to `"Controller.1"` if the first fails).
3. Uses `QueryInterfaceByCallsign` to open a shell for the target plugin.
4. Reads `IShell::ConfigLine()` and parses it as JSON.
5. Extracts the requested key as a string.

**Failure modes:**
- If the Controller shell cannot be opened, the query returns empty/failure.
- If the plugin shell cannot be opened, the query returns empty/failure.
- If `ConfigLine()` is not valid JSON, does not contain the key, or the value is not a string, the query returns empty/failure.
- In all failure cases, SceneSet falls back to compile-time defaults (see [Configuration spec](../configuration/spec.md)).

---

### 4. Shutdown and Interface Release

On shutdown (`onTerminate()` or `~SceneSetApp()`):

1. Event handlers are unregistered by calling `Unregister(handler)` on each interface.
   - Exceptions during unregister are caught and logged; they do not prevent further cleanup.
   - If either the interface or the handler is null, unregister is skipped with a log message.
2. Interfaces are released in reverse acquisition order: `IPackageInstaller`, `IPreinstallManager`, `IAppManager`.

---

## Connection Topology

```
SceneSetApp
    │
    ├── CommunicatorClient ──► /tmp/communicator (or THUNDER_ACCESS)
    │       └── IAppManager  ←→  org.rdk.AppManager
    │
    ├── CommunicatorClient ──► /tmp/communicator (or THUNDER_ACCESS)
    │       └── IPreinstallManager  ←→  org.rdk.PreinstallManager
    │
    ├── CommunicatorClient ──► /tmp/communicator (or THUNDER_ACCESS)
    │       └── IPackageInstaller  ←→  org.rdk.PackageManagerRDKEMS
    │
    └── CommunicatorClient (on-demand, short-lived)
            └── IShell (Controller) → queryInterfaceByCallsign → plugin ConfigLine
```

---

## Constraints

- All three primary interfaces must open successfully for SceneSet to start.
- SceneSet does not reconnect if the Thunder socket becomes unavailable after startup (no reconnect loop).
- The on-demand config query client is not cached; a new client is created for each call to `fetchPluginConfigValue`.
- COMRPC socket path defaults to `/tmp/communicator` and can be overridden via the `THUNDER_ACCESS` environment variable.
