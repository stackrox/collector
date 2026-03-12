pub mod events;

#[allow(
    clippy::all,
    non_camel_case_types,
    non_upper_case_globals,
    non_snake_case,
    dead_code
)]
mod skel {
    include!(concat!(env!("OUT_DIR"), "/collector.skel.rs"));
}

use std::collections::VecDeque;
use std::mem::{self, MaybeUninit};
use std::time::Duration;

use anyhow::{Context, Result};
use libbpf_rs::skel::{OpenSkel, Skel, SkelBuilder};
use libbpf_rs::RingBufferBuilder;

use events::{ConnectEvent, EventHeader, EventType, ExecEvent, ExitEvent, RawEvent};

/// Abstraction over BPF event sources, enabling test mocks without real BPF programs.
pub trait EventSource: Send {
    fn next_event(&mut self, timeout: Duration) -> Option<RawEvent>;
}

/// Loads and attaches BPF programs, then polls the ring buffer for events.
pub struct BpfLoader {
    rx: std::sync::mpsc::Receiver<RawEvent>,
    // Must keep skel and ring alive for the BPF program's lifetime.
    // Fields are dropped in declaration order, ring first (stops callbacks), then skel.
    _ring: libbpf_rs::RingBuffer<'static>,
    _open_obj: Box<MaybeUninit<libbpf_rs::OpenObject>>,
    _skel: Box<skel::CollectorSkel<'static>>,
}

// Safety: BPF objects are safe to use from any thread once loaded and attached.
// The raw pointers inside OpenObject/CollectorSkel are only accessed through
// the ring buffer poll, which is synchronized via the mpsc channel.
unsafe impl Send for BpfLoader {}

impl BpfLoader {
    /// Opens the BPF skeleton, loads programs into the kernel, and attaches tracepoints.
    pub fn load_and_attach() -> Result<Self> {
        let mut open_obj = Box::new(MaybeUninit::uninit());
        let builder = skel::CollectorSkelBuilder::default();
        // Safety: open_obj is boxed (stable address) and lives as long as the skel.
        let open_obj_ref: &'static mut MaybeUninit<libbpf_rs::OpenObject> =
            unsafe { &mut *(&mut *open_obj as *mut _) };
        let open_skel = builder.open(open_obj_ref).context("Failed to open BPF skeleton")?;
        let mut skel = open_skel.load().context("Failed to load BPF programs")?;
        skel.attach().context("Failed to attach BPF programs")?;

        let (tx, rx) = std::sync::mpsc::sync_channel::<RawEvent>(4096);

        let skel = Box::new(skel);

        // Safety: skel is boxed (stable address) and we keep it alive in Self.
        let skel_ref: &'static skel::CollectorSkel<'static> =
            unsafe { &*(&*skel as *const _) };

        let mut ring_builder = RingBufferBuilder::new();
        ring_builder
            .add(&skel_ref.maps.events, move |data: &[u8]| {
                if let Some(event) = parse_ring_event(data) {
                    let _ = tx.try_send(event);
                }
                0
            })
            .context("Failed to add ring buffer")?;

        let ring = ring_builder.build().context("Failed to build ring buffer")?;

        Ok(Self {
            rx,
            _ring: ring,
            _open_obj: open_obj,
            _skel: skel,
        })
    }
}

impl EventSource for BpfLoader {
    fn next_event(&mut self, timeout: Duration) -> Option<RawEvent> {
        let _ = self._ring.poll(timeout);
        self.rx.try_recv().ok()
    }
}

fn parse_ring_event(data: &[u8]) -> Option<RawEvent> {
    if data.len() < mem::size_of::<EventHeader>() {
        return None;
    }

    let header: EventHeader = unsafe { std::ptr::read_unaligned(data.as_ptr() as *const _) };
    let event_type = EventType::from_u32(header.event_type)?;

    match event_type {
        EventType::ProcessExec if data.len() >= mem::size_of::<ExecEvent>() => {
            let evt: ExecEvent = unsafe { std::ptr::read_unaligned(data.as_ptr() as *const _) };
            Some(RawEvent::Exec(evt))
        }
        EventType::ProcessExit if data.len() >= mem::size_of::<ExitEvent>() => {
            let evt: ExitEvent = unsafe { std::ptr::read_unaligned(data.as_ptr() as *const _) };
            Some(RawEvent::Exit(evt))
        }
        EventType::SocketConnect | EventType::SocketAccept | EventType::SocketClose | EventType::SocketListen
            if data.len() >= mem::size_of::<ConnectEvent>() =>
        {
            let evt: ConnectEvent =
                unsafe { std::ptr::read_unaligned(data.as_ptr() as *const _) };
            match event_type {
                EventType::SocketConnect => Some(RawEvent::Connect(evt)),
                EventType::SocketAccept => Some(RawEvent::Accept(evt)),
                EventType::SocketClose => Some(RawEvent::Close(evt)),
                EventType::SocketListen => Some(RawEvent::Listen(evt)),
                _ => unreachable!(),
            }
        }
        _ => None,
    }
}

/// Test double that replays a pre-recorded sequence of events.
pub struct MockEventSource {
    events: VecDeque<RawEvent>,
}

impl MockEventSource {
    pub fn new(events: Vec<RawEvent>) -> Self {
        Self {
            events: events.into(),
        }
    }
}

impl EventSource for MockEventSource {
    fn next_event(&mut self, _timeout: Duration) -> Option<RawEvent> {
        self.events.pop_front()
    }
}
