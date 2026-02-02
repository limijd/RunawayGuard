# RunawayGuard Architecture

## Overview

RunawayGuard is a Linux process monitoring tool that detects runaway processes (high CPU, hangs, memory leaks) and alerts users. It uses a daemon + GUI architecture with Unix socket IPC.

```
┌─────────────────┐     Unix Socket      ┌──────────────────┐
│   Qt6 GUI       │◄───────────────────►│   Rust Daemon    │
│  (C++ client)   │   JSON-line protocol │  (background)    │
└─────────────────┘                      └──────────────────┘
                                                  │
                                                  ▼
                                         ┌──────────────────┐
                                         │  SQLite Database │
                                         │  (alerts, config)│
                                         └──────────────────┘
```

## Directory Structure

```
RunawayGuard/
├── daemon/                    # Rust daemon (process monitor)
│   ├── src/
│   │   ├── main.rs           # Entry point, request handler, monitoring loop
│   │   ├── lib.rs            # Library exports
│   │   ├── collector/        # Process collection
│   │   │   ├── mod.rs        # ProcessCollector trait
│   │   │   └── linux.rs      # Linux /proc implementation
│   │   ├── detector.rs       # Anomaly detection (CPU, hang, memory)
│   │   ├── config.rs         # TOML configuration
│   │   ├── db.rs             # SQLite operations
│   │   ├── socket.rs         # Unix socket server
│   │   ├── protocol.rs       # IPC message definitions
│   │   ├── notifier.rs       # Desktop notifications (notify-rust)
│   │   ├── executor.rs       # Process actions (kill signals)
│   │   └── learner.rs        # (Future) ML-based learning
│   ├── tests/
│   │   ├── collector_tests.rs
│   │   ├── config_tests.rs
│   │   ├── db_tests.rs
│   │   └── integration_tests.rs
│   └── Cargo.toml
├── gui/                       # Qt6 GUI (C++)
│   ├── src/
│   │   ├── main.cpp          # Application entry
│   │   ├── MainWindow.h/cpp  # Main window with tabs
│   │   ├── DaemonManager.h/cpp # Daemon lifecycle management (auto-start, crash recovery)
│   │   ├── DaemonClient.h/cpp # Unix socket client (IPC)
│   │   ├── ProcessTab.h/cpp  # Process list with context menu
│   │   ├── AlertTab.h/cpp    # Alert history with context menu
│   │   ├── WhitelistTab.h/cpp # Whitelist management
│   │   ├── SettingsTab.h/cpp # Configuration UI
│   │   └── TrayIcon.h/cpp    # System tray with status colors
│   └── CMakeLists.txt
└── docs/
    ├── plans/                 # Design documents
    └── ARCHITECTURE.md        # This file
```

## Daemon (Rust)

### Core Components

#### 1. Process Collector (`collector/linux.rs`)

Reads `/proc` filesystem to collect process information:

```rust
pub struct ProcessInfo {
    pub pid: u32,
    pub name: String,
    pub cmdline: String,
    pub cpu_percent: f64,      // Calculated from utime+stime delta
    pub memory_mb: f64,        // From /proc/[pid]/statm
    pub runtime_seconds: u64,  // From process start time
    pub state: ProcessState,   // Running, Sleeping, Stopped, Zombie
}
```

**CPU Calculation**: Uses sampling - stores previous `utime+stime` ticks, calculates delta over time interval.

#### 2. Anomaly Detector (`detector.rs`)

Implements detection rules:

| Detection | Trigger | Default |
|-----------|---------|---------|
| CPU High | CPU > threshold for duration | 90% for 60s |
| Hang | State 'D' (uninterruptible) for duration | 30s |
| Memory Leak | Memory growth > threshold in window | 500MB in 5min |

```rust
pub trait Detector {
    fn check(&mut self, process: &ProcessInfo) -> Option<Alert>;
    fn cleanup(&mut self, active_pids: &[u32]);
}
```

**Whitelist**: Detector maintains `HashSet<String>` of whitelisted process names, skips detection for matches.

#### 3. Database (`db.rs`)

SQLite schema:

