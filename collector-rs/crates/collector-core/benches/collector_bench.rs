use criterion::{black_box, criterion_group, criterion_main, Criterion};
use std::collections::HashMap;
use std::net::{IpAddr, Ipv4Addr};
use std::time::Duration;

use collector_core::conn_tracker::ConnTracker;
use collector_core::container_id;
use collector_core::process_table::ProcessTable;
use collector_core::rate_limit::RateLimitCache;
use collector_types::address::Endpoint;
use collector_types::connection::{ConnStatus, Connection, L4Protocol, Role};
use collector_types::container::ContainerId;
use collector_types::process::ProcessInfo;

fn make_conn(i: u16) -> Connection {
    Connection {
        container_id: ContainerId(format!("container{:04}", i % 100)),
        local: Endpoint {
            address: IpAddr::V4(Ipv4Addr::new(10, 0, 0, 1)),
            port: 8080,
        },
        remote: Endpoint {
            address: IpAddr::V4(Ipv4Addr::new(10, 0, (i >> 8) as u8, i as u8)),
            port: 50000 + i,
        },
        protocol: L4Protocol::Tcp,
        role: Role::Client,
    }
}

fn make_process(pid: u32, container: &str) -> ProcessInfo {
    ProcessInfo {
        pid,
        uid: 1000,
        gid: 1000,
        exe_path: format!("/usr/bin/app{}", pid % 50),
        args: format!("--config /etc/app{}.conf", pid % 50),
        container_id: ContainerId(container.to_string()),
    }
}

fn bench_connection_tracking(c: &mut Criterion) {
    c.bench_function("conn_tracker_insert_1000", |b| {
        b.iter(|| {
            let mut tracker = ConnTracker::new(Duration::from_secs(30));
            for i in 0..1000u16 {
                let conn = make_conn(i);
                tracker.update_connection(conn, (i as u64) * 1000, true);
            }
            black_box(&tracker);
        });
    });
}

fn bench_fetch_and_normalize(c: &mut Criterion) {
    let mut tracker = ConnTracker::new(Duration::from_secs(30));
    for i in 0..1000u16 {
        let conn = make_conn(i);
        tracker.update_connection(conn, (i as u64) * 1000, true);
    }

    c.bench_function("conn_tracker_fetch_state_normalized_1000", |b| {
        b.iter(|| {
            let state = tracker.fetch_state(true, false);
            black_box(&state);
        });
    });
}

fn bench_compute_delta(c: &mut Criterion) {
    let tracker = ConnTracker::new(Duration::from_secs(30));

    // Old state: connections 0..500, all active
    let mut old_state: HashMap<Connection, ConnStatus> = HashMap::new();
    for i in 0..500u16 {
        old_state.insert(make_conn(i), ConnStatus::new((i as u64) * 1000, true));
    }

    // New state: connections 250..750, all active (250 overlap, 250 removed, 250 added)
    let mut new_state: HashMap<Connection, ConnStatus> = HashMap::new();
    for i in 250..750u16 {
        new_state.insert(make_conn(i), ConnStatus::new((i as u64 + 500) * 1000, true));
    }

    c.bench_function("conn_tracker_compute_delta_500", |b| {
        b.iter(|| {
            let delta = tracker.compute_delta(
                black_box(&old_state),
                black_box(&new_state),
                1_000_000,
            );
            black_box(&delta);
        });
    });
}

fn bench_container_id_extraction(c: &mut Criterion) {
    // Pre-generate 1000 realistic cgroup paths
    let cgroups: Vec<String> = (0..1000)
        .map(|i| {
            format!(
                "/kubepods/besteffort/pod{:08x}/crio-{:064x}.scope",
                i, i + 0xabc123u64
            )
        })
        .collect();

    c.bench_function("container_id_extract_1000", |b| {
        b.iter(|| {
            for cgroup in &cgroups {
                black_box(container_id::extract_container_id(cgroup));
            }
        });
    });
}

fn bench_rate_limiter(c: &mut Criterion) {
    // Pre-generate 10000 keys
    let keys: Vec<String> = (0..10000).map(|i| format!("process:{}:/usr/bin/app", i)).collect();

    c.bench_function("rate_limit_allow_10000", |b| {
        b.iter(|| {
            let mut cache = RateLimitCache::with_config(10, Duration::from_secs(1800), 65536);
            for key in &keys {
                black_box(cache.allow(key));
            }
        });
    });
}

fn bench_process_table_upsert(c: &mut Criterion) {
    c.bench_function("process_table_upsert_1000", |b| {
        b.iter(|| {
            let mut table = ProcessTable::new();
            for i in 0..1000u32 {
                let proc = make_process(i, &format!("container{:04}", i % 100));
                table.upsert(proc, i.saturating_sub(1));
            }
            black_box(&table);
        });
    });
}

fn bench_process_table_lineage(c: &mut Criterion) {
    let mut table = ProcessTable::new();
    let cid = ContainerId("bench-container".into());

    // Build a chain of depth 10: pid 1 -> 2 -> 3 -> ... -> 10
    for i in 1..=10u32 {
        let proc = ProcessInfo {
            pid: i,
            uid: 1000,
            gid: 1000,
            exe_path: format!("/usr/bin/level{}", i),
            args: String::new(),
            container_id: cid.clone(),
        };
        table.upsert(proc, i.saturating_sub(1));
    }

    c.bench_function("process_table_lineage_depth10", |b| {
        b.iter(|| {
            let lineage = table.lineage(black_box(10), &cid);
            black_box(&lineage);
        });
    });
}

criterion_group!(
    benches,
    bench_connection_tracking,
    bench_fetch_and_normalize,
    bench_compute_delta,
    bench_container_id_extraction,
    bench_rate_limiter,
    bench_process_table_upsert,
    bench_process_table_lineage,
);
criterion_main!(benches);
