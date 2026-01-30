//! Unix socket server for IPC

use std::path::Path;
use tokio::net::UnixListener;

pub struct SocketServer {
    listener: UnixListener,
}

impl SocketServer {
    pub async fn bind(path: &Path) -> std::io::Result<Self> {
        let _ = std::fs::remove_file(path);
        let listener = UnixListener::bind(path)?;
        Ok(Self { listener })
    }
}
