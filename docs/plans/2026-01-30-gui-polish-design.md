# GUI Polish Design

Date: 2026-01-30

## Overview

Comprehensive GUI refinement for RunawayGuard, focusing on information readability, interaction experience, and visual polish while maintaining native Qt system style.

## 1. Table Data Display

### 1.1 Value Formatting

**Runtime Smart Formatting:**
- < 60s: `45s`
- < 1 hour: `45m 30s`
- < 1 day: `2h 15m`
- >= 1 day: `3d 5h`

**CPU/Memory Display Precision:**
- CPU: 1 decimal place, e.g., `45.3%`
- Memory: < 1GB shows MB e.g., `512.0 MB`, >= 1GB shows GB e.g., `2.3 GB`

### 1.2 Threshold Coloring

| Metric | Normal | Warning (Orange) | Danger (Red) |
|--------|--------|------------------|--------------|
| CPU | < 80% | 80-90% | > 90% |
| Memory | < 1GB | 1-4GB | > 4GB |
| State | R/S | - | D/Z (zombie/uninterruptible) |

Coloring: Light cell background (maintain readability), darker text color.

### 1.3 Tooltip Enhancement

- **Name column**: Hover shows full command line (cmdline)
- **Numeric columns**: Hover shows precise values, e.g., "CPU: 45.328%, Memory: 1234.5 MB, Runtime: 86423 seconds"

## 2. Table Interaction Enhancement

### 2.1 Search/Filter

Add search bar at top of ProcessTab and AlertTab:
- Real-time filtering, instant search on input
- Search scope: process name, PID, command line
- Shortcut `Ctrl+F` focuses search box, `Esc` clears and hides

Layout: `[Search box] [Clear button]` right-aligned, minimal space usage

### 2.2 Column Control

- **Draggable column width**: Auto-save to QSettings after user adjustment
- **Column show/hide**: Right-click table header for menu with checkboxes
- Default hidden: none (all columns visible by default)
- Settings persisted to `~/.config/runaway-guard/gui.ini`

### 2.3 Convenience Operations

- **Double-click cell**: Copy content to clipboard, status bar shows "Copied: xxx"
- **Row selection highlight**: Use system highlight color for visibility
- **Alternating row background**: Enable `alternatingRowColors` for readability

### 2.4 Sort Memory

Current sort column and order saved to QSettings, restored on next launch.

## 3. System Tray Enhancement

### 3.1 Tray Menu Structure

```
+-----------------------------+
| RunawayGuard                |  <- Title (bold)
| --------------------------- |
| Processes: 342 | Alerts: 2  |  <- Status info (gray, non-clickable)
| --------------------------- |
| Pause Monitoring            |  <- Toggle, becomes "Resume Monitoring" when paused
| Clear All Alerts            |  <- Only enabled when alerts exist
| --------------------------- |
| Show Main Window            |
| Quit                        |
+-----------------------------+
```

### 3.2 Pause Monitoring Feature

- Requires daemon support for `pause_monitoring` / `resume_monitoring` commands
- When paused, tray icon turns gray (new `tray_paused.png`)
- When paused, tooltip shows "RunawayGuard - Monitoring paused"

### 3.3 Real-time Status Update

- Refresh status info when tray menu opens
- Use `aboutToShow` signal to trigger update

### 3.4 Icon Status Summary

| Status | Icon Color | Tooltip |
|--------|-----------|---------|
| Normal | Green | "All systems normal" |
| Warning | Yellow | "Alerts detected (N)" |
| Critical | Red | "Disconnected" |
| Paused | Gray | "Monitoring paused" |

## 4. Operation Feedback

### 4.1 Status Bar Messages

Show temporary message in status bar after operation, restore default after 3 seconds:

| Operation | Message Example |
|-----------|----------------|
| Terminate process | "Sent SIGTERM to firefox (PID: 1234)" |
| Add whitelist | "Added to whitelist: firefox" |
| Remove whitelist | "Removed from whitelist: chrome" |
| Copy content | "Copied: /usr/bin/firefox" |
| Apply settings | "Settings saved" |

