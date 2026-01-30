use runaway_daemon::collector::{ProcessCollector, LinuxProcessCollector};

#[test]
fn test_list_processes_returns_current_process() {
    let collector = LinuxProcessCollector::new();
    let processes = collector.list_processes();
    let current_pid = std::process::id();
    let found = processes.iter().any(|p| p.pid == current_pid);
    assert!(found, "Current process should be in the list");
}

#[test]
fn test_get_process_returns_current_process() {
    let collector = LinuxProcessCollector::new();
    let current_pid = std::process::id();
    let process = collector.get_process(current_pid);
    assert!(process.is_some(), "Should find current process");
    let p = process.unwrap();
    assert_eq!(p.pid, current_pid);
    assert!(!p.name.is_empty());
}

#[test]
fn test_get_process_returns_none_for_invalid_pid() {
    let collector = LinuxProcessCollector::new();
    let process = collector.get_process(999999999);
    assert!(process.is_none());
}
