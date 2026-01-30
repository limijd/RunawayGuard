use anyhow::Result;
use runaway_daemon::{
    collector::{ProcessCollector, LinuxProcessCollector},
    config::Config,
    db::Database,
    detector::{Alert, AlertReason, AnomalyDetector, Detector, Severity},
    notifier::Notifier,
    protocol::{AlertData, Request, Response, StatusData},
    socket::{handle_client, RequestHandler, SocketServer},
};
use std::sync::Arc;
use std::time::Duration;
use tokio::sync::{broadcast, Mutex, RwLock};
use tracing::{error, info, warn};

struct DaemonState {
    collector: LinuxProcessCollector,
    detector: Mutex<AnomalyDetector>,
    db: Mutex<Database>,
    notifier: Notifier,
    config: RwLock<Config>,
    broadcast_tx: broadcast::Sender<String>,
    alert_count: Mutex<u32>,
}

impl DaemonState {
    fn new(config: Config, db: Database, broadcast_tx: broadcast::Sender<String>) -> Self {
        Self {
            collector: LinuxProcessCollector::new(),
            detector: Mutex::new(AnomalyDetector::new(config.detection.clone())),
            db: Mutex::new(db),
            notifier: Notifier::new(),
            config: RwLock::new(config),
            broadcast_tx,
            alert_count: Mutex::new(0),
        }
    }

    async fn handle_alert(&self, alert: Alert) {
        let reason_str = match alert.reason {
            AlertReason::CpuHigh => "cpu_high",
            AlertReason::Hang => "hang",
            AlertReason::MemoryLeak => "memory_leak",
            AlertReason::Timeout => "timeout",
        };
        let severity_str = match alert.severity {
            Severity::Warning => "warning",
            Severity::Critical => "critical",
        };

        // Save to database
        {
            let db = self.db.lock().await;
            if let Err(e) = db.insert_alert(
                alert.pid,
                &alert.name,
                &alert.cmdline,
                reason_str,
                severity_str,
            ) {
                error!("Failed to save alert: {}", e);
            }
        }

        // Send desktop notification
        let title = format!("RunawayGuard: {} ({})", alert.name, severity_str);
        let body = format!("PID {} - {}", alert.pid, reason_str);
        self.notifier.send(&title, &body);

        // Broadcast to connected clients
        let alert_data = AlertData {
            pid: alert.pid,
            name: alert.name,
            reason: reason_str.to_string(),
            severity: severity_str.to_string(),
        };
        let alert_response = Response::Alert { data: alert_data };
        if let Ok(json) = serde_json::to_string(&alert_response) {
            let _ = self.broadcast_tx.send(json);
        }

        // Update alert count
        *self.alert_count.lock().await += 1;
    }
}

#[async_trait::async_trait]
impl RequestHandler for DaemonState {
    async fn handle(&self, request: Request) -> Response {
        match request {
            Request::Ping => Response::Pong,

            Request::ListProcesses => {
                let processes = self.collector.list_processes();
                let data: Vec<_> = processes
                    .iter()
                    .map(|p| {
                        serde_json::json!({
                            "pid": p.pid,
                            "name": p.name,
                            "cmdline": p.cmdline,
                            "cpu_percent": p.cpu_percent,
                            "memory_mb": p.memory_mb,
                            "runtime_seconds": p.runtime_seconds,
                            "state": p.state.to_string(),
                        })
                    })
                    .collect();
                Response::Response {
                    id: None,
                    data: serde_json::json!(data),
                }
            }

            Request::GetAlerts { params } => {
                let limit = params.limit.unwrap_or(50);
                let db = self.db.lock().await;
                match db.get_alerts(limit, None) {
                    Ok(alerts) => {
                        let data: Vec<_> = alerts
                            .iter()
                            .map(|a| {
                                serde_json::json!({
                                    "id": a.id,
                                    "pid": a.pid,
                                    "name": a.name,
                                    "reason": a.reason,
                                    "severity": a.severity,
                                    "timestamp": a.timestamp,
                                })
                            })
                            .collect();
                        Response::Response {
                            id: None,
                            data: serde_json::json!(data),
                        }
                    }
                    Err(e) => Response::Response {
                        id: None,
                        data: serde_json::json!({"error": e.to_string()}),
                    },
                }
            }

            Request::KillProcess { params } => {
                let signal = match params.signal.to_uppercase().as_str() {
                    "SIGTERM" | "TERM" | "15" => libc::SIGTERM,
                    "SIGKILL" | "KILL" | "9" => libc::SIGKILL,
                    "SIGSTOP" | "STOP" | "19" => libc::SIGSTOP,
                    "SIGCONT" | "CONT" | "18" => libc::SIGCONT,
                    _ => {
                        return Response::Response {
                            id: None,
                            data: serde_json::json!({"error": "Invalid signal"}),
                        }
                    }
                };
                let result = unsafe { libc::kill(params.pid as i32, signal) };
                if result == 0 {
                    Response::Response {
                        id: None,
                        data: serde_json::json!({"success": true}),
                    }
                } else {
                    Response::Response {
                        id: None,
                        data: serde_json::json!({"error": "Failed to send signal"}),
                    }
                }
            }

            Request::AddWhitelist { params } => {
                let mut detector = self.detector.lock().await;
                detector.add_whitelist(params.pattern.clone());
                let db = self.db.lock().await;
                match db.add_whitelist(&params.pattern, &params.match_type, None) {
                    Ok(_) => Response::Response {
                        id: None,
                        data: serde_json::json!({"success": true}),
                    },
                    Err(e) => Response::Response {
                        id: None,
                        data: serde_json::json!({"error": e.to_string()}),
                    },
                }
            }

            Request::UpdateConfig { params: _ } => {
                // TODO: Implement config update
                Response::Response {
                    id: None,
                    data: serde_json::json!({"error": "Not implemented"}),
                }
            }
        }
    }
}

