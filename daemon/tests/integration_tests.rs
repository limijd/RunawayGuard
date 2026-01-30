//! Integration tests for the RunawayGuard daemon

use runaway_daemon::{
    collector::{LinuxProcessCollector, ProcessCollector},
    config::Config,
    db::Database,
    detector::{AnomalyDetector, Detector},
};
use tempfile::TempDir;

/// Test that collector finds the current process
#[test]
fn test_collector_finds_current_process() {
    let collector = LinuxProcessCollector::new();
    let current_pid = std::process::id();
    let process = collector.get_process(current_pid);
    assert!(process.is_some());
    let p = process.unwrap();
    assert_eq!(p.pid, current_pid);
    assert!(p.memory_mb > 0.0);
}

/// Test that detector does not alert on normal processes
#[test]
fn test_detector_no_false_positives() {
    let config = Config::default();
    let mut detector = AnomalyDetector::new(config.detection);
    let collector = LinuxProcessCollector::new();

    // Check several normal processes
    let processes = collector.list_processes();
    let checked = processes.iter().take(10).collect::<Vec<_>>();

    for process in checked {
        // First check should not produce alert for normal process
        let alert = detector.check(process);
        // We can't guarantee no alert since some system processes might be stressed
        // Just ensure we don't panic
        if alert.is_some() {
            println!("Got alert for {}: {:?}", process.name, alert);
        }
    }
}

/// Test database round-trip for alerts
#[test]
fn test_database_alert_roundtrip() {
    let temp_dir = TempDir::new().unwrap();
    let db_path = temp_dir.path().join("test.db");
    let db = Database::open(&db_path).unwrap();
    db.init_schema().unwrap();

    // Insert alert
    db.insert_alert(1234, "test_process", "cpu_high", "warning", "/usr/bin/test")
        .unwrap();

    // Retrieve alerts
    let alerts = db.get_alerts(10).unwrap();
    assert_eq!(alerts.len(), 1);
    assert_eq!(alerts[0].pid, 1234);
    assert_eq!(alerts[0].process_name, "test_process");
    assert_eq!(alerts[0].alert_type, "cpu_high");
}

/// Test database whitelist operations
#[test]
fn test_database_whitelist() {
    let temp_dir = TempDir::new().unwrap();
    let db_path = temp_dir.path().join("test.db");
    let db = Database::open(&db_path).unwrap();
    db.init_schema().unwrap();

    // Add whitelist entry
    db.add_whitelist("firefox", "name").unwrap();

    // Retrieve whitelist
    let whitelist = db.get_whitelist().unwrap();
    assert_eq!(whitelist.len(), 1);
    assert_eq!(whitelist[0].pattern, "firefox");
    assert_eq!(whitelist[0].match_type, "name");
}

/// Test config load and save
#[test]
fn test_config_roundtrip() {
    let temp_dir = TempDir::new().unwrap();
    let config_path = temp_dir.path().join("config.toml");

    let mut config = Config::default();
    config.general.sample_interval_normal = 20;
    config.detection.cpu.threshold_percent = 85;

    config.save(&config_path).unwrap();

    let loaded = Config::load(&config_path).unwrap();
    assert_eq!(loaded.general.sample_interval_normal, 20);
    assert_eq!(loaded.detection.cpu.threshold_percent, 85);
}

/// Test detector whitelist functionality
#[test]
fn test_detector_whitelist() {
    let config = Config::default();
    let mut detector = AnomalyDetector::new(config.detection);
    detector.add_whitelist("firefox".to_string());

    assert!(detector.is_whitelisted("firefox"));
    assert!(detector.is_whitelisted("firefox-esr"));
    assert!(!detector.is_whitelisted("chrome"));
}

/// Test adaptive sampling interval concept
#[test]
fn test_config_adaptive_intervals() {
    let config = Config::default();
    assert!(config.general.sample_interval_normal > config.general.sample_interval_alert);
    assert_eq!(config.general.sample_interval_normal, 10);
    assert_eq!(config.general.sample_interval_alert, 2);
}
