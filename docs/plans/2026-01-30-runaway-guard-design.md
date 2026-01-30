# RunawayGuard 设计文档

**日期**: 2026-01-30
**状态**: 待实现
**作者**: wli

## 概述

RunawayGuard 是一个 Linux/macOS 进程监控工具，用于检测和处理"跑飞"的进程（CPU 持续高占用、卡死、内存泄漏、运行超时）。程序以系统托盘形式常驻，检测到异常时通知用户并提供处理选项。

**目标平台**: Ubuntu 24.04 (优先)，macOS (第二阶段)
**开源协议**: MIT 或 Apache-2.0
**核心要求**: 高性能、无内存泄漏、与系统 UI 风格一致

---

## 1. 整体架构

```
┌─────────────────────────────────────────────────────────────┐
│                         用户                                 │
└─────────────────────────────────────────────────────────────┘
                              │
              ┌───────────────┴───────────────┐
              ▼                               ▼
┌─────────────────────────┐     ┌─────────────────────────────┐
│   runaway-gui (Qt C++)  │     │  系统通知 (libnotify/Mac)   │
│   - 系统托盘 (状态颜色)  │     └─────────────────────────────┘
│   - 标签页界面           │                   ▲
│   - 跟随系统主题         │                   │
└───────────┬─────────────┘                   │
            │ Unix Socket + JSON              │
            ▼                                 │
┌─────────────────────────────────────────────┴───────────────┐
│                  runaway-daemon (Rust)                       │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────────┐│
│  │ 进程采集器   │ │ 异常检测器   │ │ 智能学习引擎            ││
│  └─────────────┘ └─────────────┘ └─────────────────────────┘│
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────────┐│
│  │ 动作执行器   │ │ 配置管理器   │ │ SQLite 存储             ││
│  └─────────────┘ └─────────────┘ └─────────────────────────┘│
└─────────────────────────────────────────────────────────────┘
```

### 1.1 组件说明

| 组件 | 语言 | 职责 |
|------|------|------|
| `runaway-daemon` | Rust | 进程监控、异常检测、智能学习、数据存储、系统通知 |
| `runaway-gui` | C++ (Qt 6) | 用户界面、系统托盘、配置编辑 |

### 1.2 文件位置 (XDG 规范)

| 用途 | Linux 路径 | macOS 路径 |
|------|------------|------------|
| 配置文件 | `~/.config/runaway-guard/config.toml` | `~/Library/Application Support/runaway-guard/config.toml` |
| 数据库 | `~/.local/share/runaway-guard/history.db` | `~/Library/Application Support/runaway-guard/history.db` |
| Socket | `/run/user/{uid}/runaway-guard.sock` | `~/Library/Application Support/runaway-guard/runaway-guard.sock` |

---

## 2. GUI 界面设计

### 2.1 设计原则

- **跟随系统主题**: 使用 Qt 原生样式，自动适配浅色/深色模式
- **使用系统字体**: 不自定义字体，保证与桌面环境一致
- **HiDPI 支持**: 支持 4K、Retina 及混合屏幕配置
- **布局响应式**: 使用 QLayout，不硬编码像素值

### 2.2 HiDPI 支持要求

```cpp
// main.cpp 启用 HiDPI
QApplication::setHighDpiScaleFactorRoundingPolicy(
    Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
```

- 字体使用 `QFont::setPointSize()` 而非 `setPixelSize()`
- 图标使用 SVG 格式或提供 @1x/@2x/@3x 多分辨率 PNG
- 测试矩阵: 100% 缩放、150% 缩放、200% 缩放、混合屏幕

### 2.3 主窗口布局

```
┌─────────────────────────────────────────────────────────────┐
│  RunawayGuard                                    [—][□][×]  │
├─────────────────────────────────────────────────────────────┤
│  ┌──────────┬──────────┬──────────┬──────────┐              │
│  │ 监控     │ 告警     │ 白名单   │ 设置     │              │
│  └──────────┴──────────┴──────────┴──────────┘              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│                      [标签页内容区]                          │
│                                                             │
└─────────────────────────────────────────────────────────────┘
│  状态栏: 守护进程状态 | 监控进程数 | 上次采样时间            │
└─────────────────────────────────────────────────────────────┘
```

默认窗口尺寸: 800x600

### 2.4 标签页功能

| 标签页 | 功能 |
|--------|------|
| 监控 | 当前监控进程表格 (PID、名称、CPU%、内存、运行时间、状态) |
| 告警 | 历史告警列表，按时间/严重程度筛选，支持批量处理 |
| 白名单 | 已忽略的进程/规则，支持添加/删除/编辑 |
| 设置 | 检测阈值、监控间隔、通知方式、开机启动等配置 |

### 2.5 托盘图标

**状态指示**:
- 绿色: 一切正常
- 黄色: 有待处理的警告
- 红色: 有严重问题需立即处理

