use std::path::{Path, PathBuf};
use std::sync::{Arc, RwLock};
use std::time::Duration;

use clap::Parser;
use serde::Deserialize;
use tokio_util::sync::CancellationToken;
use tracing::{debug, info, warn};

/// Command-line arguments for the collector binary, with env var fallbacks.
#[derive(Parser, Debug)]
#[command(name = "collector")]
pub struct CliArgs {
    #[arg(long, env = "GRPC_SERVER")]
    pub grpc_server: Option<String>,

    #[arg(long, default_value = "/etc/stackrox/runtime_config.yaml",
          env = "ROX_COLLECTOR_CONFIG_PATH")]
    pub config_file: PathBuf,

    #[arg(long, env = "ROX_COLLECTOR_TLS_CERTS")]
    pub tls_certs: Option<PathBuf>,

    #[arg(long, env = "ROX_COLLECTOR_TLS_CA")]
    pub tls_ca: Option<PathBuf>,

    #[arg(long, env = "ROX_COLLECTOR_TLS_CLIENT_CERT")]
    pub tls_client_cert: Option<PathBuf>,

    #[arg(long, env = "ROX_COLLECTOR_TLS_CLIENT_KEY")]
    pub tls_client_key: Option<PathBuf>,

    #[arg(long, default_value = "info", env = "ROX_COLLECTOR_LOG_LEVEL")]
    pub log_level: String,

    #[arg(long, default_value = "/host", env = "COLLECTOR_HOST_ROOT")]
    pub host_root: PathBuf,

    #[arg(long, env = "COLLECTOR_CONFIG")]
    pub collector_config: Option<String>,

    #[arg(long, default_value = "536870912",
          env = "ROX_COLLECTOR_SINSP_TOTAL_BUFFER_SIZE")]
    pub bpf_buffer_size: usize,

    #[arg(long, env = "ROX_COLLECTOR_SINSP_CPU_PER_BUFFER")]
    pub cpus_per_buffer: Option<usize>,

    #[arg(long, default_value = "32768",
          env = "ROX_COLLECTOR_SINSP_THREAD_CACHE_SIZE")]
    pub process_table_size: usize,
}

/// Hot-reloadable runtime configuration, loaded from YAML and pushed by Sensor.
#[derive(Debug, Clone, Deserialize)]
#[serde(default)]
pub struct RuntimeConfig {
    #[serde(with = "serde_duration_secs")]
    pub scrape_interval: Duration,
    #[serde(with = "serde_duration_secs")]
    pub afterglow_period: Duration,
    pub enable_afterglow: bool,
    pub enable_external_ips: bool,
    pub max_connections_per_minute: u32,
    pub disable_process_args: bool,
    pub collect_connection_status: bool,
    pub track_send_recv: bool,
    pub disable_network_flows: bool,
    pub enable_ports: bool,
    pub network_drop_ignored: bool,
    pub ignored_networks: Vec<String>,
    pub non_aggregated_networks: Vec<String>,
    pub scrape_disabled: bool,
    pub processes_listening_on_port: bool,
    pub import_users: bool,
    pub enable_connection_stats: bool,
    pub enable_detailed_metrics: bool,
    pub enable_introspection: bool,
}

impl Default for RuntimeConfig {
    fn default() -> Self {
        Self {
            scrape_interval: Duration::from_secs(30),
            afterglow_period: Duration::from_secs(300),
            enable_afterglow: true,
            enable_external_ips: false,
            max_connections_per_minute: 2048,
            disable_process_args: false,
            collect_connection_status: true,
            track_send_recv: false,
            disable_network_flows: false,
            enable_ports: false,
            network_drop_ignored: false,
            ignored_networks: Vec::new(),
            non_aggregated_networks: Vec::new(),
            scrape_disabled: false,
            processes_listening_on_port: false,
            import_users: true,
            enable_connection_stats: false,
            enable_detailed_metrics: false,
            enable_introspection: false,
        }
    }
}

/// Loads runtime config from YAML at `path`, falling back to defaults on any error.
pub fn load_runtime_config(path: &Path) -> RuntimeConfig {
    match std::fs::read_to_string(path) {
        Ok(contents) => match serde_yaml::from_str(&contents) {
            Ok(config) => {
                info!("loaded runtime config from {}", path.display());
                config
            }
            Err(e) => {
                warn!("failed to parse config {}: {}, using defaults", path.display(), e);
                RuntimeConfig::default()
            }
        },
        Err(_) => {
            debug!("config file {} not found, using defaults", path.display());
            RuntimeConfig::default()
        }
    }
}

/// Polls the config file for modifications and reloads into the shared `RwLock` on change.
pub async fn watch_config_file(
    path: PathBuf,
    config: Arc<RwLock<RuntimeConfig>>,
    cancel: CancellationToken,
) {
    let mut last_modified = std::fs::metadata(&path)
        .and_then(|m| m.modified())
        .ok();

    let poll_interval = Duration::from_secs(5);

    loop {
        tokio::select! {
            _ = cancel.cancelled() => break,
            _ = tokio::time::sleep(poll_interval) => {}
        }

        let current_modified = match std::fs::metadata(&path).and_then(|m| m.modified()) {
            Ok(t) => t,
            Err(_) => continue,
        };

        let changed = match last_modified {
            Some(prev) => current_modified != prev,
            None => true,
        };

        if changed {
            last_modified = Some(current_modified);
            let new_config = load_runtime_config(&path);
            match config.write() {
                Ok(mut cfg) => {
                    *cfg = new_config;
                    info!("runtime config reloaded from {}", path.display());
                }
                Err(e) => {
                    warn!("failed to acquire config write lock: {}", e);
                }
            }
        }
    }
    debug!("config watcher stopped");
}

mod serde_duration_secs {
    use serde::{self, Deserialize, Deserializer};
    use std::time::Duration;

    pub fn deserialize<'de, D>(deserializer: D) -> Result<Duration, D::Error>
    where
        D: Deserializer<'de>,
    {
        let secs = u64::deserialize(deserializer)?;
        Ok(Duration::from_secs(secs))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn runtime_config_defaults() {
        let config = RuntimeConfig::default();
        assert_eq!(config.scrape_interval, Duration::from_secs(30));
        assert_eq!(config.afterglow_period, Duration::from_secs(300));
        assert!(config.enable_afterglow);
        assert!(!config.enable_external_ips);
        assert_eq!(config.max_connections_per_minute, 2048);
        assert!(!config.disable_process_args);
        assert!(config.collect_connection_status);
        assert!(!config.track_send_recv);
        assert!(!config.disable_network_flows);
    }

    #[test]
    fn runtime_config_from_yaml() {
        let yaml = r#"
scrape_interval: 60
afterglow_period: 600
enable_afterglow: false
track_send_recv: true
ignored_networks:
  - "10.0.0.0/8"
  - "172.16.0.0/12"
"#;
        let config: RuntimeConfig = serde_yaml::from_str(yaml).unwrap();
        assert_eq!(config.scrape_interval, Duration::from_secs(60));
        assert_eq!(config.afterglow_period, Duration::from_secs(600));
        assert!(!config.enable_afterglow);
        assert!(config.track_send_recv);
        assert_eq!(config.ignored_networks, vec!["10.0.0.0/8", "172.16.0.0/12"]);
        assert!(!config.enable_external_ips);
    }

    #[test]
    fn runtime_config_empty_yaml() {
        let config: RuntimeConfig = serde_yaml::from_str("{}").unwrap();
        assert_eq!(config.scrape_interval, Duration::from_secs(30));
    }
}
