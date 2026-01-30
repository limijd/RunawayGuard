//! System notification sender

use notify_rust::Notification;

pub fn send_notification(summary: &str, body: &str) -> Result<(), notify_rust::error::Error> {
    Notification::new()
        .summary(summary)
        .body(body)
        .appname("RunawayGuard")
        .show()?;
    Ok(())
}
