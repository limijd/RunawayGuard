//! Unix socket server for IPC

use crate::protocol::{Request, Response};
use std::path::{Path, PathBuf};
use std::sync::Arc;
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};
use tokio::net::{UnixListener, UnixStream};
use tokio::sync::broadcast;
use tracing::{error, info, warn};

pub struct SocketServer {
    path: PathBuf,
    listener: UnixListener,
    broadcast_tx: broadcast::Sender<String>,
}

impl SocketServer {
    pub async fn bind(path: &Path) -> std::io::Result<Self> {
        let _ = std::fs::remove_file(path);
        if let Some(parent) = path.parent() {
            std::fs::create_dir_all(parent)?;
        }
        let listener = UnixListener::bind(path)?;
        let (broadcast_tx, _) = broadcast::channel(100);
        info!("Socket server listening on {:?}", path);
        Ok(Self { path: path.to_path_buf(), listener, broadcast_tx })
    }

    pub fn broadcast_sender(&self) -> broadcast::Sender<String> {
        self.broadcast_tx.clone()
    }

    pub async fn accept(&self) -> std::io::Result<UnixStream> {
        let (stream, _) = self.listener.accept().await?;
        Ok(stream)
    }

    pub fn socket_path() -> PathBuf {
        let uid = unsafe { libc::getuid() };
        PathBuf::from(format!("/run/user/{}/runaway-guard.sock", uid))
    }
}

impl Drop for SocketServer {
    fn drop(&mut self) {
        let _ = std::fs::remove_file(&self.path);
    }
}

pub async fn handle_client<H>(
    stream: UnixStream,
    mut broadcast_rx: broadcast::Receiver<String>,
    handler: Arc<H>,
) where
    H: RequestHandler + Send + Sync + 'static,
{
    let (reader, mut writer) = stream.into_split();
    let mut reader = BufReader::new(reader);
    let mut line = String::new();

    loop {
        tokio::select! {
            result = reader.read_line(&mut line) => {
                match result {
                    Ok(0) => break,
                    Ok(_) => {
                        let response = match serde_json::from_str::<Request>(&line) {
                            Ok(request) => handler.handle(request).await,
                            Err(e) => {
                                warn!("Invalid request: {}", e);
                                Response::Response {
                                    id: None,
                                    data: serde_json::json!({"error": e.to_string()}),
                                }
                            }
                        };
                        let json = serde_json::to_string(&response).unwrap() + "\n";
                        if let Err(e) = writer.write_all(json.as_bytes()).await {
                            error!("Failed to write response: {}", e);
                            break;
                        }
                        line.clear();
                    }
                    Err(e) => {
                        error!("Read error: {}", e);
                        break;
                    }
                }
            }
            result = broadcast_rx.recv() => {
                if let Ok(msg) = result {
                    if let Err(e) = writer.write_all((msg + "\n").as_bytes()).await {
                        error!("Failed to broadcast: {}", e);
                        break;
                    }
                }
            }
        }
    }
}

#[async_trait::async_trait]
pub trait RequestHandler {
    async fn handle(&self, request: Request) -> Response;
}
