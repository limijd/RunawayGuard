use runaway_daemon::config::Config;
use std::io::Write;
use tempfile::NamedTempFile;

#[test]
fn test_default_config() {
    let config = Config::default();
    assert!(config.general.autostart);
    assert_eq!(config.general.sample_interval_normal, 10);
    assert_eq!(config.detection.cpu.threshold_percent, 90);
}

#[test]
fn test_load_from_toml() {
    let toml_content = r#"
[general]
autostart = false
sample_interval_normal = 5
sample_interval_alert = 1
notification_method = "system"

[detection.cpu]
enabled = true
threshold_percent = 80
duration_seconds = 30

[detection.hang]
enabled = false
duration_seconds = 60

[detection.memory]
enabled = true
growth_mb = 1000
window_minutes = 10

[detection.timeout]
enabled = false

[learning]
enabled = false
min_history_days = 14
suggest_whitelist = false
"#;
    let mut file = NamedTempFile::new().unwrap();
    file.write_all(toml_content.as_bytes()).unwrap();
    let config = Config::load(file.path()).unwrap();
    assert!(!config.general.autostart);
    assert_eq!(config.general.sample_interval_normal, 5);
    assert_eq!(config.detection.cpu.threshold_percent, 80);
    assert!(!config.detection.hang.enabled);
}

#[test]
fn test_save_config() {
    let config = Config::default();
    let file = NamedTempFile::new().unwrap();
    config.save(file.path()).unwrap();
    let loaded = Config::load(file.path()).unwrap();
    assert_eq!(loaded.general.autostart, config.general.autostart);
}
