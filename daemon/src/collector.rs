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
