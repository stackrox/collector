use std::time::Duration;

use anyhow::{Context, Result};
use tokio::sync::mpsc;
use tokio_util::sync::CancellationToken;
use tonic::transport::Channel;
use tracing::{debug, info, warn};

use crate::process_handler::SignalSender;
use crate::proto::sensor;
use crate::proto::sensor::signal_service_client::SignalServiceClient;

pub struct GrpcSignalClient {
    tx: mpsc::Sender<sensor::SignalStreamMessage>,
}

impl GrpcSignalClient {
    pub fn spawn(
        endpoint: String,
        hostname: String,
        instance_id: String,
        cancel: CancellationToken,
    ) -> Self {
        let (tx, rx) = mpsc::channel::<sensor::SignalStreamMessage>(1024);

        tokio::spawn(signal_stream_loop(
            endpoint,
            hostname,
            instance_id,
            rx,
            cancel,
        ));

        Self { tx }
    }
}

#[async_trait::async_trait]
impl SignalSender for GrpcSignalClient {
    async fn send(&self, msg: sensor::SignalStreamMessage) -> Result<()> {
        self.tx
            .send(msg)
            .await
            .map_err(|_| anyhow::anyhow!("signal channel closed"))
    }
}

async fn signal_stream_loop(
    endpoint: String,
    hostname: String,
    instance_id: String,
    mut rx: mpsc::Receiver<sensor::SignalStreamMessage>,
    cancel: CancellationToken,
) {
    loop {
        if cancel.is_cancelled() {
            break;
        }

        info!("connecting to sensor signal service at {}", endpoint);

        match connect_and_stream(
            &endpoint,
            &hostname,
            &instance_id,
            &mut rx,
            &cancel,
        )
        .await
        {
            Ok(()) => {
                debug!("signal stream ended normally");
                break;
            }
            Err(e) => {
                warn!("signal stream error: {}, reconnecting in 5s", e);
                tokio::select! {
                    _ = cancel.cancelled() => break,
                    _ = tokio::time::sleep(Duration::from_secs(5)) => {}
                }
            }
        }
    }
    debug!("signal stream loop stopped");
}

async fn connect_and_stream(
    endpoint: &str,
    hostname: &str,
    instance_id: &str,
    rx: &mut mpsc::Receiver<sensor::SignalStreamMessage>,
    cancel: &CancellationToken,
) -> Result<()> {
    let channel = Channel::from_shared(endpoint.to_string())
        .context("invalid endpoint")?
        .connect()
        .await
        .context("failed to connect")?;

    let mut client = SignalServiceClient::new(channel);

    let (stream_tx, stream_rx) = mpsc::channel::<sensor::SignalStreamMessage>(256);

    // Send registration message first
    let register_msg = sensor::SignalStreamMessage {
        msg: Some(sensor::signal_stream_message::Msg::CollectorRegisterRequest(
            sensor::CollectorRegisterRequest {
                hostname: hostname.to_string(),
                instance_id: instance_id.to_string(),
            },
        )),
    };
    stream_tx
        .send(register_msg)
        .await
        .context("failed to send register message")?;

    let stream = tokio_stream::wrappers::ReceiverStream::new(stream_rx);

    let response = client
        .push_signals(stream)
        .await
        .context("failed to open signal stream")?;

    // Spawn a task to drain the response stream (server doesn't send anything useful)
    let mut response_stream = response.into_inner();
    let cancel_clone = cancel.clone();
    tokio::spawn(async move {
        loop {
            tokio::select! {
                _ = cancel_clone.cancelled() => break,
                msg = response_stream.message() => {
                    match msg {
                        Ok(Some(_)) => {},
                        Ok(None) => break,
                        Err(e) => {
                            debug!("signal response stream error: {}", e);
                            break;
                        }
                    }
                }
            }
        }
    });

    // Forward messages from rx to the stream
    loop {
        tokio::select! {
            _ = cancel.cancelled() => return Ok(()),
            msg = rx.recv() => {
                match msg {
                    Some(signal_msg) => {
                        if stream_tx.send(signal_msg).await.is_err() {
                            return Err(anyhow::anyhow!("stream sender closed"));
                        }
                    }
                    None => return Ok(()),
                }
            }
        }
    }
}

pub struct StdoutSignalSender;

#[async_trait::async_trait]
impl SignalSender for StdoutSignalSender {
    async fn send(&self, msg: sensor::SignalStreamMessage) -> Result<()> {
        info!("signal: {:?}", msg);
        Ok(())
    }
}