```sql
-- Alerts table
CREATE TABLE alerts (
    id INTEGER PRIMARY KEY,
    pid INTEGER,
    name TEXT,
    cmdline TEXT,
    reason TEXT,      -- cpu_high, hang, memory_leak, timeout
    severity TEXT,    -- warning, critical
    timestamp INTEGER
);

-- Whitelist table
CREATE TABLE whitelist (
    id INTEGER PRIMARY KEY,
    pattern TEXT UNIQUE,
    match_type TEXT,  -- name, cmdline, regex
    reason TEXT
);
```

#### 4. Socket Server (`socket.rs`)

Unix domain socket at `/run/user/<uid>/runaway-guard.sock`:

- Async using tokio
- Broadcasts status/alerts to all connected clients
- Per-client request/response handling

#### 5. Protocol (`protocol.rs`)

JSON-line protocol (one JSON object per line):

**Requests** (GUI → Daemon):
```json
{"cmd": "ping"}
{"cmd": "list_processes"}
{"cmd": "get_alerts", "params": {"limit": 50}}
{"cmd": "kill_process", "params": {"pid": 1234, "signal": "SIGTERM"}}
{"cmd": "list_whitelist"}
{"cmd": "add_whitelist", "params": {"pattern": "firefox", "match_type": "name"}}
{"cmd": "remove_whitelist", "params": {"id": 1}}
{"cmd": "update_config", "params": {...}}
```

**Responses** (Daemon → GUI):
```json
{"type": "pong"}
{"type": "response", "id": null, "data": [...]}
{"type": "alert", "data": {"pid": 1234, "name": "proc", "reason": "cpu_high", "severity": "critical"}}
{"type": "status", "data": {"monitored_count": 500, "alert_count": 10}}
```

### Monitoring Loop

```
┌─────────────────────────────────────────────────────────────┐
│                    Monitoring Loop                          │
├─────────────────────────────────────────────────────────────┤
│  1. Collect all processes from /proc                        │
│  2. For each process:                                       │
│     - Check against whitelist (skip if matched)             │
│     - Run anomaly detection                                 │
│     - If alert: save to DB, notify, broadcast               │
│  3. Cleanup stale history (dead PIDs)                       │
│  4. Adaptive interval:                                      │
│     - Normal: 10 seconds                                    │
│     - Alert mode: 2 seconds                                 │
│  5. Broadcast status update                                 │
└─────────────────────────────────────────────────────────────┘
```

### Configuration (`~/.config/runaway-guard/config.toml`)

```toml
[detection.cpu_high]
enabled = true
threshold_percent = 90
duration_seconds = 60

[detection.hang]
enabled = true
duration_seconds = 30

[detection.memory_leak]
enabled = true
growth_threshold_mb = 500
window_minutes = 5

[general]
sample_interval_normal = 10
sample_interval_alert = 2
notification_method = "both"  # system, popup, both
```

## GUI (Qt6 C++)

### Components

#### MainWindow
- Tab widget with 4 tabs: Monitor, Alerts, Whitelist, Settings
- Status bar: connection status, process count, alert count
- Manages DaemonManager for daemon lifecycle

#### DaemonManager
- **Automatic daemon startup**: Starts daemon if not running on GUI launch
- **Lifecycle management**: Optionally stops daemon when GUI exits (configurable)
- **Crash recovery**: Detects daemon crashes and auto-restarts
- **Crash loop detection**: Stops auto-restart after 3 crashes in 60 seconds
- **Binary search**: Finds daemon in env var, bundled paths, system paths
- States: Unknown → Starting → Running → Stopped → Failed
- Owns DaemonClient internally, manages reconnection externally

```
Startup Flow:
  GUI Launch → DaemonManager::initialize()
       │
       ├── Socket exists? → Connect via DaemonClient
       │
       └── No socket → Check if daemon running
              │
              ├── Running → Poll for socket
              │
              └── Not running → QProcess::start(runaway-daemon)
                     │
                     └── Poll for socket → Connect
```

#### DaemonClient
- QLocalSocket connection to daemon
- Auto-reconnection (max 10 attempts, 3s interval)
- Emits signals for responses: `processListReceived`, `alertListReceived`, `whitelistReceived`
- Convenience methods: `requestProcessList()`, `requestKillProcess()`, etc.

