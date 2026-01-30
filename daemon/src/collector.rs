//! Process information collector

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
