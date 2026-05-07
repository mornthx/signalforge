# M9 — Connection Manager (Full Features)

| Field | Value |
|---|---|
| Milestone ID | M9 |
| Sprint | 9 |
| Estimated effort | 5-7 person-days |
| Prerequisites | M8 closed (main at v0.0.9-alpha.1 or later); deferred M8 1-hour soak follow-up |
| Next milestone | M10 (Session Writer) |
| Hard-stop type | **Interface freeze** (`ConnectionManager` API + connections yaml schema) + **Hardware loop verification** (Serial / TCP / UDP / Replay all reconnect cleanly across app restart) |
| Soft-HALT allowed | **No** |
| Branch | `milestone/M9` |

**Cross-reference notation**:

- `[EM §N]` — Execution Manual, section N
- `[Arch §N]` — Architecture document, section N
- `[MR]` — Milestone Roadmap
- `[CM §X]` — CLAUDE.md, section X
- `[ADR-N]` — Architecture Decision Record N
- `[M<n> §N]` — M<n> spec

---

## 1. Goal

User-facing connection management UI that lets users add, configure, edit, remove, connect, disconnect, and persist driver connections (Serial / TCP / UDP / Replay) with optional auto-connect command sequences.

M3 delivered the driver interfaces and a preview ConnectionManager Dialog. M9 completes that work:

- A polished modal dialog for adding/editing connections of any supported driver type
- Persistent connection list across app restarts
- Manual reconnect on startup (no surprise network traffic)
- Connection status displayed in M8's status bar
- Auto-connect command sequences (yaml-configurable init commands)

This milestone freezes the `ConnectionManager` C++ API and the connections yaml schema. After M9, the user can perform the full daily workflow:

1. Open SignalForge
2. Add a connection (e.g., serial port at /dev/ttyUSB0 @ 115200)
3. Connect → frames flow → M5 decoder sees them → signals appear in M6 registry
4. Charts (M8) display signals in real-time
5. Quit; on next start, connections list is preserved (manual reconnect required)

This is the "first daily-use complete loop" of V1. Subsequent milestones (M10 Session Writer, M11 Replay, M12 Performance) round out V1 features but the M9 + earlier milestones together comprise the **minimum viable workflow**.

Quality philosophy from M8: **fail visibly, document edge cases**. Connection failures (port not found, host unreachable, file permission denied) must show clear errors. Missing or invalid yaml must degrade gracefully — empty connection list is better than crash.

---

## 2. Scope

### 2.1 Must deliver

