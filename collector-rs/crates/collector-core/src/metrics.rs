use prometheus::{GaugeVec, Opts, Registry};
use std::sync::LazyLock;

pub static REGISTRY: LazyLock<Registry> = LazyLock::new(Registry::new);

/// General-purpose counters keyed by type (e.g. "events_total", "process_signals_sent").
pub static COLLECTOR_COUNTERS: LazyLock<GaugeVec> = LazyLock::new(|| {
    let opts = Opts::new("rox_collector_counters", "Collector internal counters");
    let gauge = GaugeVec::new(opts, &["type"]).unwrap();
    REGISTRY.register(Box::new(gauge.clone())).unwrap();
    gauge
});

/// Timer gauges keyed by (type, metric) for tracking operation durations.
pub static COLLECTOR_TIMERS: LazyLock<GaugeVec> = LazyLock::new(|| {
    let opts = Opts::new("rox_collector_timers", "Collector internal timers");
    let gauge = GaugeVec::new(opts, &["type", "metric"]).unwrap();
    REGISTRY.register(Box::new(gauge.clone())).unwrap();
    gauge
});

/// Per-event-type counters (exec, exit, connect, accept, close, listen).
pub static COLLECTOR_EVENTS: LazyLock<GaugeVec> = LazyLock::new(|| {
    let opts = Opts::new("rox_collector_events", "Per-event-type counters");
    let gauge = GaugeVec::new(opts, &["type"]).unwrap();
    REGISTRY.register(Box::new(gauge.clone())).unwrap();
    gauge
});

/// Process lineage statistics (string counts, truncations, etc.).
pub static PROCESS_LINEAGE_INFO: LazyLock<GaugeVec> = LazyLock::new(|| {
    let opts = Opts::new(
        "rox_collector_process_lineage_info",
        "Process lineage statistics",
    );
    let gauge = GaugeVec::new(opts, &["type"]).unwrap();
    REGISTRY.register(Box::new(gauge.clone())).unwrap();
    gauge
});

/// Returns the global Prometheus registry used for all collector metrics.
pub fn registry() -> &'static Registry {
    &REGISTRY
}

/// Increments a named counter in `COLLECTOR_COUNTERS` by 1.
pub fn inc_counter(name: &str) {
    COLLECTOR_COUNTERS.with_label_values(&[name]).inc();
}

/// Sets a named counter in `COLLECTOR_COUNTERS` to an absolute value.
pub fn set_counter(name: &str, value: f64) {
    COLLECTOR_COUNTERS.with_label_values(&[name]).set(value);
}

/// Sets a timer gauge in `COLLECTOR_TIMERS` keyed by (name, metric).
pub fn set_timer(name: &str, metric: &str, value: f64) {
    COLLECTOR_TIMERS
        .with_label_values(&[name, metric])
        .set(value);
}
