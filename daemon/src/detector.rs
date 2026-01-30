//! Anomaly detection engine

use crate::collector::ProcessInfo;
use crate::config::DetectionConfig;
use std::collections::HashMap;
use std::time::{SystemTime, UNIX_EPOCH};

#[derive(Debug, Clone, PartialEq)]
pub enum AlertReason {
    CpuHigh,
    Hang,
    MemoryLeak,
    Timeout,
}

#[derive(Debug, Clone, PartialEq)]
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

/// Tracks process state over time for anomaly detection
#[derive(Debug, Clone)]
struct ProcessHistory {
    first_seen: u64,
    last_state: char,
    state_unchanged_since: u64,
    memory_samples: Vec<(u64, f64)>, // (timestamp, memory_mb)
    cpu_high_since: Option<u64>,
}

/// Main anomaly detector combining all detection modes
pub struct AnomalyDetector {
    config: DetectionConfig,
    history: HashMap<u32, ProcessHistory>,
    whitelisted_names: Vec<String>,
}

impl AnomalyDetector {
    pub fn new(config: DetectionConfig) -> Self {
        Self {
            config,
            history: HashMap::new(),
            whitelisted_names: Vec::new(),
        }
    }

    pub fn add_whitelist(&mut self, name: String) {
        if !self.whitelisted_names.contains(&name) {
            self.whitelisted_names.push(name);
        }
    }

    pub fn is_whitelisted(&self, name: &str) -> bool {
        self.whitelisted_names.iter().any(|w| name.contains(w))
    }

    fn now() -> u64 {
        SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .map(|d| d.as_secs())
            .unwrap_or(0)
    }

    fn check_cpu(&mut self, process: &ProcessInfo, history: &mut ProcessHistory) -> Option<Alert> {
        if !self.config.cpu.enabled {
            return None;
        }

        let threshold = self.config.cpu.threshold_percent as f64;
        let duration = self.config.cpu.duration_seconds;
        let now = Self::now();

        if process.cpu_percent >= threshold {
            let high_since = history.cpu_high_since.get_or_insert(now);
            if now - *high_since >= duration {
                return Some(Alert {
                    pid: process.pid,
                    name: process.name.clone(),
                    cmdline: process.cmdline.clone(),
                    reason: AlertReason::CpuHigh,
                    severity: if now - *high_since >= duration * 2 {
                        Severity::Critical
                    } else {
                        Severity::Warning
                    },
                    timestamp: now,
                });
            }
        } else {
            history.cpu_high_since = None;
        }
        None
    }

    fn check_hang(&self, process: &ProcessInfo, history: &ProcessHistory) -> Option<Alert> {
        if !self.config.hang.enabled {
            return None;
        }

        // Process is hanging if in D (uninterruptible sleep) or T (stopped) state
        let hang_states = ['D', 'T'];
        if hang_states.contains(&process.state) {
            let now = Self::now();
            let duration = now - history.state_unchanged_since;
            if duration >= self.config.hang.duration_seconds {
                return Some(Alert {
                    pid: process.pid,
                    name: process.name.clone(),
                    cmdline: process.cmdline.clone(),
                    reason: AlertReason::Hang,
                    severity: if duration >= self.config.hang.duration_seconds * 2 {
                        Severity::Critical
                    } else {
                        Severity::Warning
                    },
                    timestamp: now,
                });
            }
        }
        None
    }

    fn check_memory_leak(&self, process: &ProcessInfo, history: &ProcessHistory) -> Option<Alert> {
        if !self.config.memory.enabled {
            return None;
        }

        let window_seconds = self.config.memory.window_minutes * 60;
        let now = Self::now();
        let cutoff = now.saturating_sub(window_seconds);

        // Find oldest sample within window
        let samples_in_window: Vec<_> = history
            .memory_samples
            .iter()
            .filter(|(ts, _)| *ts >= cutoff)
            .collect();

        if samples_in_window.len() < 2 {
            return None;
        }

        let oldest = samples_in_window.first().map(|(_, m)| *m).unwrap_or(0.0);
        let growth = process.memory_mb - oldest;

        if growth >= self.config.memory.growth_mb as f64 {
            return Some(Alert {
                pid: process.pid,
                name: process.name.clone(),
                cmdline: process.cmdline.clone(),
                reason: AlertReason::MemoryLeak,
                severity: if growth >= (self.config.memory.growth_mb * 2) as f64 {
                    Severity::Critical
                } else {
                    Severity::Warning
                },
                timestamp: now,
            });
        }
        None
    }