1. **`ConnectionManager`** at `src/connection/connection_manager.{hpp,cpp}` (replaces M3's preview dialog):
   - Owns N `Connection` instances (one per user-configured driver)
   - Provides API to add/edit/remove connections
   - Coordinates lifecycle: connect/disconnect per Connection
   - Persists state to yaml on changes
   - Loads state from yaml on startup

2. **`Connection`** at `src/connection/connection.{hpp,cpp}`:
   - Wraps a single driver instance (Serial / TCP / UDP / Replay) with config + state
   - Lifecycle: Idle → Connecting → Connected → Disconnecting → Idle (or → Error)
   - Owns the underlying `DriverInterface` from M3
   - Exposes Qt signals for state changes
   - Stores last-error message for diagnostic display

3. **`ConnectionConfig`** struct (frozen at M9 close):
   - Common fields: `id`, `displayName`, `driverType` (enum), `autoConnectOnStartup` (bool, default false), `decoderSchemaId` (optional QString)
   - Driver-specific fields wrapped in a variant or per-type struct
   - Auto-connect command sequence: `std::vector<AutoConnectCommand>` (optional)

4. **`ConnectionDialog`** at `src/connection/connection_dialog.{hpp,cpp}` (modal QDialog):
   - Add new connection: select driver type → dynamic config widget appears
   - Edit existing: pre-populated config widget
   - Driver-type-specific config widgets (per decision M9.3 Option Q):
     - **Serial**: port name (autocomplete from `QSerialPortInfo::availablePorts()`), baud rate, data bits, parity, stop bits, flow control
     - **TCP**: host, port, optional connection timeout
     - **UDP**: bind port (or remote host:port for sender mode), receive buffer size
     - **Replay**: file path (file picker), playback speed, loop mode (once / repeat)
   - Common fields visible regardless of driver type: display name, decoder schema selection, auto-connect toggle, auto-connect commands list

5. **`ConnectionListWidget`** at `src/connection/connection_list_widget.{hpp,cpp}`:
   - QListView/QListWidget showing all configured connections
   - Per-row: connection name, driver type icon, current status (Idle/Connected/Error)
   - Buttons / context menu: Connect, Disconnect, Edit, Remove, Add New
   - Double-click a row → opens Edit dialog

6. **`ConnectionStatusWidget`** at `src/connection/connection_status_widget.{hpp,cpp}`:
   - Extends M8's MainWindow status bar
   - Shows: total connections / connected count / errored count
   - Click → opens Connections dialog

7. **`AutoConnectCommandSequence`** (per decision M9.5 Option U):
   - On successful connect, send N commands in sequence with delays
   - Each command: hex bytes or ASCII text + optional response timeout
   - Supports waiting for response (string or hex match) before next command
   - V1 supports raw bytes; V1.5+ adds richer command DSL
   - Errors during auto-connect: log ERROR, drop to "connected but auto-connect failed" state (don't disconnect)

8. **yaml connection schema v1** at `schemas/connections_schema_v1.yaml`:
   - Top-level: `schema_version`, `connections`
   - Per connection: `id`, `displayName`, `driverType`, `driverConfig` (driver-specific), `autoConnectOnStartup`, `decoderSchemaId`, `autoConnectCommands` (optional list)
   - JSON-Schema meta-format at `schemas/connections_schema_v1.json`

9. **Persistence**:
   - Saved to `~/.config/signalforge/connections.yaml` (or platform equivalent: `%APPDATA%/SignalForge/connections.yaml` on Windows)
   - Saved on every change (add/edit/remove/auto-connect-toggle)
   - Loaded on app startup; if file missing: empty state; if invalid: log ERROR + start with empty state

10. **MainWindow integration**:
    - "Connections" menu in menu bar: Add, Edit, Remove, Connect All, Disconnect All
    - Toolbar: "Add Connection" button (calls ConnectionDialog)
    - ConnectionListWidget docked left of M8 chart area (above SignalSelector or as separate panel)
    - Status bar shows ConnectionStatusWidget

11. **Driver-specific configuration validation**:
    - Serial: port exists (validates against `QSerialPortInfo`); baud rate is positive
    - TCP: host is non-empty, port in 1-65535
    - UDP: port in 1-65535, buffer size positive
    - Replay: file exists + readable; playback speed > 0
    - Validation errors shown inline in dialog before "Apply" button enables

12. **Reconnect-on-startup behavior** (per decision M9.2 Option X):
    - On app startup, load yaml → populate ConnectionListWidget with all connections in **Idle** state
    - Do NOT auto-connect (per decision M9.2)
    - Show banner / hint: "X connections loaded. Click Connect to start."
    - User clicks Connect on each connection (or "Connect All")

13. **Connection lifecycle Qt signals**:
    - `Connection::stateChanged(State)` — Idle / Connecting / Connected / Disconnecting / Error
    - `Connection::errorOccurred(QString errorMessage)`
    - `ConnectionManager::connectionAdded(QString id)`
    - `ConnectionManager::connectionRemoved(QString id)`
    - `ConnectionManager::connectionStateChanged(QString id, Connection::State newState)`

14. **Integration tests** at `tests/integration/`:
    - `test_connection_manager_lifecycle.cpp` — add 1 connection, connect, disconnect, remove
    - `test_connection_persistence.cpp` — add 2 connections, save yaml, reload from yaml, verify both present
    - `test_connection_dialog_validation.cpp` — invalid configs rejected; valid ones accepted
    - `test_auto_connect_commands.cpp` — connection with auto-connect commands; verify sent in order
    - `test_connection_status_widget.cpp` — connection status reflects in widget label
    - `test_replay_driver_full_cycle.cpp` — Replay driver via ConnectionManager (preparation for M11)
    - `test_serial_loopback.cpp` — Serial driver with mock port (or skipped on CI; manual hardware verification)

15. **Unit tests** ≥ 80% coverage on connection modules

16. **Manual hardware verification protocol** at `docs/m9-hardware-verification.md`:
    - Steps for verifying Serial / TCP / UDP / Replay against real hardware (or mocks)
    - Run before M9 close
    - Documented results in M9-done.md (manual gate, not CI)

17. **Doxygen** on all public declarations

18. **`.claude/M9-done.md`** with standard completion report + freeze record

### 2.2 Must not do

1. **No modifications to M2/M3/M4/M5/M6/M7/M8 frozen `.hpp`**. If freeze-scope change seems needed, HALT.
2. **No always-auto-reconnect mode**. Manual connect on startup (decision M9.2).
3. **No multi-modal config dialog with tabs**. Single dialog with dynamic widget (decision M9.3).
4. **No connection sharing / export to other users**. V1 is single-user.
5. **No connection groups / folders**. V1 flat list. V1.5+ may add grouping.
6. **No remote connection management** (e.g., via SSH to remote SignalForge instance). V1 local only.
7. **No advanced auto-connect command DSL** (no conditionals, no loops). V1 sequential commands only. V1.5+ may add DSL.
8. **No new top-level dependencies**. Use existing Qt SerialPort + Qt Network + yaml-cpp.
9. **No driver-internal changes**. M3's `DriverInterface`, `SerialDriver`, etc. remain frozen. M9 only adds the management layer.
10. **No connection editor for the underlying schema**. `decoderSchemaId` is selected from existing M5 schemas; M9 doesn't edit decoder schemas. Schema editing UI is V1.5+.
11. **No connection status remoting** (no syslog, no Prometheus push, no email alerts). V1 local UI only.

---

## 3. Design Decisions (locked by this spec)

### 3.1 Connection configuration via modal dialog (decision M9.1 Option A)

`ConnectionDialog` is modal QDialog. Opens on "Add Connection" or "Edit" actions.

**Why modal**:
- Simple, established Qt pattern
- User must complete or cancel before continuing — no half-configured state
- Driver-specific config requires immediate feedback (port autocomplete, file picker)
- Side panel pattern (Option B) was considered but adds complexity for minimal benefit in V1

**Dialog layout**:
- Top: Driver Type combo box (Serial / TCP / UDP / Replay)
- Below: Driver-specific config widget (dynamic per Driver Type)
- Common section: Display Name (text), Decoder Schema (combo from M5 schemas), Auto-connect on Startup (checkbox)
- Auto-connect commands section: editable list of command rows (V1 minimal)
- Bottom: OK / Cancel / (when editing) Test Connection button

V1.5+ may add: side panel mode toggle, multi-edit mode, drag-drop reordering.

### 3.2 Manual reconnect on startup (decision M9.2 Option X)

On app start:
- Load `connections.yaml`
- Populate connection list, all in Idle state
- Show banner: "X connections loaded. Click Connect to start."

**Rationale**:
- User control: avoids unexpected network traffic (e.g., laptop wakeup → SignalForge auto-connects to dev board not powered on)
- Predictable: app start is fast; no network delays
- Simple mental model: connection state = user action

V1.5+ may add per-connection "auto-connect on startup" toggle (Option Y) but **default off**.

`ConnectionConfig::autoConnectOnStartup` exists (per spec §2.1-3) for forward compatibility but **always reads as false in V1 reconnect logic**. The setting can be toggled in dialog and saved, but reconnect logic ignores it. V1.5+ enables the toggle's effect.

This split (config exists, behavior deferred) prevents schema migration in V1.5+. Forward-compatible.

### 3.3 Dynamic driver-specific config widget (decision M9.3 Option Q)

Single QDialog with one container widget that swaps content based on Driver Type combo selection.

**Why dynamic, not tabbed**:
- Tabbed dialog adds visual complexity (4 tabs for 4 driver types)
- Driver-specific config is mutually exclusive — only one at a time
- Smaller dialog footprint
- Simpler state management

**Implementation**: a `QStackedWidget` with one page per driver type; combo box selection changes current index.

### 3.4 Status bar connection status (decision M9.4 Option R)

`ConnectionStatusWidget` extends M8's MainWindow status bar (which already has FPS / dropped / throttled labels).

Layout:
```
[FPS: 30 | Dropped: 0 | Throttled: 0] | [Connections: 2/3 connected] | [Status: 0 errors]
```

Click on the connections widget → opens main Connection List dialog.

**Why status bar, not panel**:
- M8 already has status bar
- Connection state is "ambient information," not the focus
- Connection list panel exists for full management
- Tray icon (Option T) was considered but adds platform-specific complexity for marginal benefit

V1.5+ may add: rich panel with per-connection state (Option S).

### 3.5 Auto-connect commands via yaml (decision M9.5 Option U)

`AutoConnectCommand` struct:
```cpp
struct AutoConnectCommand {
    QString name;                        // For diagnostics, e.g. "Send INIT"
    QByteArray payload;                  // Raw bytes to send
    std::optional<QByteArray> expected;  // If set, wait for matching response
    std::chrono::milliseconds timeout{1000};  // Per-command timeout
    std::chrono::milliseconds delayBefore{0}; // Delay before sending
};
```

On successful Connection state → Connected, the manager iterates the `autoConnectCommands` list:
1. Wait `delayBefore`
2. Send `payload` via driver's write API
3. If `expected` set: wait for matching response or timeout
4. Move to next command

Errors:
- Timeout: log WARN, increment `connection_auto_command_timeouts` metric, continue to next command
- Driver write fails: log ERROR, abort sequence, but **do not disconnect** — connection stays up; user sees "auto-connect failed" status
- Expected mismatch: log WARN, treat as success-with-warning, continue

**Rationale**:
- Embedded debugging often requires init commands (e.g., "ENABLE_TELEMETRY\r\n")
- yaml-configurable means non-coder can edit without rebuilding
- V1 keeps it simple: sequential commands, no DSL

V1.5+ may add: conditional commands, command grouping, response parsing.

### 3.6 No soft-HALT (inherits M2-M8)

### 3.7 Metric naming

Per `<module>_<metric>_<scope>` convention:

- `connection_total` (gauge): total configured connections
- `connection_connected_total` (gauge): currently connected
- `connection_errored_total` (gauge): in error state
- `connection_state_transitions_<connectionId>` (counter): per-connection lifecycle transitions
- `connection_auto_command_timeouts` (counter): auto-connect command timeouts
- `connection_auto_command_failures` (counter): auto-connect command write failures
- `connection_yaml_load_errors` (counter): startup yaml parse failures

---

## 4. Key Implementation Details

### 4.1 `ConnectionManager` class

Place at `src/connection/connection_manager.hpp`.

```cpp
// src/connection/connection_manager.hpp
#pragma once

#include "connection/connection.hpp"
#include "driver/driver_interface.hpp"
#include "decoder/decoder_registrar.hpp"

#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>
#include <unordered_map>

namespace signalforge::connection {

/// Manager of all connection instances + persistence.
///
/// Lifecycle:
/// - Constructed in MainWindow with refs to DecoderRegistrar (for schema lookup)
/// - loadConfigFile() called on app start; creates Connection instances in Idle
/// - User actions (connect / disconnect / add / edit / remove) modify state
/// - State changes auto-saved to yaml
///
/// Threading: lives on the main thread. Internal Connection instances may
/// have driver IO on background threads (per M3 driver topology); connection
/// state is reported back via Qt signals.
///
/// Freeze scope: this class is frozen at M9 close.
class ConnectionManager : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ConnectionManager)

public:
    explicit ConnectionManager(
        signalforge::decoder::DecoderRegistrar& decoderRegistrar,
        QObject* parent = nullptr);
    ~ConnectionManager() override;

    // Connection management

    /// Add a new connection. Persists to yaml.
    /// Returns connection ID (auto-generated if config.id is empty).
    [[nodiscard]] QString addConnection(const ConnectionConfig& config);

    /// Edit an existing connection (must be Idle).
    /// Persists to yaml.
    /// Returns false if connection is currently connected (must disconnect first).
    [[nodiscard]] bool editConnection(const QString& id, const ConnectionConfig& newConfig);

    /// Remove a connection (must be Idle). Persists to yaml.
    [[nodiscard]] bool removeConnection(const QString& id);

    // Connection lifecycle

    /// Initiate connect for a connection. State transitions to Connecting.
    [[nodiscard]] bool connectConnection(const QString& id);

    /// Initiate disconnect.
    [[nodiscard]] bool disconnectConnection(const QString& id);

    /// Connect all configured connections.
    void connectAll();

    /// Disconnect all currently-connected.
    void disconnectAll();

    // Lookup

    [[nodiscard]] Connection* connection(const QString& id) const;
    [[nodiscard]] QStringList connectionIds() const;
    [[nodiscard]] std::size_t connectionCount() const noexcept;
    [[nodiscard]] std::size_t connectedCount() const noexcept;
    [[nodiscard]] std::size_t erroredCount() const noexcept;

    // Persistence

    /// Load connections from yaml. Returns false on parse error (still
    /// produces empty manager state).
    [[nodiscard]] bool loadConfigFile(const QString& path);

    /// Save current connections to yaml. Returns false on write error.
    [[nodiscard]] bool saveConfigFile(const QString& path) const;

    /// Default config file path (~/.config/signalforge/connections.yaml or
    /// platform equivalent).
    [[nodiscard]] static QString defaultConfigPath();

signals:
    void connectionAdded(const QString& id);
    void connectionRemoved(const QString& id);
    void connectionStateChanged(const QString& id, Connection::State newState);
    void connectionError(const QString& id, const QString& errorMessage);

private:
    signalforge::decoder::DecoderRegistrar* decoderRegistrar_;
    std::unordered_map<QString, std::unique_ptr<Connection>> connections_;
    QString configPath_;  // Auto-saved to this path on changes
};

}  // namespace signalforge::connection
```

### 4.2 `Connection` class

Place at `src/connection/connection.hpp`.

```cpp
// src/connection/connection.hpp
#pragma once

#include "driver/driver_interface.hpp"

#include <QObject>
#include <QString>
#include <chrono>
#include <memory>
#include <vector>

namespace signalforge::connection {

/// Driver type enumeration.
enum class DriverType {
    Serial,
    Tcp,
    Udp,
    Replay,
};

/// One auto-connect command (executed after successful connect).
struct AutoConnectCommand {
    QString name;
    QByteArray payload;
    std::optional<QByteArray> expected;
    std::chrono::milliseconds timeout{1000};
    std::chrono::milliseconds delayBefore{0};
};

/// Driver-specific configuration. Variant per type.
struct SerialConfig {
    QString portName;
    int baudRate = 115200;
    int dataBits = 8;
    int parity = 0;       // 0=None, 1=Even, 2=Odd
    int stopBits = 1;
    int flowControl = 0;  // 0=None, 1=Hardware, 2=Software
};

struct TcpConfig {
    QString host;
    int port = 0;
    std::chrono::milliseconds connectTimeout{5000};
};

struct UdpConfig {
    int bindPort = 0;
    QString remoteHost;     // Optional; empty = listen mode
    int remotePort = 0;     // Optional
    int receiveBufferSize = 65536;
};

struct ReplayConfig {
    QString filePath;
    double playbackSpeed = 1.0;
    bool loop = false;
};

using DriverConfig = std::variant<SerialConfig, TcpConfig, UdpConfig, ReplayConfig>;

/// Common connection configuration.
struct ConnectionConfig {
    QString id;                                          // Auto-generated if empty at add
    QString displayName;                                 // User-friendly
    DriverType driverType;                               
    DriverConfig driverConfig;                           // Variant per driver type
    QString decoderSchemaId;                             // M5 schema reference
    bool autoConnectOnStartup = false;                   // V1.5+ effect; V1 reads as false
    std::vector<AutoConnectCommand> autoConnectCommands;
};

/// Single connection wrapping a driver + config + state.
class Connection : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Connection)

public:
    enum class State {
        Idle,
        Connecting,
        Connected,
        Disconnecting,
        Error,
    };

    explicit Connection(ConnectionConfig config, QObject* parent = nullptr);
    ~Connection() override;

    // Lifecycle
    [[nodiscard]] bool connectDriver();
    [[nodiscard]] bool disconnectDriver();

    // State accessors
    [[nodiscard]] State state() const noexcept;
    [[nodiscard]] const QString& lastError() const noexcept;
    [[nodiscard]] const ConnectionConfig& config() const noexcept;
    [[nodiscard]] signalforge::driver::DriverInterface* driver() const noexcept;

    // Mutate config (only when Idle)
    [[nodiscard]] bool setConfig(const ConnectionConfig& newConfig);

signals:
    void stateChanged(Connection::State newState);
    void errorOccurred(const QString& errorMessage);
    void autoConnectCommandSent(const QString& commandName);
    void autoConnectCompleted(bool success);

private:
    ConnectionConfig config_;
    std::unique_ptr<signalforge::driver::DriverInterface> driver_;
    State state_ = State::Idle;
    QString lastError_;
};

}  // namespace signalforge::connection
```

### 4.3 yaml connection schema v1

Example at `examples/connections/example_connections.yaml`:

```yaml
schema_version: 1

connections:
  - id: dev-board-serial
    displayName: "Dev Board (USB Serial)"
    driverType: serial
    driverConfig:
      portName: /dev/ttyUSB0
      baudRate: 115200
      dataBits: 8
      parity: 0
      stopBits: 1
      flowControl: 0
    decoderSchemaId: dev-board-frame-v1
    autoConnectOnStartup: false
    autoConnectCommands:
      - name: "Enable telemetry"
        payload: !!binary "RU5BQkxFX1RFTEVNRVRSWQo="
        expected: !!binary "T0sK"
        timeout: 1000
        delayBefore: 100

  - id: tcp-monitor
    displayName: "Network Monitor"
    driverType: tcp
    driverConfig:
      host: 192.168.1.100
      port: 9090
    decoderSchemaId: simple-protocol-v1
    autoConnectOnStartup: false
```

Frozen yaml keys (M9 close):
- Top-level: `schema_version`, `connections`
- Per-connection: `id`, `displayName`, `driverType`, `driverConfig`, `decoderSchemaId`, `autoConnectOnStartup`, `autoConnectCommands`
- Per-command: `name`, `payload`, `expected`, `timeout`, `delayBefore`
- Per-driverConfig: type-specific keys per the SerialConfig/TcpConfig/UdpConfig/ReplayConfig structs in §4.2

### 4.4 Connection state machine

```
                          ┌─ connect() ──────────────┐
                          │                          │
[Idle] ─── connect() ──▶ [Connecting]              │
   ▲                          │                       │
   │                          │ (driver.connect()→OK) │
   │                          ▼                       │
   └────disconnect() ── [Connected] ─── error ──▶ [Error]
                                          │             │
                                          └─ retry ────┘
```

Transitions:
- `Idle → Connecting`: `connectDriver()` called; driver `connect()` initiated
- `Connecting → Connected`: driver `connect()` succeeded; trigger auto-connect commands
- `Connecting → Error`: driver `connect()` failed; `lastError` set
- `Connected → Disconnecting`: `disconnectDriver()` called
- `Disconnecting → Idle`: driver `disconnect()` succeeded
- `Connected → Error`: runtime error from driver (e.g., port lost mid-session)
- `Error → Idle`: `disconnectDriver()` clears error, returns to Idle (may retry connect)

### 4.5 Driver instantiation per type

Within `Connection::Connection()`, dispatch by driverType:

```cpp
Connection::Connection(ConnectionConfig config, QObject* parent)
    : QObject(parent), config_(std::move(config))
{
    using namespace signalforge::driver;
    switch (config_.driverType) {
        case DriverType::Serial:
            driver_ = std::make_unique<SerialDriver>(
                std::get<SerialConfig>(config_.driverConfig));
            break;
        case DriverType::Tcp:
            driver_ = std::make_unique<TcpDriver>(
                std::get<TcpConfig>(config_.driverConfig));
            break;
        case DriverType::Udp:
            driver_ = std::make_unique<UdpDriver>(
                std::get<UdpConfig>(config_.driverConfig));
            break;
        case DriverType::Replay:
            driver_ = std::make_unique<ReplayDriver>(
                std::get<ReplayConfig>(config_.driverConfig));
            break;
    }
    
    // Wire driver state callbacks → Connection::stateChanged
    // ...
}
```

**Note on M3 driver constructors**: M3's drivers may need slight constructor adaptation to accept the new `*Config` structs. This is **internal to driver implementation**, not a frozen-interface change; M3's `DriverInterface` (the abstract API) is unchanged.

If a driver constructor change requires an M3 interface tweak that's not purely additive, **HALT** and consider whether ADR-006 is needed.

### 4.6 Persistence layer

`ConnectionManager` persists to yaml on every `addConnection` / `editConnection` / `removeConnection`:

```cpp
QString ConnectionManager::addConnection(const ConnectionConfig& config) {
    auto id = config.id.isEmpty() ? generateId() : config.id;
    
    auto fullConfig = config;
    fullConfig.id = id;
    
    auto conn = std::make_unique<Connection>(fullConfig);
    // ... wire signals
    connections_[id] = std::move(conn);
    
    auto saveOk = saveConfigFile(configPath_);
    if (!saveOk) {
        SF_LOG_WARN("Connection added but failed to persist to {}", configPath_);
    }
    
    emit connectionAdded(id);
    return id;
}
```

Errors during save are non-fatal — the connection still exists in memory; user may retry save manually via "File → Save All".

### 4.7 ConnectionDialog dynamic widget

`ConnectionDialog` uses `QStackedWidget` for driver-specific config:

```
┌───────────────────────────────────────┐
│ Driver Type: [Serial ▼]              │
├───────────────────────────────────────┤
│ ┌─────────────────────────────────┐ │
│ │ (Serial config widget visible)   │ │
│ │  Port: [/dev/ttyUSB0  ▼]         │ │
│ │  Baud: [115200      ]            │ │
│ │  ...                              │ │
│ └─────────────────────────────────┘ │
├───────────────────────────────────────┤
│ Display Name: [____________________] │
│ Decoder Schema: [dev-board-v1   ▼]   │
│ ☐ Auto-connect on startup            │
├───────────────────────────────────────┤
│ Auto-connect commands:               │
│  [+] Add command                     │
│  ────────────────────                │
│  [Cancel] [Test Connection] [OK]     │
└───────────────────────────────────────┘
```

When user changes Driver Type combo, the QStackedWidget current index changes; previous widget's config is lost (but a Cancel + reopen pattern is fine for V1).

### 4.8 ReplayDriver + decoder schema integration

ReplayDriver (M3 skeleton) is completed in M9 to play back a frame log file. **This intersects with M11 (Replay)**, but M9 only delivers the **driver**, not the full replay UX (timeline scrubber, playback controls, etc.). M11 builds on M9's ReplayDriver to add UX.

M9 ReplayDriver:
- Reads frame log from file
- Plays at `playbackSpeed` rate
- Loops if configured
- Emits frames to FramePipeline like other drivers
- Supports pause / resume via DriverInterface API (M11 will use this)

---

## 5. Performance gates

M9 is primarily a UI / persistence milestone; performance is not the primary concern. But ensure:

1. **Connection list load** under 100ms for 100 connections (yaml parse)
2. **Connection state UI update** within 50ms of state change (Qt signal → label update)
3. **No memory growth** in Connection's idle state (verify with 100-connection × 10-minute idle test)
4. **Auto-connect commands** sent within 100ms of Connected state (excluding `delayBefore`)

These are not benchmark-gated but verified by integration tests.

---

## 6. Freeze protocol

### 6.1 What freezes at M9 close

**C++ interfaces**:
- `src/connection/connection_manager.hpp`: `ConnectionManager` class
- `src/connection/connection.hpp`: `Connection` class, `Connection::State` enum, `ConnectionConfig` struct, `DriverType` enum, `AutoConnectCommand` struct, `DriverConfig` variant + 4 *Config structs

**yaml schema**:
- `~/.config/signalforge/connections.yaml` schema (top-level + per-connection + per-driverConfig + per-command keys)

Once frozen, modifications require new ADR.

### 6.2 What does NOT freeze

- `ConnectionDialog` UI layout (visual changes OK without ADR)
- `ConnectionListWidget` UI (additive cells OK without ADR)
- Default values in `*Config` structs (tunable based on V1 user feedback)
- Auto-connect command delivery sequencing details (e.g., parallel vs serial; M9 chooses serial, V1.5+ may add parallel)

### 6.3 Freeze record format

`.claude/M9-done.md`:

```markdown
## Freezes established in this milestone

Frozen per M9 spec §6.1.

| File | sha256 |
|---|---|
| `src/connection/connection_manager.hpp` | <...> |
| `src/connection/connection.hpp` | <...> |
| `schemas/connections_schema_v1.yaml` | <...> |
| `schemas/connections_schema_v1.json` | <...> |

Modifications require new ADR per M9 §6.2.
```

---

## 7. M9-specific HALT triggers

Beyond CLAUDE.md §HALT:

1. **Modification to M2/M3/M4/M5/M6/M7/M8 frozen `.hpp`** → HALT.
2. **M3 driver interface needs non-additive change** to support per-driver configuration → HALT (consider ADR for M3 amendment).
3. **yaml schema breaking change** required mid-implementation (e.g., realizing V1 `autoConnectOnStartup` semantics need to change) → HALT (re-evaluate decision M9.2).
4. **Connection dialog can't validate driver config** for some driver type (e.g., Serial port enumeration hangs on slow filesystem) → HALT, propose mitigation.
5. **Persistence yaml round-trip not bit-identical** when saving/loading without changes → HALT (yaml-cpp formatting issue requires investigation).
6. **Qt SerialPort not available on target platform** (extremely unlikely on Ubuntu 24.04) → HALT, document hardware limitation.
7. **Auto-connect command cannot reliably wait for response** (timing flakiness in tests) → HALT, simplify or re-design.

---

## 8. Acceptance criteria

### 8.1 Build and test

- [ ] Debug, Release, debug-asan all build clean under C++23 (GCC 13)
- [ ] All unit + integration tests pass under all three presets
- [ ] Coverage ≥ 80% per §2.1-15
- [ ] CI green on milestone/M9 head

### 8.2 Functional correctness

- [ ] Add connection via dialog: each driver type configurable + saved to yaml
- [ ] Edit connection: existing config loaded into dialog, changes saved
- [ ] Remove connection: removed from list + yaml
- [ ] Connect/disconnect lifecycle works for each driver type
- [ ] Connection state reflected in status bar
- [ ] Auto-connect commands sent in order on connect
- [ ] Persistence yaml round-trip preserves all fields
- [ ] Missing yaml file: empty manager (no crash)
- [ ] Invalid yaml: log ERROR, empty manager

### 8.3 Manual hardware verification

Before M9 close, run the protocol in `docs/m9-hardware-verification.md`:

- [ ] Serial: connect to known device (mock or real); verify frames flow
- [ ] TCP: connect to test server; verify frames flow
- [ ] UDP: bind to port; verify frame reception
- [ ] Replay: play recorded frame log; verify timing within 5% of `playbackSpeed`

Results documented in M9-done.md "Manual verification" section.

### 8.4 Persistence

- [ ] yaml saves on every change (verified via file mtime)
- [ ] yaml loads correctly on next startup
- [ ] Cross-version forward compatibility: V1.5+ readers should accept V1 yaml (additive schema only)

### 8.5 Freeze record

- [ ] M9-done.md has Freezes section per §6.3
- [ ] Sha256s recorded for 4 frozen artifacts (2 hpp + 2 schema files)
- [ ] No modifications to M2-M8 frozen files

### 8.6 Hand-off

- [ ] M9-done.md hand-off section covers:
  - For M10 Session Writer: writes from same `SignalBufferRegistry`; Connection lifecycle drives signal flow into the registry
  - For M11 Replay: M9's ReplayDriver is the foundation; M11 adds replay-specific UX
  - For M12 Performance: connection mgmt is non-perf-critical but verify no regression in driver hot paths

---

## 9. Notes for CC

- **M3 driver constructors may need adaptation**. M3 declared driver constructors that took config in some form. M9 introduces `SerialConfig`, `TcpConfig`, etc. structs that replace ad-hoc parameters. Adapting constructors is OK if it's purely additive (overload + deprecated alias). If non-additive (signature change), HALT trigger #2 fires.

- **yaml-cpp + binary fields**. Auto-connect command `payload` is `QByteArray`. yaml-cpp's binary type (`!!binary`) handles base64-encoded bytes. Test the round-trip carefully; trailing newlines in commands like `"INIT\n"` are common and should be preserved.

- **QStackedWidget pattern is stable Qt**. Driver-specific config dialog is straightforward; no fancy custom widgets needed. Use existing Qt form layout patterns.

- **Manual hardware verification is necessary**. Some bugs only surface with real hardware (USB device disconnects, network switch resets). Run the protocol; document edge cases in M9-done.md. CI cannot replace this for V1.

- **Don't over-invest in dialog visual polish**. V1 priority is functional correctness, not "looks beautiful." Use Qt default style; avoid custom theming. M13 packaging will polish if needed.

- **Decoder schema selection in dialog**: query `DecoderRegistrar::availableSchemas()` (M5 frozen API). If user selects a schema that doesn't exist when connection is created from yaml, log WARN and use a fallback / display error.

- **M8 1-hour soak follow-up** (per M9 plan §0): include this in the plan, schedule for an opportune moment (e.g., S5 or later).

---

## 10. Closing note

M9 completes the **first daily-use loop** of V1. After M9, the user can:

1. Start SignalForge
2. Configure a connection
3. Connect to a device
4. See decoded signals as charts
5. Disconnect and quit
6. On next start, the connection list is preserved

The remaining V1 milestones (M10 Session Writer, M11 Replay, M12 Performance, M13 Packaging) round out the feature set but **M9 is the inflection point** where SignalForge becomes useful as a daily debugging tool.

Quality discipline:

- Connection failures must produce clear, actionable error messages. "Cannot connect" is bad. "TCP connection to 192.168.1.100:9090 timed out after 5 seconds. Verify host is reachable." is good.
- Persistence must be bulletproof: yaml save/load round-trip, missing/invalid yaml graceful degradation.
- Driver lifecycle correctness: connecting twice without disconnecting first should fail gracefully, not crash.

Performance is not gated; UI work doesn't have a performance cert milestone here. M12 covers cross-cutting performance optimization across all milestones.

When in doubt about user-facing decisions (button placement, dialog layout, default values), choose the **most predictable / least surprising** option. V1 prioritizes correctness and predictability over fancy UX.
