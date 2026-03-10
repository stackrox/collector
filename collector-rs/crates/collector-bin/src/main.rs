use std::sync::{Arc, Mutex, RwLock};
use std::time::Duration;

use anyhow::Result;
use clap::Parser;
use tokio::sync::mpsc;
use tokio_util::sync::CancellationToken;
use tracing::{error, info};

use collector_bpf::BpfLoader;
use collector_core::config::{self, CliArgs};
use collector_core::conn_tracker::ConnTracker;
use collector_core::event_reader::spawn_event_reader;
use collector_core::health::build_health_router;
use collector_core::network_client::run_network_client;
use collector_core::network_handler::run_network_handler;
use collector_core::process_handler::run_process_handler;
use collector_core::process_table::ProcessTable;
use collector_core::rate_limit::RateLimitCache;
use collector_core::self_check::verify_bpf;
use collector_core::signal_client::{GrpcSignalClient, StdoutSignalSender};

#[tokio::main]
async fn main() -> Result<()> {
    let args = CliArgs::parse();

    // Initialize tracing
    tracing_subscriber::fmt()
        .with_env_filter(&args.log_level)
        .init();

    info!("collector starting");

    let cancel = CancellationToken::new();

    // Signal handler for graceful shutdown
    let cancel_signal = cancel.clone();
    tokio::spawn(async move {
        tokio::signal::ctrl_c().await.ok();
        info!("received shutdown signal");
        cancel_signal.cancel();
    });

    // Load runtime config
    let runtime_config = config::load_runtime_config(&args.config_file);
    let runtime_config = Arc::new(RwLock::new(runtime_config));

    // Start config file watcher
    let config_watcher_cancel = cancel.clone();
    let config_path = args.config_file.clone();
    let config_for_watcher = Arc::clone(&runtime_config);
    tokio::spawn(async move {
        config::watch_config_file(config_path, config_for_watcher, config_watcher_cancel).await;
    });

    // Initialize BPF
    info!("loading BPF programs");
    let loader = BpfLoader::load_and_attach()?;

    // Create channels
    let (proc_tx, mut proc_rx) = mpsc::channel(4096);
    let (net_tx, net_rx) = mpsc::channel(4096);

    // Spawn event reader on dedicated OS thread (before self-check so events flow through channels)
    let event_reader_cancel = cancel.clone();
    let event_reader_handle = spawn_event_reader(
        Box::new(loader),
        proc_tx,
        net_tx,
        event_reader_cancel,
    );

    // Self-check: reads from process channel, doesn't consume from ring buffer directly
    verify_bpf(&mut proc_rx).await?;

    // Shared state
    let afterglow = {
        let cfg = runtime_config.read().unwrap();
        if cfg.enable_afterglow {
            cfg.afterglow_period
        } else {
            Duration::from_secs(0)
        }
    };
    let conn_tracker = Arc::new(Mutex::new(ConnTracker::new(afterglow)));
    let process_table = Mutex::new(ProcessTable::new());
    let rate_limiter = Mutex::new(RateLimitCache::new());

    // Create signal sender (gRPC or stdout)
    let hostname = hostname::get()
        .map(|h| h.to_string_lossy().to_string())
        .unwrap_or_else(|_| "unknown".to_string());
    let instance_id = uuid::Uuid::new_v4().to_string();

    let signal_sender: Box<dyn collector_core::process_handler::SignalSender> =
        if let Some(ref grpc_server) = args.grpc_server {
            Box::new(GrpcSignalClient::spawn(
                grpc_server.clone(),
                hostname.clone(),
                instance_id.clone(),
                cancel.clone(),
            ))
        } else {
            info!("no GRPC_SERVER configured, logging signals to stdout");
            Box::new(StdoutSignalSender)
        };

    // Spawn process handler
    let proc_cancel = cancel.clone();
    tokio::spawn(async move {
        run_process_handler(proc_rx, signal_sender, &process_table, &rate_limiter, proc_cancel)
            .await;
    });

    // Spawn network handler
    let net_cancel = cancel.clone();
    let conn_tracker_for_handler = Arc::clone(&conn_tracker);
    tokio::spawn(async move {
        run_network_handler(net_rx, &conn_tracker_for_handler, net_cancel).await;
    });

    // Spawn network gRPC client (if configured)
    if let Some(ref grpc_server) = args.grpc_server {
        let scrape_interval = runtime_config.read().unwrap().scrape_interval;
        let conn_tracker_for_client = Arc::clone(&conn_tracker);
        let grpc = grpc_server.clone();
        let host = hostname.clone();
        let inst = instance_id.clone();
        let net_client_cancel = cancel.clone();
        tokio::spawn(async move {
            run_network_client(
                grpc,
                host,
                inst,
                conn_tracker_for_client,
                scrape_interval,
                net_client_cancel,
            )
            .await;
        });
    }

    // Start health server
    let health_router = build_health_router();
    let listener = tokio::net::TcpListener::bind("0.0.0.0:8080").await?;
    info!("health server listening on :8080");
    let health_cancel = cancel.clone();
    tokio::spawn(async move {
        axum::serve(listener, health_router)
            .with_graceful_shutdown(async move {
                health_cancel.cancelled().await;
            })
            .await
            .ok();
    });

    // Wait for shutdown
    cancel.cancelled().await;
    info!("shutting down");

    // Wait for event reader thread
    if let Err(e) = event_reader_handle.join() {
        error!("event reader thread panicked: {:?}", e);
    }

    info!("collector stopped");
    Ok(())
}
