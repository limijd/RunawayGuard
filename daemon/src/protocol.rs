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
