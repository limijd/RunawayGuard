# Daemon Lifecycle Binding Design

## Overview

GUI 启动时自动启动 daemon，GUI 退出时自动停止 daemon（默认行为），可通过配置关闭。

## Configuration

- **Key**: `GUI/manageDaemonLifecycle`
- **Type**: `bool`
- **Default**: `true`
- **Storage**: QSettings (`~/.config/RunawayGuard/RunawayGuard.conf`)

## Behavior

| Config Value | GUI Start | GUI Exit |
|--------------|-----------|----------|
| `true` (default) | Start daemon | Stop daemon |
| `false` | Start daemon | Daemon keeps running |

Note: GUI always starts daemon if not running, regardless of config. The difference is only in exit behavior.

## Code Changes

### 1. DaemonManager (`gui/src/DaemonManager.h/.cpp`)

New members and methods:
```cpp
// Member
bool m_manageDaemonLifecycle = true;

// Methods
void setManageDaemonLifecycle(bool manage);
bool manageDaemonLifecycle() const;
void shutdown();  // Called on GUI exit
```

Shutdown implementation:
- If `m_manageDaemonLifecycle` is false, do nothing
- If daemon process exists and running, send SIGTERM via `terminate()`
- Wait up to 3 seconds for graceful exit
- If still running, send SIGKILL via `kill()`

### 2. MainWindow (`gui/src/MainWindow.cpp`)

- Override `closeEvent()` to call `m_daemonManager->shutdown()`
- On startup, read config from QSettings and pass to DaemonManager

### 3. SettingsTab (`gui/src/SettingsTab.h/.cpp`)

Add UI elements:
- New group box: "GUI Behavior" (placed at top)
- Checkbox: "Stop daemon when GUI exits"
- Default: checked

Save/load via QSettings.

## Files Modified

| File | Change |
|------|--------|
| `gui/src/DaemonManager.h` | Add member and methods |
| `gui/src/DaemonManager.cpp` | Implement shutdown logic |
| `gui/src/MainWindow.h` | Declare closeEvent override |
| `gui/src/MainWindow.cpp` | Implement closeEvent, load config |
| `gui/src/SettingsTab.h` | Add checkbox member |
| `gui/src/SettingsTab.cpp` | Add GUI Behavior group |
