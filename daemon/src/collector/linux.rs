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
        Self { page_size, clock_ticks, boot_time }
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
        let start_time_ticks: u64 = stat_parts[21].parse().unwrap_or(0);
        let rss_pages: u64 = stat_parts[23].parse().unwrap_or(0);

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
            cpu_percent: 0.0, // TODO: Implement with sampling history
            memory_mb, runtime_seconds, state, start_time,
        })
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
        processes
    }

    fn get_process(&self, pid: u32) -> Option<ProcessInfo> {
        self.parse_process(pid)
    }
}
