//! IPC protocol definitions (JSON messages)

use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "cmd", rename_all = "snake_case")]
pub enum Request {
    Ping,
    ListProcesses,
    GetAlerts { params: GetAlertsParams },
    KillProcess { params: KillProcessParams },
    ListWhitelist,
    AddWhitelist { params: AddWhitelistParams },
    RemoveWhitelist { params: RemoveWhitelistParams },
    UpdateConfig { params: serde_json::Value },
    GetConfig,
    PauseMonitoring,
    ResumeMonitoring,
    ClearAlerts,
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
pub struct RemoveWhitelistParams {
    pub id: i64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "type", rename_all = "snake_case")]
pub enum Response {
    Pong,
    Response { id: Option<String>, data: serde_json::Value },
    Alert { data: AlertData },
    Status { data: StatusData },
    Config { data: ConfigData },
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

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ConfigData {
    pub cpu_high: CpuHighConfig,
    pub hang: HangConfig,
    pub memory_leak: MemoryLeakConfig,
    pub general: GeneralConfig,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CpuHighConfig {
    pub enabled: bool,
    pub threshold_percent: u32,
    pub duration_seconds: u32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HangConfig {
    pub enabled: bool,
    pub duration_seconds: u32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MemoryLeakConfig {
    pub enabled: bool,
    pub growth_threshold_mb: u32,
    pub window_minutes: u32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GeneralConfig {
    pub sample_interval_normal: u32,
    pub sample_interval_alert: u32,
    pub notification_method: String,
}