**右键菜单**:
- 最近 3 条告警快捷入口
- 打开主窗口
- 暂停/恢复监控
- 退出

---

## 3. 检测引擎

### 3.1 检测模式

| 检测类型 | 默认阈值 | 判定逻辑 |
|----------|----------|----------|
| CPU 持续高占用 | >90% 持续 60 秒 | 连续 N 次采样都超阈值 |
| 进程卡死/无响应 | CPU=0% 且状态为 D/T 持续 30 秒 | 排除正常睡眠进程 |
| 内存持续增长 | 5 分钟内增长 >500MB 且无下降趋势 | 线性回归检测单调增长 |
| 运行超时 | 用户自定义 | 按进程名/命令行匹配规则 |

### 3.2 自适应采样

```
正常状态: 每 10 秒采样
发现可疑: 切换到每 2 秒采样该进程
连续 5 次正常: 恢复 10 秒间隔
```

### 3.3 智能学习

- 记录每个进程名的历史 CPU/内存分布
- 超过 7 天数据后，自动计算"正常范围"
- 进程超出自身历史 P95 值时触发告警
- 定期建议: "Chrome 经常高 CPU，是否加入白名单？"

### 3.4 用户操作

检测到异常后，用户可执行:

| 操作 | 说明 |
|------|------|
| 忽略本次 | 不处理，本次告警标记为已处理 |
| 加入白名单 | 以后不再对该进程告警 |
| 终止进程 (SIGTERM) | 优雅终止 |
| 强制杀死 (SIGKILL) | 强制终止 |
| 暂停进程 (SIGSTOP) | 暂停执行，可稍后恢复 |
| 调整优先级 (nice) | 降低进程优先级 |

---

## 4. 通信协议

### 4.1 传输层

- Unix Domain Socket
- 路径: `/run/user/{uid}/runaway-guard.sock`
- 消息格式: JSON，以换行符 `\n` 分隔

### 4.2 GUI → Daemon 请求

```json
{"cmd": "list_processes"}
{"cmd": "get_alerts", "params": {"limit": 50, "since": "2024-01-01T00:00:00Z"}}
{"cmd": "kill_process", "params": {"pid": 12345, "signal": "TERM"}}
{"cmd": "add_whitelist", "params": {"pattern": "chrome", "match_type": "name"}}
{"cmd": "update_config", "params": {"cpu_threshold": 85}}
```

### 4.3 Daemon → GUI 响应/推送

```json
{"type": "response", "id": "xxx", "data": {...}}
{"type": "alert", "data": {"pid": 1234, "name": "make", "reason": "cpu_high", "severity": "warning"}}
{"type": "status", "data": {"monitored_count": 156, "alert_count": 2}}
```

### 4.4 连接管理

- GUI 启动时连接，断开自动重连
- Daemon 支持多个 GUI 同时连接 (广播告警)
- 心跳: 每 30 秒 `{"cmd": "ping"}` / `{"type": "pong"}`

---

## 5. 配置文件

**路径**: `~/.config/runaway-guard/config.toml`

```toml
[general]
autostart = true              # 开机启动
sample_interval_normal = 10   # 正常采样间隔 (秒)
sample_interval_alert = 2     # 警戒采样间隔 (秒)
notification_method = "both"  # "system", "popup", "both"

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

[[rules]]
name = "make-timeout"
match = "make"
match_type = "name"           # "name", "cmdline", "regex"
max_runtime_minutes = 120

[[rules]]
name = "ignore-vscode"
match = "code"
match_type = "name"
action = "whitelist"

[learning]
enabled = true
min_history_days = 7
suggest_whitelist = true
```

配置变更时 Daemon 自动热重载。

---

## 6. 数据库结构

**路径**: `~/.local/share/runaway-guard/history.db`

```sql
-- 进程采样历史 (智能学习)
CREATE TABLE process_samples (
    id INTEGER PRIMARY KEY,
    timestamp INTEGER NOT NULL,
    name TEXT NOT NULL,
    pid INTEGER NOT NULL,
    cpu_percent REAL,
    memory_mb REAL,
    runtime_seconds INTEGER
);
CREATE INDEX idx_samples_name_time ON process_samples(name, timestamp);

-- 告警记录
CREATE TABLE alerts (
    id INTEGER PRIMARY KEY,
    timestamp INTEGER NOT NULL,
    pid INTEGER NOT NULL,
    name TEXT NOT NULL,
    cmdline TEXT,
    reason TEXT NOT NULL,            -- cpu_high, hang, memory_leak, timeout
    severity TEXT NOT NULL,          -- warning, critical
    resolved INTEGER DEFAULT 0,
    action_taken TEXT
);
CREATE INDEX idx_alerts_time ON alerts(timestamp DESC);

-- 进程统计 (智能学习)
CREATE TABLE process_stats (
    name TEXT PRIMARY KEY,
    sample_count INTEGER,
    cpu_avg REAL,
    cpu_p95 REAL,
    memory_avg REAL,
    memory_p95 REAL,
    last_updated INTEGER
);

-- 白名单
CREATE TABLE whitelist (
    id INTEGER PRIMARY KEY,
    pattern TEXT NOT NULL,
    match_type TEXT NOT NULL,
    reason TEXT,
    created_at INTEGER
);
```

