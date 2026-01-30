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
