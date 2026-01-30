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
