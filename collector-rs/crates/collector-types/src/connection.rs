use crate::address::Endpoint;
use crate::container::ContainerId;

/// Layer-4 transport protocol for a tracked connection.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum L4Protocol {
    Tcp,
    Udp,
}

/// Whether this side initiated (Client) or accepted (Server) the connection.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum Role {
    Client,
    Server,
}

/// Uniquely identifies a tracked network connection by its endpoints, protocol, and role.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct Connection {
    pub container_id: ContainerId,
    pub local: Endpoint,
    pub remote: Endpoint,
    pub protocol: L4Protocol,
    pub role: Role,
}

/// Packed connection status: active flag + timestamp.
/// Uses a single u64 for cache efficiency (same as C++ version).
/// Upper bit (bit 63) = active flag, lower 63 bits = timestamp in microseconds.
#[derive(Debug, Clone, Copy)]
pub struct ConnStatus(u64);

impl ConnStatus {
    const ACTIVE_BIT: u64 = 1 << 63;

    pub fn new(timestamp_us: u64, active: bool) -> Self {
        let ts = timestamp_us & !Self::ACTIVE_BIT;
        Self(if active { ts | Self::ACTIVE_BIT } else { ts })
    }

    pub fn is_active(self) -> bool {
        self.0 & Self::ACTIVE_BIT != 0
    }

    pub fn timestamp_us(self) -> u64 {
        self.0 & !Self::ACTIVE_BIT
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn conn_status_active() {
        let status = ConnStatus::new(1_000_000, true);
        assert!(status.is_active());
        assert_eq!(status.timestamp_us(), 1_000_000);
    }

    #[test]
    fn conn_status_inactive() {
        let status = ConnStatus::new(2_000_000, false);
        assert!(!status.is_active());
        assert_eq!(status.timestamp_us(), 2_000_000);
    }

    #[test]
    fn conn_status_max_timestamp() {
        let max_ts = (1u64 << 63) - 1;
        let status = ConnStatus::new(max_ts, true);
        assert!(status.is_active());
        assert_eq!(status.timestamp_us(), max_ts);

        let status = ConnStatus::new(max_ts, false);
        assert!(!status.is_active());
        assert_eq!(status.timestamp_us(), max_ts);
    }

    #[test]
    fn conn_status_zero_timestamp() {
        let status = ConnStatus::new(0, true);
        assert!(status.is_active());
        assert_eq!(status.timestamp_us(), 0);

        let status = ConnStatus::new(0, false);
        assert!(!status.is_active());
        assert_eq!(status.timestamp_us(), 0);
    }

    #[test]
    fn conn_status_truncates_overflow_bit() {
        // If someone passes a u64 with bit 63 set, it gets masked off
        let overflow = 1u64 << 63 | 42;
        let status = ConnStatus::new(overflow, false);
        assert_eq!(status.timestamp_us(), 42);
        assert!(!status.is_active());
    }
}
