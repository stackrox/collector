use prometheus::{GaugeVec, Opts, Registry};
use std::sync::LazyLock;

pub static REGISTRY: LazyLock<Registry> = LazyLock::new(Registry::new);

pub static COLLECTOR_COUNTERS: LazyLock<GaugeVec> = LazyLock::new(|| {
    let opts = Opts::new("rox_collector_counters", "Collector internal counters");
    let gauge = GaugeVec::new(opts, &["type"]).unwrap();
    REGISTRY.register(Box::new(gauge.clone())).unwrap();
    gauge
});

pub static COLLECTOR_TIMERS: LazyLock<GaugeVec> = LazyLock::new(|| {
    let opts = Opts::new("rox_collector_timers", "Collector internal timers");
    let gauge = GaugeVec::new(opts, &["type", "metric"]).unwrap();
    REGISTRY.register(Box::new(gauge.clone())).unwrap();
    gauge
});

pub static COLLECTOR_EVENTS: LazyLock<GaugeVec> = LazyLock::new(|| {
    let opts = Opts::new("rox_collector_events", "Per-event-type counters");
    let gauge = GaugeVec::new(opts, &["type"]).unwrap();
    REGISTRY.register(Box::new(gauge.clone())).unwrap();
    gauge
});

pub static PROCESS_LINEAGE_INFO: LazyLock<GaugeVec> = LazyLock::new(|| {
    let opts = Opts::new(
        "rox_collector_process_lineage_info",
        "Process lineage statistics",
    );
    let gauge = GaugeVec::new(opts, &["type"]).unwrap();
    REGISTRY.register(Box::new(gauge.clone())).unwrap();
    gauge
});

pub fn registry() -> &'static Registry {
    &REGISTRY
}

pub fn inc_counter(name: &str) {
    COLLECTOR_COUNTERS.with_label_values(&[name]).inc();
}

pub fn set_counter(name: &str, value: f64) {
    COLLECTOR_COUNTERS.with_label_values(&[name]).set(value);
}

pub fn set_timer(name: &str, metric: &str, value: f64) {
    COLLECTOR_TIMERS
        .with_label_values(&[name, metric])
        .set(value);
}
