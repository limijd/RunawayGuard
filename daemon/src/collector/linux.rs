use super::{ProcessCollector, ProcessInfo};
use std::collections::HashMap;
use std::fs;
use std::path::Path;
use std::sync::Mutex;
use std::time::{Instant, SystemTime, UNIX_EPOCH};

#[derive(Clone)]
struct CpuSample {
    total_ticks: u64,  // utime + stime
    timestamp: Instant,
}

pub struct LinuxProcessCollector {
    page_size: u64,
    clock_ticks: u64,
    boot_time: u64,
    num_cpus: u64,
    cpu_samples: Mutex<HashMap<u32, CpuSample>>,
}

impl LinuxProcessCollector {
    pub fn new() -> Self {
        let page_size = unsafe { libc::sysconf(libc::_SC_PAGESIZE) as u64 };
        let clock_ticks = unsafe { libc::sysconf(libc::_SC_CLK_TCK) as u64 };
        let num_cpus = unsafe { libc::sysconf(libc::_SC_NPROCESSORS_ONLN) as u64 }.max(1);
        let boot_time = Self::get_boot_time();
        Self {
            page_size,
            clock_ticks,
            boot_time,
            num_cpus,
            cpu_samples: Mutex::new(HashMap::new()),
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
        if !proc_dir.exists() { return None; }

        let stat_content = fs::read_to_string(proc_dir.join("stat")).ok()?;
        let stat_parts: Vec<&str> = stat_content.split_whitespace().collect();
        if stat_parts.len() < 24 { return None; }

        let name = stat_parts[1].trim_matches(|c| c == '(' || c == ')').to_string();
        let state = stat_parts[2].chars().next().unwrap_or('?');
        let utime: u64 = stat_parts[13].parse().unwrap_or(0);
        let stime: u64 = stat_parts[14].parse().unwrap_or(0);
        let start_time_ticks: u64 = stat_parts[21].parse().unwrap_or(0);
        let rss_pages: u64 = stat_parts[23].parse().unwrap_or(0);

        let total_ticks = utime + stime;
        let now_instant = Instant::now();

        // Calculate CPU percentage from previous sample
        let cpu_percent = {
            let mut samples = self.cpu_samples.lock().unwrap();
            let percent = if let Some(prev) = samples.get(&pid) {
                let tick_delta = total_ticks.saturating_sub(prev.total_ticks);
                let time_delta = now_instant.duration_since(prev.timestamp).as_secs_f64();
                if time_delta > 0.0 {
                    // Convert ticks to seconds, then to percentage
                    let cpu_seconds = tick_delta as f64 / self.clock_ticks as f64;
                    (cpu_seconds / time_delta) * 100.0
                } else {
                    0.0
                }
            } else {
                0.0 // First sample, no previous data
            };
            // Update sample for next calculation
            samples.insert(pid, CpuSample { total_ticks, timestamp: now_instant });
            percent
        };

        let memory_mb = (rss_pages * self.page_size) as f64 / (1024.0 * 1024.0);
        let start_time = self.boot_time + (start_time_ticks / self.clock_ticks);
        let now = SystemTime::now().duration_since(UNIX_EPOCH).map(|d| d.as_secs()).unwrap_or(0);
        let runtime_seconds = now.saturating_sub(start_time);

        let cmdline = fs::read_to_string(proc_dir.join("cmdline"))
            .unwrap_or_default()
            .replace('\0', " ")
            .trim()
            .to_string();

        Some(ProcessInfo {
            pid, name, cmdline,
            cpu_percent,
            memory_mb, runtime_seconds, state, start_time,
        })
    }

    /// Remove stale CPU samples for processes that no longer exist
    pub fn cleanup_stale(&self, active_pids: &[u32]) {
        let mut samples = self.cpu_samples.lock().unwrap();
        samples.retain(|pid, _| active_pids.contains(pid));
    }
}

impl Default for LinuxProcessCollector {
    fn default() -> Self { Self::new() }
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
        // Cleanup stale samples
        let pids: Vec<u32> = processes.iter().map(|p| p.pid).collect();
        self.cleanup_stale(&pids);
        processes
    }

    fn get_process(&self, pid: u32) -> Option<ProcessInfo> {
        self.parse_process(pid)
    }
}
