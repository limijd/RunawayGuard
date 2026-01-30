//! System notification sender

use notify_rust::Notification;
use tracing::warn;

pub struct Notifier {
    app_name: String,
}

impl Notifier {
    pub fn new() -> Self {
        Self {
            app_name: "RunawayGuard".to_string(),
        }
    }

    pub fn send(&self, summary: &str, body: &str) {
        if let Err(e) = Notification::new()
            .summary(summary)
            .body(body)
            .appname(&self.app_name)
            .show()
        {
            warn!("Failed to send notification: {}", e);
        }
    }
}

impl Default for Notifier {
    fn default() -> Self {
        Self::new()
    }
}

pub fn send_notification(summary: &str, body: &str) -> Result<(), notify_rust::error::Error> {
    Notification::new()
        .summary(summary)
        .body(body)
        .appname("RunawayGuard")
        .show()?;
    Ok(())
}