async fn monitoring_loop(state: Arc<DaemonState>) {
    let mut interval = tokio::time::interval(Duration::from_secs(10));
    let mut in_alert_mode = false;

    loop {
        interval.tick().await;

        let processes = state.collector.list_processes();
        let mut detector = state.detector.lock().await;
        let mut had_alert = false;

        for process in &processes {
            if let Some(alert) = detector.check(process) {
                had_alert = true;
                drop(detector); // Release lock before async operation
                state.handle_alert(alert).await;
                detector = state.detector.lock().await;
            }
        }

        // Cleanup stale process history
        let pids: Vec<u32> = processes.iter().map(|p| p.pid).collect();
        detector.cleanup(&pids);
        drop(detector);

        // Adaptive interval: faster during alerts
        let config = state.config.read().await;
        if had_alert && !in_alert_mode {
            in_alert_mode = true;
            interval = tokio::time::interval(Duration::from_secs(
                config.general.sample_interval_alert,
            ));
            info!("Entering alert mode - increased sampling rate");
        } else if !had_alert && in_alert_mode {
            in_alert_mode = false;
            interval = tokio::time::interval(Duration::from_secs(
                config.general.sample_interval_normal,
            ));
            info!("Exiting alert mode - normal sampling rate");
        }

        // Broadcast status update periodically
        let alert_count = *state.alert_count.lock().await;
        let status = Response::Status {
            data: StatusData {
                monitored_count: processes.len() as u32,
                alert_count,
            },
        };
        if let Ok(json) = serde_json::to_string(&status) {
            let _ = state.broadcast_tx.send(json);
        }
    }
}

#[tokio::main]
async fn main() -> Result<()> {
    tracing_subscriber::fmt::init();
    info!("RunawayGuard daemon starting...");

    // Load configuration
    let config_path = Config::config_path();
    let config = if config_path.exists() {
        Config::load(&config_path).unwrap_or_else(|e| {
            warn!("Failed to load config: {}, using defaults", e);
            Config::default()
        })
    } else {
        info!("No config file found, using defaults");
        Config::default()
    };

    // Initialize database
    let db = Database::open_default()?;
    db.init_schema()?;

    // Load whitelist from database
    let whitelist = db.get_whitelist()?;

    // Create socket server
    let socket_path = SocketServer::socket_path();
    let server = SocketServer::bind(&socket_path).await?;
    let broadcast_tx = server.broadcast_sender();

    // Create shared state
    let state = Arc::new(DaemonState::new(config, db, broadcast_tx));

    // Add whitelist entries to detector
    {
        let mut detector = state.detector.lock().await;
        for entry in whitelist {
            detector.add_whitelist(entry.pattern);
        }
    }

    // Start monitoring loop
    let monitor_state = Arc::clone(&state);
    tokio::spawn(async move {
        monitoring_loop(monitor_state).await;
    });

    info!("Daemon ready, listening for connections...");

    // Accept client connections
    loop {
        match server.accept().await {
            Ok(stream) => {
                let state = Arc::clone(&state);
                let broadcast_rx = server.broadcast_sender().subscribe();
                tokio::spawn(async move {
                    handle_client(stream, broadcast_rx, state).await;
                });
            }
            Err(e) => {
                error!("Failed to accept connection: {}", e);
            }
        }
    }
}