**数据清理策略**:
- `process_samples`: 保留 30 天
- `alerts`: 保留 90 天

---

## 7. 项目结构

```
runaway-guard/
├── daemon/                     # Rust 守护进程
│   ├── Cargo.toml
│   └── src/
│       ├── main.rs
│       ├── collector.rs        # 进程采集 (/proc)
│       ├── detector.rs         # 异常检测引擎
│       ├── learner.rs          # 智能学习模块
│       ├── executor.rs         # 动作执行 (kill/nice)
│       ├── config.rs           # 配置管理
│       ├── db.rs               # SQLite 操作
│       ├── socket.rs           # Unix Socket 服务
│       └── notifier.rs         # 系统通知
│
├── gui/                        # Qt C++ 界面
│   ├── CMakeLists.txt
│   ├── src/
│   │   ├── main.cpp
│   │   ├── MainWindow.cpp/h
│   │   ├── TrayIcon.cpp/h
│   │   ├── ProcessTab.cpp/h
│   │   ├── AlertTab.cpp/h
│   │   ├── WhitelistTab.cpp/h
│   │   ├── SettingsTab.cpp/h
│   │   └── DaemonClient.cpp/h
│   └── resources/
│       └── icons/              # SVG 图标
│
├── packaging/
│   ├── deb/
│   │   ├── control
│   │   ├── postinst
│   │   └── runaway-guard.install
│   └── appimage/
│       ├── AppRun
│       ├── runaway-guard.desktop
│       └── runaway-guard.appdata.xml
│
├── config/
│   └── default.toml
├── scripts/
│   ├── install.sh
│   └── runaway-guard.service
├── Makefile
├── LICENSE
└── README.md
```

### 7.1 关键依赖

**Rust (daemon)**:
- `tokio`: 异步运行时
- `rusqlite`: SQLite
- `serde` + `serde_json`: JSON 序列化
- `toml`: 配置解析
- `notify-rust`: 系统通知

**C++ (gui)**:
- Qt 6 (Widgets, Network)
- CMake 3.16+

---

## 8. 跨平台支持

| 功能 | Linux | macOS |
|------|-------|-------|
| 进程信息 | `/proc` 文件系统 | `sysctl` / `libproc` |
| 系统通知 | libnotify (D-Bus) | NSUserNotification |
| 开机启动 | systemd user service | LaunchAgent plist |

### 8.1 Rust 抽象层

```rust
trait ProcessCollector {
    fn list_processes(&self) -> Vec<ProcessInfo>;
    fn get_process(&self, pid: u32) -> Option<ProcessInfo>;
}

#[cfg(target_os = "linux")]
mod linux;

#[cfg(target_os = "macos")]
mod macos;
```

### 8.2 实现优先级

1. **Phase 1**: Linux (Ubuntu 24.04) 完整功能
2. **Phase 2**: macOS 支持，复用 90% 代码

---

## 9. 安装与部署

### 9.1 分发格式

| 格式 | 用途 | 构建工具 |
|------|------|----------|
| 源码 | 开发者 | Makefile |
| `.deb` | Ubuntu/Debian | `cargo-deb` + `dpkg-deb` |
| AppImage | 通用 Linux | `linuxdeploy` + `appimagetool` |

### 9.2 构建命令

```bash
make build          # 编译
make deb            # 生成 .deb 包
make appimage       # 生成 AppImage
```

### 9.3 安装产物

```
/usr/local/bin/runaway-daemon
/usr/local/bin/runaway-gui
/usr/local/share/applications/runaway-guard.desktop
/usr/local/share/icons/hicolor/scalable/apps/runaway-guard.svg
```

### 9.4 systemd 用户服务

`~/.config/systemd/user/runaway-guard.service`:

```ini
[Unit]
Description=RunawayGuard Process Monitor Daemon
After=default.target

[Service]
ExecStart=/usr/local/bin/runaway-daemon
Restart=on-failure
RestartSec=5

[Install]
WantedBy=default.target
```

### 9.5 启动流程

1. 用户登录 → systemd 启动 `runaway-daemon`
2. 桌面环境加载 → autostart 启动 `runaway-gui` (最小化到托盘)

---

## 10. 后续工作

- [ ] 创建项目骨架结构
- [ ] 实现 daemon 核心监控逻辑
- [ ] 实现 GUI 基础界面
- [ ] 实现 IPC 通信
- [ ] 实现智能学习模块
- [ ] 打包 .deb 和 AppImage
- [ ] macOS 适配