Implementation: Use `QStatusBar::showMessage(msg, 3000)`

### 4.2 SIGKILL Confirmation Dialog

Only show confirmation for SIGKILL:

```
+-- Confirm Force Kill -------------------+
|                                         |
|  Are you sure you want to force kill?   |
|                                         |
|  Process: firefox (PID: 1234)           |
|                                         |
|  Force killing may cause data loss.     |
|                                         |
|              [Cancel]  [Force Kill]     |
+-----------------------------------------+
```

### 4.3 Error Handling

Show red error message in status bar when daemon returns error:
- "Operation failed: Process not found"
- "Operation failed: Permission denied"

## 5. SettingsTab Completion

### 5.1 Config Sync Flow

```
On startup:
  GUI connected -> send get_config request -> daemon returns config -> populate form

On apply:
  User clicks Apply -> collect form data -> send update_config -> daemon saves -> status bar notification
```

### 5.2 Protocol Extension

New daemon commands:

```json
// Request
{"cmd": "get_config"}
{"cmd": "update_config", "params": {"cpu_high": {"enabled": true, "threshold": 90, ...}}}

// Response
{"type": "config", "data": {"cpu_high": {...}, "hang": {...}, "memory_leak": {...}, "general": {...}}}
```

### 5.3 UI State Management

- **Loading**: Controls disabled before connection, show "Loading config..."
- **Unmodified**: Apply button disabled
- **Modified**: Apply button enabled, title shows "*" marker
- **Save success**: Return to unmodified state

### 5.4 Layout Optimization

- Add appropriate padding to GroupBoxes (10px)
- Right-align form labels with input fields
- Uniform SpinBox width (80px)

## 6. Visual Detail Optimization

### 6.1 Unified Spacing

| Location | Current | Optimized |
|----------|---------|-----------|
| Tab content margin | 0px | 8px |
| GroupBox padding | default | 12px |
| Button spacing | default | 8px |
| Table row height | default | +4px increase |

### 6.2 Alternating Row Colors

Enable `setAlternatingRowColors(true)` for ProcessTab / AlertTab / WhitelistTab, use system default alternating colors.

### 6.3 Window Details

- **Minimum size**: Set reasonable minimumSize (600x400)
- **Remember window position/size**: Save to QSettings, restore on next launch
- **Tab icons**: Add small icons for each tab
  - Monitor: process icon
  - Alerts: warning icon
  - Whitelist: checkmark icon
  - Settings: gear icon

### 6.4 Status Bar Optimization

Current: `[Status text] ... [Process count] [Alert count]`

Optimized: Add separators and icon hints
`[Connected] ... [Processes: 342] [Alerts: 2]`

## 7. Implementation Summary

### 7.1 Files to Modify

| File | Changes |
|------|---------|
| `ProcessTab.cpp/h` | Formatting, coloring, search, column control, tooltip, double-click |
| `AlertTab.cpp/h` | Same interaction enhancements as ProcessTab |
| `WhitelistTab.cpp/h` | Alternating row colors, layout optimization |
| `SettingsTab.cpp/h` | Config sync, state management, layout optimization |
| `TrayIcon.cpp/h` | Menu enhancement, pause feature, status icons |
| `MainWindow.cpp/h` | Status bar optimization, window memory, tab icons, operation feedback |
| `DaemonClient.cpp/h` | New protocol commands |
| `daemon/src/main.rs` | get_config, update_config, pause/resume handlers |
| `daemon/src/protocol.rs` | New message types |

### 7.2 New Files

- `gui/resources/tray_paused.png` - Gray tray icon
- `gui/resources/tab_monitor.png` - Monitor tab icon
- `gui/resources/tab_alerts.png` - Alerts tab icon
- `gui/resources/tab_whitelist.png` - Whitelist tab icon
- `gui/resources/tab_settings.png` - Settings tab icon

### 7.3 Persisted Settings

`~/.config/runaway-guard/gui.ini`:
- Window position/size
- Column width settings
- Sort settings
- Column show/hide preferences
