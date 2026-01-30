//! Configuration management (TOML)

use serde::{Deserialize, Serialize};
use std::fs;
use std::path::Path;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Config {
    pub general: GeneralConfig,
    pub detection: DetectionConfig,
    pub learning: LearningConfig,
    #[serde(default)]
    pub rules: Vec<Rule>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GeneralConfig {
    pub autostart: bool,
    pub sample_interval_normal: u64,
    pub sample_interval_alert: u64,
    pub notification_method: String,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DetectionConfig {
    pub cpu: CpuDetectionConfig,
    pub hang: HangDetectionConfig,
    pub memory: MemoryDetectionConfig,
    pub timeout: TimeoutDetectionConfig,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CpuDetectionConfig {
    pub enabled: bool,
    pub threshold_percent: u8,
    pub duration_seconds: u64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HangDetectionConfig {
    pub enabled: bool,
    pub duration_seconds: u64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MemoryDetectionConfig {
    pub enabled: bool,
    pub growth_mb: u64,
    pub window_minutes: u64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TimeoutDetectionConfig {
    pub enabled: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LearningConfig {
    pub enabled: bool,
    pub min_history_days: u64,
    pub suggest_whitelist: bool,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Rule {
    pub name: String,
    #[serde(rename = "match")]
    pub pattern: String,
    pub match_type: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub max_runtime_minutes: Option<u64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub action: Option<String>,
}

impl Default for Config {
    fn default() -> Self {
        Config {
            general: GeneralConfig {
                autostart: true,
                sample_interval_normal: 10,
                sample_interval_alert: 2,
                notification_method: "both".to_string(),
            },
            detection: DetectionConfig {
                cpu: CpuDetectionConfig {
                    enabled: true,
                    threshold_percent: 90,
                    duration_seconds: 60,
                },
                hang: HangDetectionConfig {
                    enabled: true,
                    duration_seconds: 30,
                },
                memory: MemoryDetectionConfig {
                    enabled: true,
                    growth_mb: 500,
                    window_minutes: 5,
                },
                timeout: TimeoutDetectionConfig { enabled: true },
            },
            learning: LearningConfig {
                enabled: true,
                min_history_days: 7,
                suggest_whitelist: true,
            },
            rules: vec![],
        }
    }
}

impl Config {
    pub fn load(path: &Path) -> Result<Self, Box<dyn std::error::Error>> {
        let content = fs::read_to_string(path)?;
        let config: Config = toml::from_str(&content)?;
        Ok(config)
    }

    pub fn save(&self, path: &Path) -> Result<(), Box<dyn std::error::Error>> {
        let content = toml::to_string_pretty(self)?;
        if let Some(parent) = path.parent() {
            fs::create_dir_all(parent)?;
        }
        fs::write(path, content)?;
        Ok(())
    }

    pub fn config_path() -> std::path::PathBuf {
        directories::ProjectDirs::from("", "", "runaway-guard")
            .map(|dirs| dirs.config_dir().join("config.toml"))
            .unwrap_or_else(|| std::path::PathBuf::from("config.toml"))
    }
}