    /// Clean up history for processes that no longer exist
    pub fn cleanup(&mut self, active_pids: &[u32]) {
        self.history.retain(|pid, _| active_pids.contains(pid));
    }
}

impl Detector for AnomalyDetector {
    fn check(&mut self, process: &ProcessInfo) -> Option<Alert> {
        if self.is_whitelisted(&process.name) {
            return None;
        }

        let now = Self::now();

        let history = self.history.entry(process.pid).or_insert_with(|| ProcessHistory {
            first_seen: now,
            last_state: process.state,
            state_unchanged_since: now,
            memory_samples: Vec::new(),
            cpu_high_since: None,
        });

        // Update state tracking
        if history.last_state != process.state {
            history.last_state = process.state;
            history.state_unchanged_since = now;
        }

        // Add memory sample (keep last 60 samples)
        history.memory_samples.push((now, process.memory_mb));
        if history.memory_samples.len() > 60 {
            history.memory_samples.remove(0);
        }

        // Clone history for checks that need immutable access
        let history_clone = history.clone();

        // Run all checks, return first alert found
        if let Some(alert) = self.check_cpu(process, history) {
            return Some(alert);
        }
        if let Some(alert) = self.check_hang(process, &history_clone) {
            return Some(alert);
        }
        if let Some(alert) = self.check_memory_leak(process, &history_clone) {
            return Some(alert);
        }

        None
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::config::{CpuDetectionConfig, HangDetectionConfig, MemoryDetectionConfig, TimeoutDetectionConfig};

    fn test_config() -> DetectionConfig {
        DetectionConfig {
            cpu: CpuDetectionConfig {
                enabled: true,
                threshold_percent: 90,
                duration_seconds: 2,
            },
            hang: HangDetectionConfig {
                enabled: true,
                duration_seconds: 2,
            },
            memory: MemoryDetectionConfig {
                enabled: true,
                growth_mb: 100,
                window_minutes: 1,
            },
            timeout: TimeoutDetectionConfig { enabled: true },
        }
    }

    fn test_process(pid: u32) -> ProcessInfo {
        ProcessInfo {
            pid,
            name: "test".to_string(),
            cmdline: "/usr/bin/test".to_string(),
            cpu_percent: 10.0,
            memory_mb: 50.0,
            runtime_seconds: 100,
            state: 'S',
            start_time: 0,
        }
    }

    #[test]
    fn test_normal_process_no_alert() {
        let mut detector = AnomalyDetector::new(test_config());
        let process = test_process(1);
        let alert = detector.check(&process);
        assert!(alert.is_none());
    }

    #[test]
    fn test_whitelisted_process_no_alert() {
        let mut detector = AnomalyDetector::new(test_config());
        detector.add_whitelist("test".to_string());
        let mut process = test_process(1);
        process.cpu_percent = 100.0;
        let alert = detector.check(&process);
        assert!(alert.is_none());
    }

    #[test]
    fn test_hanging_process_alert() {
        let mut detector = AnomalyDetector::new(test_config());
        let mut process = test_process(1);
        process.state = 'D'; // Uninterruptible sleep

        // First check initializes history
        detector.check(&process);

        // Manually set state_unchanged_since to simulate time passing
        if let Some(h) = detector.history.get_mut(&1) {
            h.state_unchanged_since = AnomalyDetector::now() - 10;
        }

        let alert = detector.check(&process);
        assert!(alert.is_some());
        let a = alert.unwrap();
        assert_eq!(a.reason, AlertReason::Hang);
    }
}
