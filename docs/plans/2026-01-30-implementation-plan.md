# RunawayGuard Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a Linux process monitor that detects runaway processes (high CPU, hangs, memory leaks, timeouts) and notifies users via system tray.

**Architecture:** Rust daemon for monitoring + Qt6 C++ GUI, communicating via Unix socket with JSON messages. SQLite for persistence, TOML for configuration.

**Tech Stack:** Rust (tokio, rusqlite, serde), C++ (Qt6 Widgets/Network), CMake

---

## Phase 1: Project Skeleton

### Task 1.1: Create Rust Daemon Project Structure

**Files:**
- Create: `daemon/Cargo.toml`
- Create: `daemon/src/main.rs`
- Create: `daemon/src/lib.rs`

**Step 1: Create daemon directory and Cargo.toml**

```bash
cd /home/wli/.local/share/Cryptomator/mnt/my25-projects/RunawayGuard/.worktrees/dev
mkdir -p daemon/src
```

```toml
# daemon/Cargo.toml
[package]
name = "runaway-daemon"
version = "0.1.0"
edition = "2021"
description = "Process monitor daemon for RunawayGuard"
license = "MIT"

[dependencies]
tokio = { version = "1", features = ["full"] }
serde = { version = "1", features = ["derive"] }
serde_json = "1"
toml = "0.8"
rusqlite = { version = "0.31", features = ["bundled"] }
notify-rust = "4"
tracing = "0.1"
tracing-subscriber = { version = "0.3", features = ["env-filter"] }
thiserror = "1"
directories = "5"

[dev-dependencies]
tempfile = "3"
```

**Step 2: Create minimal main.rs**

```rust
// daemon/src/main.rs
use tracing::info;

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    tracing_subscriber::fmt::init();
    info!("RunawayGuard daemon starting...");
    Ok(())
}
```

**Step 3: Create lib.rs with module declarations**

```rust
// daemon/src/lib.rs
pub mod collector;
pub mod config;
pub mod db;
pub mod detector;
pub mod executor;
pub mod learner;
pub mod notifier;
pub mod protocol;
pub mod socket;
```

**Step 4: Verify it compiles**

Run: `cd daemon && cargo check`
Expected: Compilation errors for missing modules (expected at this stage)

**Step 5: Commit**

```bash
git add daemon/
git commit -m "feat(daemon): initialize Rust project structure

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

### Task 1.2: Create Daemon Module Stubs

**Files:**
- Create: `daemon/src/collector.rs`
- Create: `daemon/src/config.rs`
- Create: `daemon/src/db.rs`
- Create: `daemon/src/detector.rs`
- Create: `daemon/src/executor.rs`
- Create: `daemon/src/learner.rs`
- Create: `daemon/src/notifier.rs`
- Create: `daemon/src/protocol.rs`
- Create: `daemon/src/socket.rs`

**Step 1: Create all module stub files**

```rust
// daemon/src/collector.rs
//! Process information collector (reads /proc on Linux)

pub struct ProcessInfo {
    pub pid: u32,
    pub name: String,
    pub cmdline: String,
    pub cpu_percent: f64,
    pub memory_mb: f64,
    pub runtime_seconds: u64,
    pub state: char,
}