#### ProcessTab
- QTableWidget showing all monitored processes
- Columns: PID, Name, CPU%, Memory, Runtime, State
- Sorting enabled
- Right-click context menu:
  - Terminate (SIGTERM)
  - Kill (SIGKILL)
  - Stop (SIGSTOP)
  - Continue (SIGCONT)
  - Add to Whitelist

#### AlertTab
- QTableWidget showing alert history
- Columns: Time, PID, Name, Reason, Severity
- Timestamps formatted as readable dates
- Right-click context menu:
  - Add to Whitelist
  - Terminate Process
  - Dismiss Alert

#### WhitelistTab
- Input: pattern field + match type combo (Name/Command/Regex)
- Add/Remove buttons
- Table displays existing entries

#### SettingsTab
- **GUI Behavior group**: Local settings stored in QSettings
  - "Stop daemon when GUI exits" checkbox (default: true)
- QGroupBox sections for each detection type
- QCheckBox for enable/disable
- QSpinBox for thresholds and durations
- Apply/Reset buttons
- **TODO**: Sync with daemon via `update_config` command

#### TrayIcon
- System tray icon with colored status:
  - Green: Normal (no alerts)
  - Yellow: Warning (active alerts)
  - Red: Critical (disconnected or critical alert)
- Menu: Show/Hide, Quit

### Signal Flow

```
ProcessTab::killProcessRequested ──────► DaemonClient::requestKillProcess
ProcessTab::addWhitelistRequested ─────► DaemonClient::requestAddWhitelist
AlertTab::killProcessRequested ────────► DaemonClient::requestKillProcess
AlertTab::addWhitelistRequested ───────► DaemonClient::requestAddWhitelist
WhitelistTab::addWhitelistRequested ───► DaemonClient::requestAddWhitelist
WhitelistTab::removeWhitelistRequested ► DaemonClient::requestRemoveWhitelist
DaemonClient::processListReceived ─────► ProcessTab::updateProcessList
DaemonClient::alertListReceived ───────► AlertTab::updateAlertList
DaemonClient::whitelistReceived ───────► WhitelistTab::updateWhitelistDisplay
```

## Building

### Daemon (Rust 1.89+)

```bash
cd daemon
cargo build --release
# Binary: target/release/runaway-daemon
```

### GUI (Qt 6.x)

```bash
cd gui
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x/gcc_64
cmake --build build
# Binary: build/runaway-gui
```

## Running

```bash
# Start daemon (background)
./daemon/target/release/runaway-daemon &

# Start GUI
./gui/build/runaway-gui
```

## Future Work

### High Priority
1. **SettingsTab sync**: Implement `update_config` handler, load/save config from UI
2. **System tray menu**: Add quick actions (pause monitoring, clear alerts)
3. **Packaging**: Create .deb and AppImage packages
4. **Systemd service**: Auto-start daemon on login

### Medium Priority
5. **macOS support**: Implement `DarwinProcessCollector` using sysctl/libproc
6. **Regex whitelist**: Full regex support for pattern matching
7. **Alert actions**: Auto-kill, auto-nice, email notifications
8. **Per-process config**: Different thresholds per process name

### Low Priority
9. **ML learner**: Learn normal behavior patterns, reduce false positives
10. **Resource graphs**: Historical CPU/memory charts in GUI
11. **Process tree**: Show parent-child relationships
12. **Remote monitoring**: Monitor processes on remote machines

## Dependencies

### Daemon
- tokio (async runtime)
- serde/serde_json (serialization)
- rusqlite (SQLite)
- notify-rust (desktop notifications)
- tracing (logging)
- directories (XDG paths)
- async-trait

### GUI
- Qt6 (Widgets, Network)
- CMake 3.16+

## Testing

```bash
# Daemon unit tests
cd daemon && cargo test

# Manual IPC testing
echo '{"cmd":"ping"}' | nc -U /run/user/$(id -u)/runaway-guard.sock
echo '{"cmd":"list_processes"}' | nc -U /run/user/$(id -u)/runaway-guard.sock
echo '{"cmd":"list_whitelist"}' | nc -U /run/user/$(id -u)/runaway-guard.sock
```