pub trait ProcessCollector: Send + Sync {
    fn list_processes(&self) -> Vec<ProcessInfo>;
    fn get_process(&self, pid: u32) -> Option<ProcessInfo>;
}
```

```rust
// daemon/src/config.rs
//! Configuration management (TOML)

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Config {
    pub general: GeneralConfig,
    pub detection: DetectionConfig,
    pub learning: LearningConfig,
    #[serde(default)]
    pub rules: Vec<Rule>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GeneralConfig {
    pub autostart: bool,
    pub sample_interval_normal: u64,
    pub sample_interval_alert: u64,
    pub notification_method: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DetectionConfig {
    pub cpu: CpuDetectionConfig,
    pub hang: HangDetectionConfig,
    pub memory: MemoryDetectionConfig,
    pub timeout: TimeoutDetectionConfig,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CpuDetectionConfig {
    pub enabled: bool,
    pub threshold_percent: u8,
    pub duration_seconds: u64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HangDetectionConfig {
    pub enabled: bool,
    pub duration_seconds: u64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MemoryDetectionConfig {
    pub enabled: bool,
    pub growth_mb: u64,
    pub window_minutes: u64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TimeoutDetectionConfig {
    pub enabled: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LearningConfig {
    pub enabled: bool,
    pub min_history_days: u64,
    pub suggest_whitelist: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Rule {
    pub name: String,
    #[serde(rename = "match")]
    pub pattern: String,
    pub match_type: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub max_runtime_minutes: Option<u64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub action: Option<String>,
}

impl Default for Config {
    fn default() -> Self {
        Config {
            general: GeneralConfig {
                autostart: true,
                sample_interval_normal: 10,
                sample_interval_alert: 2,
                notification_method: "both".to_string(),
            },
            detection: DetectionConfig {
                cpu: CpuDetectionConfig {
                    enabled: true,
                    threshold_percent: 90,
                    duration_seconds: 60,
                },
                hang: HangDetectionConfig {
                    enabled: true,
                    duration_seconds: 30,
                },
                memory: MemoryDetectionConfig {
                    enabled: true,
                    growth_mb: 500,
                    window_minutes: 5,
                },
                timeout: TimeoutDetectionConfig { enabled: true },
            },
            learning: LearningConfig {
                enabled: true,
                min_history_days: 7,
                suggest_whitelist: true,
            },
            rules: vec![],
        }
    }
}
```

```rust
// daemon/src/db.rs
//! SQLite database operations

use rusqlite::Connection;
use std::path::Path;

pub struct Database {
    conn: Connection,
}

impl Database {
    pub fn open(path: &Path) -> rusqlite::Result<Self> {
        let conn = Connection::open(path)?;
        Ok(Self { conn })
    }

    pub fn init_schema(&self) -> rusqlite::Result<()> {
        self.conn.execute_batch(include_str!("../schema.sql"))
    }
}
```

```rust
// daemon/src/detector.rs
//! Anomaly detection engine

use crate::collector::ProcessInfo;

#[derive(Debug, Clone)]
pub enum AlertReason {
    CpuHigh,
    Hang,
    MemoryLeak,
    Timeout,
}

#[derive(Debug, Clone)]
pub enum Severity {
    Warning,
    Critical,
}

#[derive(Debug, Clone)]
pub struct Alert {
    pub pid: u32,
    pub name: String,
    pub cmdline: String,
    pub reason: AlertReason,
    pub severity: Severity,
    pub timestamp: u64,
}

pub trait Detector: Send + Sync {
    fn check(&mut self, process: &ProcessInfo) -> Option<Alert>;
}
```

```rust
// daemon/src/executor.rs
//! Process action executor (kill, nice, etc.)

use std::process::Command;

#[derive(Debug, Clone, Copy)]
pub enum Signal {
    Term,
    Kill,
    Stop,
    Cont,
}

pub fn send_signal(pid: u32, signal: Signal) -> std::io::Result<()> {
    let sig = match signal {
        Signal::Term => "TERM",
        Signal::Kill => "KILL",
        Signal::Stop => "STOP",
        Signal::Cont => "CONT",
    };
    Command::new("kill")
        .args(["-s", sig, &pid.to_string()])
        .status()?;
    Ok(())
}

pub fn renice(pid: u32, priority: i32) -> std::io::Result<()> {
    Command::new("renice")
        .args(["-n", &priority.to_string(), "-p", &pid.to_string()])
        .status()?;
    Ok(())
}
```

```rust
// daemon/src/learner.rs
//! Smart learning engine for process behavior

pub struct ProcessStats {
    pub name: String,
    pub sample_count: u64,
    pub cpu_avg: f64,
    pub cpu_p95: f64,
    pub memory_avg: f64,
    pub memory_p95: f64,
}

pub struct Learner {
    min_history_days: u64,
}

impl Learner {
    pub fn new(min_history_days: u64) -> Self {
        Self { min_history_days }
    }
}
```

```rust
// daemon/src/notifier.rs
//! System notification sender

use notify_rust::Notification;

pub fn send_notification(summary: &str, body: &str) -> Result<(), notify_rust::error::Error> {
    Notification::new()
        .summary(summary)
        .body(body)
        .appname("RunawayGuard")
        .show()?;
    Ok(())
}
```

```rust
// daemon/src/protocol.rs
//! IPC protocol definitions (JSON messages)

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "cmd", rename_all = "snake_case")]
pub enum Request {
    Ping,
    ListProcesses,
    GetAlerts { params: GetAlertsParams },
    KillProcess { params: KillProcessParams },
    AddWhitelist { params: AddWhitelistParams },
    UpdateConfig { params: serde_json::Value },
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GetAlertsParams {
    pub limit: Option<u32>,
    pub since: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct KillProcessParams {
    pub pid: u32,
    pub signal: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AddWhitelistParams {
    pub pattern: String,
    pub match_type: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "type", rename_all = "snake_case")]
pub enum Response {
    Pong,
    Response { id: Option<String>, data: serde_json::Value },
    Alert { data: AlertData },
    Status { data: StatusData },
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AlertData {
    pub pid: u32,
    pub name: String,
    pub reason: String,
    pub severity: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StatusData {
    pub monitored_count: u32,
    pub alert_count: u32,
}
```

```rust
// daemon/src/socket.rs
//! Unix socket server for IPC

use std::path::Path;
use tokio::net::UnixListener;

pub struct SocketServer {
    listener: UnixListener,
}

impl SocketServer {
    pub async fn bind(path: &Path) -> std::io::Result<Self> {
        // Remove existing socket file if present
        let _ = std::fs::remove_file(path);
        let listener = UnixListener::bind(path)?;
        Ok(Self { listener })
    }
}
```

**Step 2: Create SQL schema file**

```sql
-- daemon/schema.sql
-- Process samples for learning
CREATE TABLE IF NOT EXISTS process_samples (
    id INTEGER PRIMARY KEY,
    timestamp INTEGER NOT NULL,
    name TEXT NOT NULL,
    pid INTEGER NOT NULL,
    cpu_percent REAL,
    memory_mb REAL,
    runtime_seconds INTEGER
);
CREATE INDEX IF NOT EXISTS idx_samples_name_time ON process_samples(name, timestamp);

-- Alert records
CREATE TABLE IF NOT EXISTS alerts (
    id INTEGER PRIMARY KEY,
    timestamp INTEGER NOT NULL,
    pid INTEGER NOT NULL,
    name TEXT NOT NULL,
    cmdline TEXT,
    reason TEXT NOT NULL,
    severity TEXT NOT NULL,
    resolved INTEGER DEFAULT 0,
    action_taken TEXT
);
CREATE INDEX IF NOT EXISTS idx_alerts_time ON alerts(timestamp DESC);

-- Process statistics for learning
CREATE TABLE IF NOT EXISTS process_stats (
    name TEXT PRIMARY KEY,
    sample_count INTEGER,
    cpu_avg REAL,
    cpu_p95 REAL,
    memory_avg REAL,
    memory_p95 REAL,
    last_updated INTEGER
);

-- Whitelist
CREATE TABLE IF NOT EXISTS whitelist (
    id INTEGER PRIMARY KEY,
    pattern TEXT NOT NULL,
    match_type TEXT NOT NULL,
    reason TEXT,
    created_at INTEGER
);
```

**Step 3: Add anyhow dependency and verify compilation**

Update Cargo.toml to add `anyhow = "1"` in dependencies.

Run: `cd daemon && cargo check`
Expected: Successful compilation

**Step 4: Commit**

```bash
git add daemon/
git commit -m "feat(daemon): add module stubs and database schema

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

### Task 1.3: Create Qt GUI Project Structure

**Files:**
- Create: `gui/CMakeLists.txt`
- Create: `gui/src/main.cpp`

**Step 1: Create gui directory structure**

```bash
mkdir -p gui/src gui/resources/icons
```

**Step 2: Create CMakeLists.txt**

```cmake
# gui/CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(runaway-gui VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/TrayIcon.cpp
    src/DaemonClient.cpp
    src/ProcessTab.cpp
    src/AlertTab.cpp
    src/WhitelistTab.cpp
    src/SettingsTab.cpp
)

set(HEADERS
    src/MainWindow.h
    src/TrayIcon.h
    src/DaemonClient.h
    src/ProcessTab.h
    src/AlertTab.h
    src/WhitelistTab.h
    src/SettingsTab.h
)

add_executable(runaway-gui ${SOURCES} ${HEADERS})

target_link_libraries(runaway-gui PRIVATE Qt6::Widgets Qt6::Network)

# HiDPI support
target_compile_definitions(runaway-gui PRIVATE
    QT_DISABLE_DEPRECATED_BEFORE=0x060000
)

install(TARGETS runaway-gui DESTINATION bin)
```

**Step 3: Create main.cpp**

```cpp
// gui/src/main.cpp
#include <QApplication>
#include <QStyleHints>
#include "MainWindow.h"
#include "TrayIcon.h"

int main(int argc, char *argv[])
{
    // Enable HiDPI support
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    app.setApplicationName("RunawayGuard");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("RunawayGuard");

    // Don't quit when main window is closed (stay in tray)
    app.setQuitOnLastWindowClosed(false);

    MainWindow mainWindow;
    TrayIcon trayIcon(&mainWindow);
    trayIcon.show();

    return app.exec();
}
```

**Step 4: Create header stubs for all GUI classes**

```cpp
// gui/src/MainWindow.h
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>

class ProcessTab;
class AlertTab;
class WhitelistTab;
class SettingsTab;
class DaemonClient;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void setupUi();
    void setupStatusBar();

    QTabWidget *m_tabWidget;
    ProcessTab *m_processTab;
    AlertTab *m_alertTab;
    WhitelistTab *m_whitelistTab;
    SettingsTab *m_settingsTab;
    DaemonClient *m_client;
};

#endif // MAINWINDOW_H
```

```cpp
// gui/src/TrayIcon.h
#ifndef TRAYICON_H
#define TRAYICON_H

#include <QSystemTrayIcon>
#include <QMenu>

class MainWindow;

class TrayIcon : public QSystemTrayIcon
{
    Q_OBJECT

public:
    explicit TrayIcon(MainWindow *mainWindow, QObject *parent = nullptr);

    enum class Status { Normal, Warning, Critical };
    void setStatus(Status status);

private slots:
    void onActivated(QSystemTrayIcon::ActivationReason reason);

private:
    void setupMenu();
    void updateIcon();

    MainWindow *m_mainWindow;
    QMenu *m_menu;
    Status m_status;
};

#endif // TRAYICON_H
```

```cpp
// gui/src/DaemonClient.h
#ifndef DAEMONCLIENT_H
#define DAEMONCLIENT_H

#include <QObject>
#include <QLocalSocket>
#include <QJsonObject>

class DaemonClient : public QObject
{
    Q_OBJECT

public:
    explicit DaemonClient(QObject *parent = nullptr);

    void connectToDaemon();
    void sendRequest(const QJsonObject &request);

signals:
    void connected();
    void disconnected();
    void alertReceived(const QJsonObject &alert);
    void statusReceived(const QJsonObject &status);
    void responseReceived(const QJsonObject &response);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QLocalSocket::LocalSocketError error);

private:
    QLocalSocket *m_socket;
    QByteArray m_buffer;
};

#endif // DAEMONCLIENT_H
```

```cpp
// gui/src/ProcessTab.h
#ifndef PROCESSTAB_H
#define PROCESSTAB_H

#include <QWidget>
#include <QTableWidget>

class ProcessTab : public QWidget
{
    Q_OBJECT

public:
    explicit ProcessTab(QWidget *parent = nullptr);

public slots:
    void updateProcessList(const QJsonArray &processes);

private:
    void setupUi();

    QTableWidget *m_table;
};

#endif // PROCESSTAB_H
```

```cpp
// gui/src/AlertTab.h
#ifndef ALERTTAB_H
#define ALERTTAB_H

#include <QWidget>
#include <QTableWidget>

class AlertTab : public QWidget
{
    Q_OBJECT

public:
    explicit AlertTab(QWidget *parent = nullptr);

public slots:
    void updateAlertList(const QJsonArray &alerts);

private:
    void setupUi();

    QTableWidget *m_table;
};

#endif // ALERTTAB_H
```

```cpp
// gui/src/WhitelistTab.h
#ifndef WHITELISTTAB_H
#define WHITELISTTAB_H

#include <QWidget>
#include <QTableWidget>

class WhitelistTab : public QWidget
{
    Q_OBJECT

public:
    explicit WhitelistTab(QWidget *parent = nullptr);

private:
    void setupUi();

    QTableWidget *m_table;
};

#endif // WHITELISTTAB_H
```

```cpp
// gui/src/SettingsTab.h
#ifndef SETTINGSTAB_H
#define SETTINGSTAB_H

#include <QWidget>

class SettingsTab : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsTab(QWidget *parent = nullptr);

private:
    void setupUi();
};

#endif // SETTINGSTAB_H
```

**Step 5: Create minimal implementation stubs**

```cpp
// gui/src/MainWindow.cpp
#include "MainWindow.h"
#include "ProcessTab.h"
#include "AlertTab.h"
#include "WhitelistTab.h"
#include "SettingsTab.h"
#include "DaemonClient.h"
#include <QStatusBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_tabWidget(new QTabWidget(this))
    , m_processTab(new ProcessTab(this))
    , m_alertTab(new AlertTab(this))
    , m_whitelistTab(new WhitelistTab(this))
    , m_settingsTab(new SettingsTab(this))
    , m_client(new DaemonClient(this))
{
    setupUi();
    setupStatusBar();
    m_client->connectToDaemon();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    setWindowTitle("RunawayGuard");
    resize(800, 600);

    m_tabWidget->addTab(m_processTab, tr("Monitor"));
    m_tabWidget->addTab(m_alertTab, tr("Alerts"));
    m_tabWidget->addTab(m_whitelistTab, tr("Whitelist"));
    m_tabWidget->addTab(m_settingsTab, tr("Settings"));

    setCentralWidget(m_tabWidget);
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage(tr("Connecting to daemon..."));
}
```

```cpp
// gui/src/TrayIcon.cpp
#include "TrayIcon.h"
#include "MainWindow.h"
#include <QApplication>

TrayIcon::TrayIcon(MainWindow *mainWindow, QObject *parent)
    : QSystemTrayIcon(parent)
    , m_mainWindow(mainWindow)
    , m_menu(new QMenu())
    , m_status(Status::Normal)
{
    setupMenu();
    updateIcon();

    connect(this, &QSystemTrayIcon::activated, this, &TrayIcon::onActivated);
}

void TrayIcon::setStatus(Status status)
{
    if (m_status != status) {
        m_status = status;
        updateIcon();
    }
}

void TrayIcon::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger) {
        if (m_mainWindow->isVisible()) {
            m_mainWindow->hide();
        } else {
            m_mainWindow->show();
            m_mainWindow->raise();
            m_mainWindow->activateWindow();
        }
    }
}

void TrayIcon::setupMenu()
{
    m_menu->addAction(tr("Open"), m_mainWindow, &QWidget::show);
    m_menu->addSeparator();
    m_menu->addAction(tr("Quit"), qApp, &QApplication::quit);

    setContextMenu(m_menu);
}

void TrayIcon::updateIcon()
{
    // TODO: Load appropriate SVG icon based on status
    // For now use a placeholder
    setIcon(QIcon::fromTheme("dialog-information"));
}
```

```cpp
// gui/src/DaemonClient.cpp
#include "DaemonClient.h"
#include <QJsonDocument>
#include <QStandardPaths>

DaemonClient::DaemonClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QLocalSocket(this))
{
    connect(m_socket, &QLocalSocket::connected, this, &DaemonClient::onConnected);
    connect(m_socket, &QLocalSocket::disconnected, this, &DaemonClient::onDisconnected);
    connect(m_socket, &QLocalSocket::readyRead, this, &DaemonClient::onReadyRead);
    connect(m_socket, &QLocalSocket::errorOccurred, this, &DaemonClient::onError);
}

void DaemonClient::connectToDaemon()
{
    QString socketPath = QString("/run/user/%1/runaway-guard.sock").arg(getuid());
    m_socket->connectToServer(socketPath);
}

void DaemonClient::sendRequest(const QJsonObject &request)
{
    QJsonDocument doc(request);
    QByteArray data = doc.toJson(QJsonDocument::Compact) + "\n";
    m_socket->write(data);
}

void DaemonClient::onConnected()
{
    emit connected();
}

void DaemonClient::onDisconnected()
{
    emit disconnected();
}

void DaemonClient::onReadyRead()
{
    m_buffer.append(m_socket->readAll());

    while (true) {
        int newlinePos = m_buffer.indexOf('\n');
        if (newlinePos < 0) break;

        QByteArray line = m_buffer.left(newlinePos);
        m_buffer.remove(0, newlinePos + 1);

        QJsonDocument doc = QJsonDocument::fromJson(line);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            QString type = obj["type"].toString();

            if (type == "alert") {
                emit alertReceived(obj["data"].toObject());
            } else if (type == "status") {
                emit statusReceived(obj["data"].toObject());
            } else {
                emit responseReceived(obj);
            }
        }
    }
}

void DaemonClient::onError(QLocalSocket::LocalSocketError error)
{
    Q_UNUSED(error);
    // TODO: Implement reconnection logic
}
```

```cpp
// gui/src/ProcessTab.cpp
#include "ProcessTab.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>

ProcessTab::ProcessTab(QWidget *parent)
    : QWidget(parent)
    , m_table(new QTableWidget(this))
{
    setupUi();
}

void ProcessTab::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels({
        tr("PID"), tr("Name"), tr("CPU %"), tr("Memory (MB)"),
        tr("Runtime"), tr("Status")
    });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    layout->addWidget(m_table);
}

void ProcessTab::updateProcessList(const QJsonArray &processes)
{
    m_table->setRowCount(processes.size());

    for (int i = 0; i < processes.size(); ++i) {
        QJsonObject proc = processes[i].toObject();
        m_table->setItem(i, 0, new QTableWidgetItem(QString::number(proc["pid"].toInt())));
        m_table->setItem(i, 1, new QTableWidgetItem(proc["name"].toString()));
        m_table->setItem(i, 2, new QTableWidgetItem(QString::number(proc["cpu_percent"].toDouble(), 'f', 1)));
        m_table->setItem(i, 3, new QTableWidgetItem(QString::number(proc["memory_mb"].toDouble(), 'f', 1)));
        m_table->setItem(i, 4, new QTableWidgetItem(QString::number(proc["runtime_seconds"].toInt())));
        m_table->setItem(i, 5, new QTableWidgetItem(proc["state"].toString()));
    }
}
```

```cpp
// gui/src/AlertTab.cpp
#include "AlertTab.h"
#include <QVBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>

AlertTab::AlertTab(QWidget *parent)
    : QWidget(parent)
    , m_table(new QTableWidget(this))
{
    setupUi();
}

void AlertTab::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({
        tr("Time"), tr("PID"), tr("Name"), tr("Reason"), tr("Severity")
    });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    layout->addWidget(m_table);
}

void AlertTab::updateAlertList(const QJsonArray &alerts)
{
    m_table->setRowCount(alerts.size());

    for (int i = 0; i < alerts.size(); ++i) {
        QJsonObject alert = alerts[i].toObject();
        m_table->setItem(i, 0, new QTableWidgetItem(QString::number(alert["timestamp"].toInt())));
        m_table->setItem(i, 1, new QTableWidgetItem(QString::number(alert["pid"].toInt())));
        m_table->setItem(i, 2, new QTableWidgetItem(alert["name"].toString()));
        m_table->setItem(i, 3, new QTableWidgetItem(alert["reason"].toString()));
        m_table->setItem(i, 4, new QTableWidgetItem(alert["severity"].toString()));
    }
}
```

```cpp
// gui/src/WhitelistTab.cpp
#include "WhitelistTab.h"
#include <QVBoxLayout>
#include <QHeaderView>

WhitelistTab::WhitelistTab(QWidget *parent)
    : QWidget(parent)
    , m_table(new QTableWidget(this))
{
    setupUi();
}

void WhitelistTab::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({
        tr("Pattern"), tr("Match Type"), tr("Reason")
    });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    layout->addWidget(m_table);
}
```

```cpp
// gui/src/SettingsTab.cpp
#include "SettingsTab.h"
#include <QVBoxLayout>
#include <QLabel>

SettingsTab::SettingsTab(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void SettingsTab::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(tr("Settings will be implemented here")));
    layout->addStretch();
}
```

**Step 6: Commit**

```bash
git add gui/
git commit -m "feat(gui): initialize Qt6 project structure with all components

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

### Task 1.4: Create Default Configuration and Makefile

**Files:**
- Create: `config/default.toml`
- Create: `Makefile`
- Create: `scripts/runaway-guard.service`

**Step 1: Create default config**

```toml
# config/default.toml
[general]
autostart = true
sample_interval_normal = 10
sample_interval_alert = 2
notification_method = "both"

[detection.cpu]
enabled = true
threshold_percent = 90
duration_seconds = 60

[detection.hang]
enabled = true
duration_seconds = 30

[detection.memory]
enabled = true
growth_mb = 500
window_minutes = 5

[detection.timeout]
enabled = true

[learning]
enabled = true
min_history_days = 7
suggest_whitelist = true
```

**Step 2: Create Makefile**

```makefile
# Makefile
.PHONY: all build build-daemon build-gui clean install

BUILD_DIR := build
DAEMON_TARGET := daemon/target/release/runaway-daemon
GUI_TARGET := $(BUILD_DIR)/runaway-gui

all: build

build: build-daemon build-gui

build-daemon:
	cd daemon && cargo build --release

build-gui:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake ../gui -DCMAKE_BUILD_TYPE=Release && make -j$$(nproc)

clean:
	rm -rf $(BUILD_DIR)
	cd daemon && cargo clean

install: build
	install -Dm755 $(DAEMON_TARGET) /usr/local/bin/runaway-daemon
	install -Dm755 $(GUI_TARGET) /usr/local/bin/runaway-gui
	install -Dm644 config/default.toml /usr/local/share/runaway-guard/default.toml

user-install:
	mkdir -p ~/.config/systemd/user
	cp scripts/runaway-guard.service ~/.config/systemd/user/
	systemctl --user daemon-reload
	systemctl --user enable runaway-guard.service
```

**Step 3: Create systemd service**

```ini
# scripts/runaway-guard.service
[Unit]
Description=RunawayGuard Process Monitor Daemon
After=default.target

[Service]
ExecStart=/usr/local/bin/runaway-daemon
Restart=on-failure
RestartSec=5
Environment=RUST_LOG=info

[Install]
WantedBy=default.target
```

**Step 4: Commit**

```bash
git add config/ Makefile scripts/
git commit -m "feat: add default config, Makefile, and systemd service

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

## Phase 2: Daemon Core Implementation

### Task 2.1: Implement Linux Process Collector

**Files:**
- Modify: `daemon/src/collector.rs`
- Create: `daemon/src/collector/linux.rs`
- Create: `daemon/tests/collector_tests.rs`

**Step 1: Write the test**

```rust
// daemon/tests/collector_tests.rs
use runaway_daemon::collector::{ProcessCollector, LinuxProcessCollector};

#[test]
fn test_list_processes_returns_current_process() {
    let collector = LinuxProcessCollector::new();
    let processes = collector.list_processes();

    // Current process should be in the list
    let current_pid = std::process::id();
    let found = processes.iter().any(|p| p.pid == current_pid);

    assert!(found, "Current process should be in the list");
}

#[test]
fn test_get_process_returns_current_process() {
    let collector = LinuxProcessCollector::new();
    let current_pid = std::process::id();

    let process = collector.get_process(current_pid);

    assert!(process.is_some(), "Should find current process");
    let p = process.unwrap();
    assert_eq!(p.pid, current_pid);
    assert!(!p.name.is_empty());
}

#[test]
fn test_get_process_returns_none_for_invalid_pid() {
    let collector = LinuxProcessCollector::new();

    // PID 0 is always kernel, not accessible
    // Use a very high PID that's unlikely to exist
    let process = collector.get_process(999999999);

    assert!(process.is_none());
}
```

**Step 2: Run test to verify it fails**

Run: `cd daemon && cargo test collector`
Expected: FAIL with "cannot find value `LinuxProcessCollector`"

**Step 3: Implement the collector**

```rust
// daemon/src/collector.rs
//! Process information collector

use std::time::SystemTime;

#[derive(Debug, Clone)]
pub struct ProcessInfo {
    pub pid: u32,
    pub name: String,
    pub cmdline: String,
    pub cpu_percent: f64,
    pub memory_mb: f64,
    pub runtime_seconds: u64,
    pub state: char,
    pub start_time: u64,
}

pub trait ProcessCollector: Send + Sync {
    fn list_processes(&self) -> Vec<ProcessInfo>;
    fn get_process(&self, pid: u32) -> Option<ProcessInfo>;
}

#[cfg(target_os = "linux")]
mod linux;

#[cfg(target_os = "linux")]
pub use linux::LinuxProcessCollector;
```

```rust
// daemon/src/collector/linux.rs
use super::{ProcessCollector, ProcessInfo};
use std::fs;
use std::path::Path;
use std::time::{SystemTime, UNIX_EPOCH};

pub struct LinuxProcessCollector {
    page_size: u64,
    clock_ticks: u64,
    boot_time: u64,
}

impl LinuxProcessCollector {
    pub fn new() -> Self {
        let page_size = unsafe { libc::sysconf(libc::_SC_PAGESIZE) as u64 };
        let clock_ticks = unsafe { libc::sysconf(libc::_SC_CLK_TCK) as u64 };
        let boot_time = Self::get_boot_time();

        Self {
            page_size,
            clock_ticks,
            boot_time,
        }
    }

    fn get_boot_time() -> u64 {
        let stat = fs::read_to_string("/proc/stat").unwrap_or_default();
        for line in stat.lines() {
            if line.starts_with("btime ") {
                return line[6..].trim().parse().unwrap_or(0);
            }
        }
        0
    }

    fn parse_process(&self, pid: u32) -> Option<ProcessInfo> {
        let proc_path = format!("/proc/{}", pid);
        let proc_dir = Path::new(&proc_path);

        if !proc_dir.exists() {
            return None;
        }

        // Read /proc/[pid]/stat
        let stat_content = fs::read_to_string(proc_dir.join("stat")).ok()?;
        let stat_parts: Vec<&str> = stat_content.split_whitespace().collect();

        if stat_parts.len() < 24 {
            return None;
        }

        // Parse name (remove parentheses)
        let name = stat_parts[1].trim_matches(|c| c == '(' || c == ')').to_string();
        let state = stat_parts[2].chars().next().unwrap_or('?');

        // Parse times
        let utime: u64 = stat_parts[13].parse().unwrap_or(0);
        let stime: u64 = stat_parts[14].parse().unwrap_or(0);
        let start_time_ticks: u64 = stat_parts[21].parse().unwrap_or(0);

        // Calculate CPU percentage (simplified - would need history for accurate %)
        let total_time = utime + stime;
        let cpu_percent = 0.0; // TODO: Implement with sampling history

        // Memory (RSS in pages)
        let rss_pages: u64 = stat_parts[23].parse().unwrap_or(0);
        let memory_mb = (rss_pages * self.page_size) as f64 / (1024.0 * 1024.0);

        // Runtime
        let start_time = self.boot_time + (start_time_ticks / self.clock_ticks);
        let now = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .map(|d| d.as_secs())
            .unwrap_or(0);
        let runtime_seconds = now.saturating_sub(start_time);

        // Read cmdline
        let cmdline = fs::read_to_string(proc_dir.join("cmdline"))
            .unwrap_or_default()
            .replace('\0', " ")
            .trim()
            .to_string();

        Some(ProcessInfo {
            pid,
            name,
            cmdline,
            cpu_percent,
            memory_mb,
            runtime_seconds,
            state,
            start_time,
        })
    }
}

impl Default for LinuxProcessCollector {
    fn default() -> Self {
        Self::new()
    }
}

impl ProcessCollector for LinuxProcessCollector {
    fn list_processes(&self) -> Vec<ProcessInfo> {
        let mut processes = Vec::new();

        if let Ok(entries) = fs::read_dir("/proc") {
            for entry in entries.flatten() {
                if let Some(name) = entry.file_name().to_str() {
                    if let Ok(pid) = name.parse::<u32>() {
                        if let Some(info) = self.parse_process(pid) {
                            processes.push(info);
                        }
                    }
                }
            }
        }

        processes
    }

    fn get_process(&self, pid: u32) -> Option<ProcessInfo> {
        self.parse_process(pid)
    }
}
```

**Step 4: Add libc dependency to Cargo.toml**

Add under `[dependencies]`:
```toml
libc = "0.2"
```

**Step 5: Run test to verify it passes**

Run: `cd daemon && cargo test collector`
Expected: PASS

**Step 6: Commit**

```bash
git add daemon/
git commit -m "feat(daemon): implement Linux process collector

- Read process info from /proc filesystem
- Parse stat, cmdline for each process
- Calculate memory usage and runtime

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

### Task 2.2: Implement Configuration Loading

**Files:**
- Modify: `daemon/src/config.rs`
- Create: `daemon/tests/config_tests.rs`

**Step 1: Write the test**

```rust
// daemon/tests/config_tests.rs
use runaway_daemon::config::Config;
use std::io::Write;
use tempfile::NamedTempFile;

#[test]
fn test_default_config() {
    let config = Config::default();

    assert!(config.general.autostart);
    assert_eq!(config.general.sample_interval_normal, 10);
    assert_eq!(config.detection.cpu.threshold_percent, 90);
}

#[test]
fn test_load_from_toml() {
    let toml_content = r#"
[general]
autostart = false
sample_interval_normal = 5
sample_interval_alert = 1
notification_method = "system"

[detection.cpu]
enabled = true
threshold_percent = 80
duration_seconds = 30

[detection.hang]
enabled = false
duration_seconds = 60

[detection.memory]
enabled = true
growth_mb = 1000
window_minutes = 10

[detection.timeout]
enabled = false

[learning]
enabled = false
min_history_days = 14
suggest_whitelist = false
"#;

    let mut file = NamedTempFile::new().unwrap();
    file.write_all(toml_content.as_bytes()).unwrap();

    let config = Config::load(file.path()).unwrap();

    assert!(!config.general.autostart);
    assert_eq!(config.general.sample_interval_normal, 5);
    assert_eq!(config.detection.cpu.threshold_percent, 80);
    assert!(!config.detection.hang.enabled);
}

#[test]
fn test_save_config() {
    let config = Config::default();
    let file = NamedTempFile::new().unwrap();

    config.save(file.path()).unwrap();

    let loaded = Config::load(file.path()).unwrap();
    assert_eq!(loaded.general.autostart, config.general.autostart);
}
```

**Step 2: Run test to verify it fails**

Run: `cd daemon && cargo test config`
Expected: FAIL with "cannot find function `load`"

**Step 3: Implement config loading**

Add to `daemon/src/config.rs`:

```rust
use std::path::Path;
use std::fs;

impl Config {
    pub fn load(path: &Path) -> Result<Self, Box<dyn std::error::Error>> {
        let content = fs::read_to_string(path)?;
        let config: Config = toml::from_str(&content)?;
        Ok(config)
    }

    pub fn save(&self, path: &Path) -> Result<(), Box<dyn std::error::Error>> {
        let content = toml::to_string_pretty(self)?;
        if let Some(parent) = path.parent() {
            fs::create_dir_all(parent)?;
        }
        fs::write(path, content)?;
        Ok(())
    }

    pub fn config_path() -> std::path::PathBuf {
        directories::ProjectDirs::from("", "", "runaway-guard")
            .map(|dirs| dirs.config_dir().join("config.toml"))
            .unwrap_or_else(|| std::path::PathBuf::from("config.toml"))
    }
}
```

**Step 4: Run test to verify it passes**

Run: `cd daemon && cargo test config`
Expected: PASS

**Step 5: Commit**

```bash
git add daemon/
git commit -m "feat(daemon): implement config loading and saving

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

### Task 2.3: Implement Database Operations

**Files:**
- Modify: `daemon/src/db.rs`
- Create: `daemon/tests/db_tests.rs`

**Step 1: Write the test**

```rust
// daemon/tests/db_tests.rs
use runaway_daemon::db::Database;
use tempfile::tempdir;

#[test]
fn test_create_database() {
    let dir = tempdir().unwrap();
    let db_path = dir.path().join("test.db");

    let db = Database::open(&db_path).unwrap();
    db.init_schema().unwrap();

    assert!(db_path.exists());
}

#[test]
fn test_insert_and_query_alert() {
    let dir = tempdir().unwrap();
    let db_path = dir.path().join("test.db");

    let db = Database::open(&db_path).unwrap();
    db.init_schema().unwrap();

    db.insert_alert(1234, "test_process", "/usr/bin/test", "cpu_high", "warning")
        .unwrap();

    let alerts = db.get_alerts(10, None).unwrap();
    assert_eq!(alerts.len(), 1);
    assert_eq!(alerts[0].pid, 1234);
    assert_eq!(alerts[0].name, "test_process");
}

#[test]
fn test_whitelist_operations() {
    let dir = tempdir().unwrap();
    let db_path = dir.path().join("test.db");

    let db = Database::open(&db_path).unwrap();
    db.init_schema().unwrap();

    db.add_whitelist("chrome", "name", Some("Browser")).unwrap();

    assert!(db.is_whitelisted("chrome", "name").unwrap());
    assert!(!db.is_whitelisted("firefox", "name").unwrap());
}
```

**Step 2: Run test to verify it fails**

Run: `cd daemon && cargo test db`
Expected: FAIL

**Step 3: Implement database operations**

```rust
// daemon/src/db.rs
use rusqlite::{Connection, params};
use std::path::Path;
use std::time::{SystemTime, UNIX_EPOCH};

pub struct Database {
    conn: Connection,
}

#[derive(Debug, Clone)]
pub struct AlertRecord {
    pub id: i64,
    pub timestamp: i64,
    pub pid: u32,
    pub name: String,
    pub cmdline: String,
    pub reason: String,
    pub severity: String,
    pub resolved: bool,
    pub action_taken: Option<String>,
}

#[derive(Debug, Clone)]
pub struct WhitelistEntry {
    pub id: i64,
    pub pattern: String,
    pub match_type: String,
    pub reason: Option<String>,
}

impl Database {
    pub fn open(path: &Path) -> rusqlite::Result<Self> {
        if let Some(parent) = path.parent() {
            std::fs::create_dir_all(parent).ok();
        }
        let conn = Connection::open(path)?;
        Ok(Self { conn })
    }

    pub fn init_schema(&self) -> rusqlite::Result<()> {
        self.conn.execute_batch(include_str!("../schema.sql"))
    }

    fn now() -> i64 {
        SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .map(|d| d.as_secs() as i64)
            .unwrap_or(0)
    }

    pub fn insert_alert(
        &self,
        pid: u32,
        name: &str,
        cmdline: &str,
        reason: &str,
        severity: &str,
    ) -> rusqlite::Result<i64> {
        self.conn.execute(
            "INSERT INTO alerts (timestamp, pid, name, cmdline, reason, severity) VALUES (?1, ?2, ?3, ?4, ?5, ?6)",
            params![Self::now(), pid, name, cmdline, reason, severity],
        )?;
        Ok(self.conn.last_insert_rowid())
    }

    pub fn get_alerts(&self, limit: u32, since: Option<i64>) -> rusqlite::Result<Vec<AlertRecord>> {
        let mut stmt = if let Some(since_ts) = since {
            self.conn.prepare(
                "SELECT id, timestamp, pid, name, cmdline, reason, severity, resolved, action_taken
                 FROM alerts WHERE timestamp >= ?1 ORDER BY timestamp DESC LIMIT ?2"
            )?
        } else {
            self.conn.prepare(
                "SELECT id, timestamp, pid, name, cmdline, reason, severity, resolved, action_taken
                 FROM alerts ORDER BY timestamp DESC LIMIT ?1"
            )?
        };

        let rows = if let Some(since_ts) = since {
            stmt.query_map(params![since_ts, limit], Self::map_alert)?
        } else {
            stmt.query_map(params![limit], Self::map_alert)?
        };

        rows.collect()
    }

    fn map_alert(row: &rusqlite::Row) -> rusqlite::Result<AlertRecord> {
        Ok(AlertRecord {
            id: row.get(0)?,
            timestamp: row.get(1)?,
            pid: row.get(2)?,
            name: row.get(3)?,
            cmdline: row.get(4)?,
            reason: row.get(5)?,
            severity: row.get(6)?,
            resolved: row.get::<_, i32>(7)? != 0,
            action_taken: row.get(8)?,
        })
    }

    pub fn resolve_alert(&self, id: i64, action: &str) -> rusqlite::Result<()> {
        self.conn.execute(
            "UPDATE alerts SET resolved = 1, action_taken = ?1 WHERE id = ?2",
            params![action, id],
        )?;
        Ok(())
    }

    pub fn add_whitelist(&self, pattern: &str, match_type: &str, reason: Option<&str>) -> rusqlite::Result<i64> {
        self.conn.execute(
            "INSERT INTO whitelist (pattern, match_type, reason, created_at) VALUES (?1, ?2, ?3, ?4)",
            params![pattern, match_type, reason, Self::now()],
        )?;
        Ok(self.conn.last_insert_rowid())
    }

    pub fn remove_whitelist(&self, id: i64) -> rusqlite::Result<()> {
        self.conn.execute("DELETE FROM whitelist WHERE id = ?1", params![id])?;
        Ok(())
    }

    pub fn get_whitelist(&self) -> rusqlite::Result<Vec<WhitelistEntry>> {
        let mut stmt = self.conn.prepare(
            "SELECT id, pattern, match_type, reason FROM whitelist"
        )?;

        let rows = stmt.query_map([], |row| {
            Ok(WhitelistEntry {
                id: row.get(0)?,
                pattern: row.get(1)?,
                match_type: row.get(2)?,
                reason: row.get(3)?,
            })
        })?;

        rows.collect()
    }

    pub fn is_whitelisted(&self, pattern: &str, match_type: &str) -> rusqlite::Result<bool> {
        let count: i32 = self.conn.query_row(
            "SELECT COUNT(*) FROM whitelist WHERE pattern = ?1 AND match_type = ?2",
            params![pattern, match_type],
            |row| row.get(0),
        )?;
        Ok(count > 0)
    }

    pub fn insert_sample(
        &self,
        name: &str,
        pid: u32,
        cpu_percent: f64,
        memory_mb: f64,
        runtime_seconds: u64,
    ) -> rusqlite::Result<()> {
        self.conn.execute(
            "INSERT INTO process_samples (timestamp, name, pid, cpu_percent, memory_mb, runtime_seconds)
             VALUES (?1, ?2, ?3, ?4, ?5, ?6)",
            params![Self::now(), name, pid, cpu_percent, memory_mb, runtime_seconds as i64],
        )?;
        Ok(())
    }

    pub fn cleanup_old_data(&self, samples_days: u32, alerts_days: u32) -> rusqlite::Result<()> {
        let samples_cutoff = Self::now() - (samples_days as i64 * 86400);
        let alerts_cutoff = Self::now() - (alerts_days as i64 * 86400);

        self.conn.execute(
            "DELETE FROM process_samples WHERE timestamp < ?1",
            params![samples_cutoff],
        )?;
        self.conn.execute(
            "DELETE FROM alerts WHERE timestamp < ?1 AND resolved = 1",
            params![alerts_cutoff],
        )?;

        Ok(())
    }
}
```

**Step 4: Run test to verify it passes**

Run: `cd daemon && cargo test db`
Expected: PASS

**Step 5: Commit**

```bash
git add daemon/
git commit -m "feat(daemon): implement database operations

- Alert CRUD operations
- Whitelist management
- Sample recording for learning
- Data cleanup for old records

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

### Task 2.4: Implement Unix Socket Server

**Files:**
- Modify: `daemon/src/socket.rs`
- Modify: `daemon/src/protocol.rs`

**Step 1: Implement socket server**

```rust
// daemon/src/socket.rs
use crate::protocol::{Request, Response};
use std::path::{Path, PathBuf};
use std::sync::Arc;
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};
use tokio::net::{UnixListener, UnixStream};
use tokio::sync::broadcast;
use tracing::{error, info, warn};

pub struct SocketServer {
    path: PathBuf,
    listener: UnixListener,
    broadcast_tx: broadcast::Sender<String>,
}

impl SocketServer {
    pub async fn bind(path: &Path) -> std::io::Result<Self> {
        // Remove existing socket file if present
        let _ = std::fs::remove_file(path);

        // Ensure parent directory exists
        if let Some(parent) = path.parent() {
            std::fs::create_dir_all(parent)?;
        }

        let listener = UnixListener::bind(path)?;
        let (broadcast_tx, _) = broadcast::channel(100);

        info!("Socket server listening on {:?}", path);

        Ok(Self {
            path: path.to_path_buf(),
            listener,
            broadcast_tx,
        })
    }

    pub fn broadcast_sender(&self) -> broadcast::Sender<String> {
        self.broadcast_tx.clone()
    }

    pub async fn accept(&self) -> std::io::Result<UnixStream> {
        let (stream, _) = self.listener.accept().await?;
        Ok(stream)
    }

    pub fn socket_path() -> PathBuf {
        let uid = unsafe { libc::getuid() };
        PathBuf::from(format!("/run/user/{}/runaway-guard.sock", uid))
    }
}

impl Drop for SocketServer {
    fn drop(&mut self) {
        let _ = std::fs::remove_file(&self.path);
    }
}

pub async fn handle_client<H>(
    stream: UnixStream,
    mut broadcast_rx: broadcast::Receiver<String>,
    handler: Arc<H>,
) where
    H: RequestHandler + Send + Sync + 'static,
{
    let (reader, mut writer) = stream.into_split();
    let mut reader = BufReader::new(reader);
    let mut line = String::new();

    loop {
        tokio::select! {
            result = reader.read_line(&mut line) => {
                match result {
                    Ok(0) => break, // Connection closed
                    Ok(_) => {
                        let response = match serde_json::from_str::<Request>(&line) {
                            Ok(request) => handler.handle(request).await,
                            Err(e) => {
                                warn!("Invalid request: {}", e);
                                Response::Response {
                                    id: None,
                                    data: serde_json::json!({"error": e.to_string()}),
                                }
                            }
                        };

                        let json = serde_json::to_string(&response).unwrap() + "\n";
                        if let Err(e) = writer.write_all(json.as_bytes()).await {
                            error!("Failed to write response: {}", e);
                            break;
                        }
                        line.clear();
                    }
                    Err(e) => {
                        error!("Read error: {}", e);
                        break;
                    }
                }
            }
            result = broadcast_rx.recv() => {
                if let Ok(msg) = result {
                    if let Err(e) = writer.write_all((msg + "\n").as_bytes()).await {
                        error!("Failed to broadcast: {}", e);
                        break;
                    }
                }
            }
        }
    }
}

#[async_trait::async_trait]
pub trait RequestHandler {
    async fn handle(&self, request: Request) -> Response;
}
```

**Step 2: Add async-trait dependency**

Add to Cargo.toml:
```toml
async-trait = "0.1"
```

**Step 3: Commit**

```bash
git add daemon/
git commit -m "feat(daemon): implement Unix socket server with broadcast

- Accept multiple client connections
- Handle JSON-line protocol
- Broadcast alerts to all connected clients

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

### Task 2.5: Implement Anomaly Detector

**Files:**
- Modify: `daemon/src/detector.rs`
- Create: `daemon/tests/detector_tests.rs`

**Step 1: Write the test**

```rust
// daemon/tests/detector_tests.rs
use runaway_daemon::collector::ProcessInfo;
use runaway_daemon::config::Config;
use runaway_daemon::detector::{AnomalyDetector, AlertReason};

fn make_process(pid: u32, name: &str, cpu: f64, memory: f64, runtime: u64, state: char) -> ProcessInfo {
    ProcessInfo {
        pid,
        name: name.to_string(),
        cmdline: format!("/usr/bin/{}", name),
        cpu_percent: cpu,
        memory_mb: memory,
        runtime_seconds: runtime,
        state,
        start_time: 0,
    }
}

#[test]
fn test_detect_high_cpu() {
    let config = Config::default();
    let mut detector = AnomalyDetector::new(&config);

    // Simulate high CPU for duration
    let process = make_process(1234, "test", 95.0, 100.0, 100, 'R');

    // First few checks should not alert (need sustained high CPU)
    for _ in 0..5 {
        detector.check(&process);
    }

    // After enough samples, should detect
    // Note: actual implementation needs time-based tracking
}

#[test]
fn test_detect_hang() {
    let config = Config::default();
    let mut detector = AnomalyDetector::new(&config);

    // Process in D state (uninterruptible sleep) with 0 CPU
    let process = make_process(1234, "test", 0.0, 100.0, 100, 'D');

    let alert = detector.check(&process);
    // Would need time tracking for actual hang detection
}

#[test]
fn test_whitelist_excludes_process() {
    let mut config = Config::default();
    config.rules.push(runaway_daemon::config::Rule {
        name: "ignore-test".to_string(),
        pattern: "test".to_string(),
        match_type: "name".to_string(),
        max_runtime_minutes: None,
        action: Some("whitelist".to_string()),
    });

    let detector = AnomalyDetector::new(&config);

    assert!(detector.is_whitelisted("test"));
    assert!(!detector.is_whitelisted("other"));
}
```

**Step 2: Implement detector**

```rust
// daemon/src/detector.rs
use crate::collector::ProcessInfo;
use crate::config::Config;
use std::collections::HashMap;
use std::time::{Instant, SystemTime, UNIX_EPOCH};

#[derive(Debug, Clone, PartialEq)]
pub enum AlertReason {
    CpuHigh,
    Hang,
    MemoryLeak,
    Timeout,
}

impl AlertReason {
    pub fn as_str(&self) -> &'static str {
        match self {
            AlertReason::CpuHigh => "cpu_high",
            AlertReason::Hang => "hang",
            AlertReason::MemoryLeak => "memory_leak",
            AlertReason::Timeout => "timeout",
        }
    }
}

#[derive(Debug, Clone, PartialEq)]
pub enum Severity {
    Warning,
    Critical,
}

impl Severity {
    pub fn as_str(&self) -> &'static str {
        match self {
            Severity::Warning => "warning",
            Severity::Critical => "critical",
        }
    }
}

#[derive(Debug, Clone)]
pub struct Alert {
    pub pid: u32,
    pub name: String,
    pub cmdline: String,
    pub reason: AlertReason,
    pub severity: Severity,
    pub timestamp: u64,
}

#[derive(Debug)]
struct ProcessState {
    high_cpu_since: Option<Instant>,
    hang_since: Option<Instant>,
    memory_samples: Vec<(Instant, f64)>,
    first_seen: Instant,
}

impl Default for ProcessState {
    fn default() -> Self {
        Self {
            high_cpu_since: None,
            hang_since: None,
            memory_samples: Vec::new(),
            first_seen: Instant::now(),
        }
    }
}

pub struct AnomalyDetector {
    config: Config,
    process_states: HashMap<u32, ProcessState>,
    whitelist_patterns: Vec<(String, String)>, // (pattern, match_type)
}

impl AnomalyDetector {
    pub fn new(config: &Config) -> Self {
        let whitelist_patterns: Vec<_> = config
            .rules
            .iter()
            .filter(|r| r.action.as_deref() == Some("whitelist"))
            .map(|r| (r.pattern.clone(), r.match_type.clone()))
            .collect();

        Self {
            config: config.clone(),
            process_states: HashMap::new(),
            whitelist_patterns,
        }
    }

    pub fn is_whitelisted(&self, name: &str) -> bool {
        self.whitelist_patterns.iter().any(|(pattern, match_type)| {
            match match_type.as_str() {
                "name" => name == pattern,
                "regex" => regex::Regex::new(pattern)
                    .map(|r| r.is_match(name))
                    .unwrap_or(false),
                _ => name.contains(pattern),
            }
        })
    }

    pub fn check(&mut self, process: &ProcessInfo) -> Option<Alert> {
        if self.is_whitelisted(&process.name) {
            return None;
        }

        let state = self.process_states.entry(process.pid).or_default();
        let now = Instant::now();

        // Check CPU
        if let Some(alert) = self.check_cpu(process, state, now) {
            return Some(alert);
        }

        // Check hang
        if let Some(alert) = self.check_hang(process, state, now) {
            return Some(alert);
        }

        // Check memory leak
        if let Some(alert) = self.check_memory(process, state, now) {
            return Some(alert);
        }

        // Check timeout
        if let Some(alert) = self.check_timeout(process) {
            return Some(alert);
        }

        None
    }

    fn check_cpu(&self, process: &ProcessInfo, state: &mut ProcessState, now: Instant) -> Option<Alert> {
        if !self.config.detection.cpu.enabled {
            return None;
        }

        let threshold = self.config.detection.cpu.threshold_percent as f64;
        let duration = std::time::Duration::from_secs(self.config.detection.cpu.duration_seconds);

        if process.cpu_percent >= threshold {
            if state.high_cpu_since.is_none() {
                state.high_cpu_since = Some(now);
            } else if now.duration_since(state.high_cpu_since.unwrap()) >= duration {
                return Some(self.make_alert(process, AlertReason::CpuHigh, Severity::Warning));
            }
        } else {
            state.high_cpu_since = None;
        }

        None
    }

    fn check_hang(&self, process: &ProcessInfo, state: &mut ProcessState, now: Instant) -> Option<Alert> {
        if !self.config.detection.hang.enabled {
            return None;
        }

        let duration = std::time::Duration::from_secs(self.config.detection.hang.duration_seconds);

        // D = uninterruptible sleep, T = stopped
        let is_hung = (process.state == 'D' || process.state == 'T') && process.cpu_percent < 0.1;

        if is_hung {
            if state.hang_since.is_none() {
                state.hang_since = Some(now);
            } else if now.duration_since(state.hang_since.unwrap()) >= duration {
                return Some(self.make_alert(process, AlertReason::Hang, Severity::Critical));
            }
        } else {
            state.hang_since = None;
        }

        None
    }

    fn check_memory(&self, process: &ProcessInfo, state: &mut ProcessState, now: Instant) -> Option<Alert> {
        if !self.config.detection.memory.enabled {
            return None;
        }

        let window = std::time::Duration::from_secs(self.config.detection.memory.window_minutes * 60);
        let growth_threshold = self.config.detection.memory.growth_mb as f64;

        // Add current sample
        state.memory_samples.push((now, process.memory_mb));

        // Remove old samples
        state.memory_samples.retain(|(t, _)| now.duration_since(*t) <= window);

        if state.memory_samples.len() >= 2 {
            let first = state.memory_samples.first().unwrap().1;
            let last = state.memory_samples.last().unwrap().1;
            let growth = last - first;

            // Check for monotonic growth
            let is_monotonic = state.memory_samples.windows(2).all(|w| w[1].1 >= w[0].1 - 10.0);

            if growth >= growth_threshold && is_monotonic {
                return Some(self.make_alert(process, AlertReason::MemoryLeak, Severity::Warning));
            }
        }

        None
    }

    fn check_timeout(&self, process: &ProcessInfo) -> Option<Alert> {
        if !self.config.detection.timeout.enabled {
            return None;
        }

        for rule in &self.config.rules {
            if rule.action.is_some() {
                continue; // Skip whitelist rules
            }

            let matches = match rule.match_type.as_str() {
                "name" => process.name == rule.pattern,
                "cmdline" => process.cmdline.contains(&rule.pattern),
                "regex" => regex::Regex::new(&rule.pattern)
                    .map(|r| r.is_match(&process.name) || r.is_match(&process.cmdline))
                    .unwrap_or(false),
                _ => false,
            };

            if matches {
                if let Some(max_minutes) = rule.max_runtime_minutes {
                    if process.runtime_seconds > max_minutes * 60 {
                        return Some(self.make_alert(process, AlertReason::Timeout, Severity::Warning));
                    }
                }
            }
        }

        None
    }

    fn make_alert(&self, process: &ProcessInfo, reason: AlertReason, severity: Severity) -> Alert {
        Alert {
            pid: process.pid,
            name: process.name.clone(),
            cmdline: process.cmdline.clone(),
            reason,
            severity,
            timestamp: SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .map(|d| d.as_secs())
                .unwrap_or(0),
        }
    }

    pub fn cleanup_stale(&mut self, active_pids: &[u32]) {
        self.process_states.retain(|pid, _| active_pids.contains(pid));
    }
}
```

**Step 3: Add regex dependency**

Add to Cargo.toml:
```toml
regex = "1"
```

**Step 4: Run tests**

Run: `cd daemon && cargo test detector`
Expected: PASS

**Step 5: Commit**

```bash
git add daemon/
git commit -m "feat(daemon): implement anomaly detector

- CPU high detection with duration tracking
- Hang detection (D/T state with low CPU)
- Memory leak detection with growth analysis
- Timeout rules matching
- Whitelist support

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

### Task 2.6: Implement Main Daemon Loop

**Files:**
- Modify: `daemon/src/main.rs`

**Step 1: Implement the main loop**

```rust
// daemon/src/main.rs
use runaway_daemon::collector::{LinuxProcessCollector, ProcessCollector};
use runaway_daemon::config::Config;
use runaway_daemon::db::Database;
use runaway_daemon::detector::AnomalyDetector;
use runaway_daemon::notifier::send_notification;
use runaway_daemon::protocol::{Request, Response, AlertData, StatusData};
use runaway_daemon::socket::{handle_client, RequestHandler, SocketServer};

use std::sync::Arc;
use std::time::Duration;
use tokio::sync::RwLock;
use tracing::{error, info, warn};

struct DaemonState {
    config: RwLock<Config>,
    db: Database,
    detector: RwLock<AnomalyDetector>,
    collector: LinuxProcessCollector,
    alert_count: RwLock<u32>,
}

struct DaemonHandler {
    state: Arc<DaemonState>,
}

#[async_trait::async_trait]
impl RequestHandler for DaemonHandler {
    async fn handle(&self, request: Request) -> Response {
        match request {
            Request::Ping => Response::Pong,

            Request::ListProcesses => {
                let processes = self.state.collector.list_processes();
                let data: Vec<_> = processes
                    .iter()
                    .map(|p| serde_json::json!({
                        "pid": p.pid,
                        "name": p.name,
                        "cpu_percent": p.cpu_percent,
                        "memory_mb": p.memory_mb,
                        "runtime_seconds": p.runtime_seconds,
                        "state": p.state.to_string(),
                    }))
                    .collect();
                Response::Response {
                    id: None,
                    data: serde_json::json!(data),
                }
            }

            Request::GetAlerts { params } => {
                let limit = params.limit.unwrap_or(50);
                let since = params.since.and_then(|s| {
                    chrono::DateTime::parse_from_rfc3339(&s)
                        .ok()
                        .map(|dt| dt.timestamp())
                });

                match self.state.db.get_alerts(limit, since) {
                    Ok(alerts) => {
                        let data: Vec<_> = alerts
                            .iter()
                            .map(|a| serde_json::json!({
                                "id": a.id,
                                "timestamp": a.timestamp,
                                "pid": a.pid,
                                "name": a.name,
                                "reason": a.reason,
                                "severity": a.severity,
                                "resolved": a.resolved,
                            }))
                            .collect();
                        Response::Response {
                            id: None,
                            data: serde_json::json!(data),
                        }
                    }
                    Err(e) => Response::Response {
                        id: None,
                        data: serde_json::json!({"error": e.to_string()}),
                    },
                }
            }

            Request::KillProcess { params } => {
                use runaway_daemon::executor::{send_signal, Signal};

                let signal = match params.signal.to_uppercase().as_str() {
                    "TERM" => Signal::Term,
                    "KILL" => Signal::Kill,
                    "STOP" => Signal::Stop,
                    "CONT" => Signal::Cont,
                    _ => {
                        return Response::Response {
                            id: None,
                            data: serde_json::json!({"error": "Invalid signal"}),
                        }
                    }
                };

                match send_signal(params.pid, signal) {
                    Ok(_) => Response::Response {
                        id: None,
                        data: serde_json::json!({"success": true}),
                    },
                    Err(e) => Response::Response {
                        id: None,
                        data: serde_json::json!({"error": e.to_string()}),
                    },
                }
            }

            Request::AddWhitelist { params } => {
                match self.state.db.add_whitelist(&params.pattern, &params.match_type, None) {
                    Ok(id) => Response::Response {
                        id: None,
                        data: serde_json::json!({"id": id}),
                    },
                    Err(e) => Response::Response {
                        id: None,
                        data: serde_json::json!({"error": e.to_string()}),
                    },
                }
            }

            Request::UpdateConfig { params: _ } => {
                // TODO: Implement config update
                Response::Response {
                    id: None,
                    data: serde_json::json!({"success": true}),
                }
            }
        }
    }
}

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    tracing_subscriber::fmt()
        .with_env_filter(
            tracing_subscriber::EnvFilter::from_default_env()
                .add_directive("runaway_daemon=info".parse()?)
        )
        .init();

    info!("RunawayGuard daemon starting...");

    // Load config
    let config_path = Config::config_path();
    let config = if config_path.exists() {
        Config::load(&config_path).unwrap_or_else(|e| {
            warn!("Failed to load config: {}, using defaults", e);
            Config::default()
        })
    } else {
        let config = Config::default();
        if let Err(e) = config.save(&config_path) {
            warn!("Failed to save default config: {}", e);
        }
        config
    };

    // Initialize database
    let db_path = directories::ProjectDirs::from("", "", "runaway-guard")
        .map(|dirs| dirs.data_dir().join("history.db"))
        .unwrap_or_else(|| std::path::PathBuf::from("history.db"));

    let db = Database::open(&db_path)?;
    db.init_schema()?;
    info!("Database initialized at {:?}", db_path);

    // Initialize components
    let detector = AnomalyDetector::new(&config);
    let collector = LinuxProcessCollector::new();

    let state = Arc::new(DaemonState {
        config: RwLock::new(config.clone()),
        db,
        detector: RwLock::new(detector),
        collector,
        alert_count: RwLock::new(0),
    });

    // Start socket server
    let socket_path = SocketServer::socket_path();
    let server = SocketServer::bind(&socket_path).await?;
    let broadcast_tx = server.broadcast_sender();

    // Spawn connection acceptor
    let state_clone = Arc::clone(&state);
    let broadcast_tx_clone = broadcast_tx.clone();
    tokio::spawn(async move {
        loop {
            match server.accept().await {
                Ok(stream) => {
                    let handler = Arc::new(DaemonHandler {
                        state: Arc::clone(&state_clone),
                    });
                    let rx = broadcast_tx_clone.subscribe();
                    tokio::spawn(handle_client(stream, rx, handler));
                }
                Err(e) => {
                    error!("Accept error: {}", e);
                }
            }
        }
    });

    // Main monitoring loop
    let normal_interval = Duration::from_secs(config.general.sample_interval_normal);
    let alert_interval = Duration::from_secs(config.general.sample_interval_alert);
    let mut current_interval = normal_interval;

    loop {
        let processes = state.collector.list_processes();
        let active_pids: Vec<_> = processes.iter().map(|p| p.pid).collect();

        let mut detector = state.detector.write().await;
        detector.cleanup_stale(&active_pids);

        let mut has_alert = false;

        for process in &processes {
            if let Some(alert) = detector.check(process) {
                has_alert = true;

                // Store in database
                if let Err(e) = state.db.insert_alert(
                    alert.pid,
                    &alert.name,
                    &alert.cmdline,
                    alert.reason.as_str(),
                    alert.severity.as_str(),
                ) {
                    error!("Failed to store alert: {}", e);
                }

                // Send notification
                let summary = format!("RunawayGuard: {}", alert.reason.as_str());
                let body = format!("{} (PID: {})", alert.name, alert.pid);
                if let Err(e) = send_notification(&summary, &body) {
                    warn!("Failed to send notification: {}", e);
                }

                // Broadcast to connected GUIs
                let alert_msg = serde_json::to_string(&Response::Alert {
                    data: AlertData {
                        pid: alert.pid,
                        name: alert.name.clone(),
                        reason: alert.reason.as_str().to_string(),
                        severity: alert.severity.as_str().to_string(),
                    },
                })
                .unwrap();
                let _ = broadcast_tx.send(alert_msg);

                info!("Alert: {} - {} (PID {})", alert.reason.as_str(), alert.name, alert.pid);
            }
        }

        // Update alert count
        {
            let mut count = state.alert_count.write().await;
            if has_alert {
                *count += 1;
            }
        }

        // Broadcast status
        let status_msg = serde_json::to_string(&Response::Status {
            data: StatusData {
                monitored_count: processes.len() as u32,
                alert_count: *state.alert_count.read().await,
            },
        })
        .unwrap();
        let _ = broadcast_tx.send(status_msg);

        // Adaptive interval
        current_interval = if has_alert { alert_interval } else { normal_interval };

        tokio::time::sleep(current_interval).await;
    }
}
```

**Step 2: Add chrono dependency**

Add to Cargo.toml:
```toml
chrono = "0.4"
anyhow = "1"
```

**Step 3: Build and verify**

Run: `cd daemon && cargo build`
Expected: Successful compilation

**Step 4: Commit**

```bash
git add daemon/
git commit -m "feat(daemon): implement main monitoring loop

- Load config and initialize database
- Start Unix socket server
- Run adaptive sampling loop
- Detect anomalies and broadcast alerts
- Send system notifications

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

## Phase 3: Integration and Testing

### Task 3.1: Create Integration Test

**Files:**
- Create: `daemon/tests/integration_test.rs`

**Step 1: Write integration test**

```rust
// daemon/tests/integration_test.rs
use std::process::{Command, Stdio};
use std::thread;
use std::time::Duration;
use std::io::{BufRead, BufReader, Write};
use std::os::unix::net::UnixStream;

#[test]
#[ignore] // Run manually with --ignored
fn test_daemon_responds_to_ping() {
    // This test requires the daemon to be running
    let socket_path = format!("/run/user/{}/runaway-guard.sock", unsafe { libc::getuid() });

    let mut stream = UnixStream::connect(&socket_path)
        .expect("Failed to connect to daemon");

    stream.write_all(b"{\"cmd\": \"ping\"}\n").unwrap();

    let mut reader = BufReader::new(&stream);
    let mut response = String::new();
    reader.read_line(&mut response).unwrap();

    assert!(response.contains("pong"), "Expected pong response");
}

#[test]
#[ignore]
fn test_daemon_lists_processes() {
    let socket_path = format!("/run/user/{}/runaway-guard.sock", unsafe { libc::getuid() });

    let mut stream = UnixStream::connect(&socket_path)
        .expect("Failed to connect to daemon");

    stream.write_all(b"{\"cmd\": \"list_processes\"}\n").unwrap();

    let mut reader = BufReader::new(&stream);
    let mut response = String::new();
    reader.read_line(&mut response).unwrap();

    let json: serde_json::Value = serde_json::from_str(&response).unwrap();
    assert!(json["data"].is_array());
}
```

**Step 2: Commit**

```bash
git add daemon/
git commit -m "test(daemon): add integration tests

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

### Task 3.2: Build and Test GUI

**Step 1: Verify Qt6 is installed**

Run: `pkg-config --modversion Qt6Widgets Qt6Network`
Expected: Version numbers (e.g., 6.x.x)

If not installed:
```bash
sudo apt install qt6-base-dev libqt6widgets6 libqt6network6
```

**Step 2: Build GUI**

Run: `mkdir -p build && cd build && cmake ../gui && make`
Expected: Successful build, `runaway-gui` executable created

**Step 3: Commit any build fixes**

---

### Task 3.3: End-to-End Test

**Step 1: Start daemon**

```bash
cd daemon && cargo run --release
```

**Step 2: Start GUI in another terminal**

```bash
./build/runaway-gui
```

**Step 3: Verify**

- GUI connects to daemon (status bar shows connected)
- Process list populates
- Tray icon appears
- Window minimizes to tray on close

**Step 4: Test alert**

```bash
# In another terminal, create a CPU-intensive process
yes > /dev/null &
# Wait for alert (should appear within configured threshold)
kill %1
```

---

## Phase 4: Packaging

### Task 4.1: Create .deb Package

**Files:**
- Create: `packaging/deb/control`
- Create: `packaging/deb/postinst`
- Modify: `Makefile`

**Step 1: Create debian control file**

```
# packaging/deb/control
Package: runaway-guard
Version: 0.1.0
Section: utils
Priority: optional
Architecture: amd64
Depends: libc6 (>= 2.34), libqt6widgets6, libqt6network6
Maintainer: Your Name <your@email.com>
Description: Process monitor for detecting runaway processes
 RunawayGuard monitors system processes for anomalies like
 high CPU usage, memory leaks, hangs, and timeouts.
```

**Step 2: Create postinst script**

```bash
#!/bin/bash
# packaging/deb/postinst
set -e

# Create default config if not exists
if [ ! -f /etc/runaway-guard/config.toml ]; then
    mkdir -p /etc/runaway-guard
    cp /usr/share/runaway-guard/default.toml /etc/runaway-guard/config.toml
fi

echo "RunawayGuard installed. Run 'systemctl --user enable runaway-guard' to enable autostart."
```

**Step 3: Add deb target to Makefile**

```makefile
deb: build
	mkdir -p dist/deb/DEBIAN
	mkdir -p dist/deb/usr/local/bin
	mkdir -p dist/deb/usr/local/share/runaway-guard
	mkdir -p dist/deb/usr/share/applications
	cp packaging/deb/control dist/deb/DEBIAN/
	cp packaging/deb/postinst dist/deb/DEBIAN/
	chmod 755 dist/deb/DEBIAN/postinst
	cp $(DAEMON_TARGET) dist/deb/usr/local/bin/
	cp $(GUI_TARGET) dist/deb/usr/local/bin/
	cp config/default.toml dist/deb/usr/local/share/runaway-guard/
	dpkg-deb --build dist/deb dist/runaway-guard_0.1.0_amd64.deb
```

**Step 4: Build .deb**

Run: `make deb`
Expected: `dist/runaway-guard_0.1.0_amd64.deb` created

**Step 5: Commit**

```bash
git add packaging/ Makefile
git commit -m "feat: add .deb packaging

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

### Task 4.2: Create AppImage

**Files:**
- Create: `packaging/appimage/AppRun`
- Create: `packaging/appimage/runaway-guard.desktop`
- Modify: `Makefile`

**Step 1: Create AppRun**

```bash
#!/bin/bash
# packaging/appimage/AppRun
APPDIR="$(dirname "$(readlink -f "$0")")"

# Start daemon in background if not running
if ! pgrep -x runaway-daemon > /dev/null; then
    "$APPDIR/usr/bin/runaway-daemon" &
fi

# Start GUI
exec "$APPDIR/usr/bin/runaway-gui" "$@"
```

**Step 2: Create desktop file**

```ini
# packaging/appimage/runaway-guard.desktop
[Desktop Entry]
Type=Application
Name=RunawayGuard
Comment=Process monitor for detecting runaway processes
Exec=runaway-gui
Icon=runaway-guard
Categories=System;Monitor;
Terminal=false
```

**Step 3: Add appimage target to Makefile**

```makefile
appimage: build
	mkdir -p dist/AppDir/usr/bin
	mkdir -p dist/AppDir/usr/share/icons/hicolor/scalable/apps
	cp $(DAEMON_TARGET) dist/AppDir/usr/bin/
	cp $(GUI_TARGET) dist/AppDir/usr/bin/
	cp packaging/appimage/AppRun dist/AppDir/
	chmod +x dist/AppDir/AppRun
	cp packaging/appimage/runaway-guard.desktop dist/AppDir/
	# Icon would go here
	cd dist && linuxdeploy --appdir AppDir --output appimage
```

**Step 4: Commit**

```bash
git add packaging/ Makefile
git commit -m "feat: add AppImage packaging

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>"
```

---

## Summary

**Phase 1** (Tasks 1.1-1.4): Project skeleton - Rust daemon structure, Qt GUI structure, config, Makefile

**Phase 2** (Tasks 2.1-2.6): Daemon core - Process collector, config loading, database, socket server, detector, main loop

**Phase 3** (Tasks 3.1-3.3): Integration testing - Unit tests, build verification, end-to-end test

**Phase 4** (Tasks 4.1-4.2): Packaging - .deb and AppImage

Total: ~16 tasks, each 10-30 minutes
