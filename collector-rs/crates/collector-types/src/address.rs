use std::net::{IpAddr, Ipv4Addr, Ipv6Addr};

/// An IP address and port pair identifying one side of a network connection.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct Endpoint {
    pub address: IpAddr,
    pub port: u16,
}

/// A CIDR network (address + prefix length) used for filtering connections.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct IpNetwork {
    pub address: IpAddr,
    pub prefix_len: u8,
}

impl IpNetwork {
    /// Returns true if the given address falls within this CIDR network.
    pub fn contains(&self, addr: IpAddr) -> bool {
        match (self.address, addr) {
            (IpAddr::V4(net), IpAddr::V4(a)) => {
                if self.prefix_len == 0 {
                    return true;
                }
                let mask = u32::MAX << (32 - self.prefix_len);
                (u32::from(net) & mask) == (u32::from(a) & mask)
            }
            (IpAddr::V6(net), IpAddr::V6(a)) => {
                if self.prefix_len == 0 {
                    return true;
                }
                let mask = u128::MAX << (128 - self.prefix_len);
                (u128::from(net) & mask) == (u128::from(a) & mask)
            }
            _ => false,
        }
    }
}

/// Returns RFC 1918 (IPv4) and RFC 4193/link-local (IPv6) private network ranges.
pub fn private_networks() -> &'static [IpNetwork] {
    static NETWORKS: &[IpNetwork] = &[
        IpNetwork {
            address: IpAddr::V4(Ipv4Addr::new(10, 0, 0, 0)),
            prefix_len: 8,
        },
        IpNetwork {
            address: IpAddr::V4(Ipv4Addr::new(172, 16, 0, 0)),
            prefix_len: 12,
        },
        IpNetwork {
            address: IpAddr::V4(Ipv4Addr::new(192, 168, 0, 0)),
            prefix_len: 16,
        },
        IpNetwork {
            address: IpAddr::V6(Ipv6Addr::new(0xfc00, 0, 0, 0, 0, 0, 0, 0)),
            prefix_len: 7,
        },
        IpNetwork {
            address: IpAddr::V6(Ipv6Addr::new(0xfe80, 0, 0, 0, 0, 0, 0, 0)),
            prefix_len: 10,
        },
    ];
    NETWORKS
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::HashSet;

    #[test]
    fn ip_network_contains_v4() {
        let net = IpNetwork {
            address: IpAddr::V4(Ipv4Addr::new(10, 0, 0, 0)),
            prefix_len: 8,
        };
        assert!(net.contains(IpAddr::V4(Ipv4Addr::new(10, 1, 2, 3))));
        assert!(net.contains(IpAddr::V4(Ipv4Addr::new(10, 255, 255, 255))));
        assert!(!net.contains(IpAddr::V4(Ipv4Addr::new(11, 0, 0, 1))));
        assert!(!net.contains(IpAddr::V4(Ipv4Addr::new(192, 168, 1, 1))));
    }

    #[test]
    fn ip_network_contains_v6() {
        let net = IpNetwork {
            address: IpAddr::V6(Ipv6Addr::new(0xfc00, 0, 0, 0, 0, 0, 0, 0)),
            prefix_len: 7,
        };
        assert!(net.contains(IpAddr::V6(Ipv6Addr::new(0xfd00, 0, 0, 0, 0, 0, 0, 1))));
        assert!(!net.contains(IpAddr::V6(Ipv6Addr::new(0xfe80, 0, 0, 0, 0, 0, 0, 1))));
    }

    #[test]
    fn ip_network_v4_v6_mismatch() {
        let net = IpNetwork {
            address: IpAddr::V4(Ipv4Addr::new(10, 0, 0, 0)),
            prefix_len: 8,
        };
        assert!(!net.contains(IpAddr::V6(Ipv6Addr::LOCALHOST)));
    }

    #[test]
    fn ip_network_zero_prefix() {
        let net = IpNetwork {
            address: IpAddr::V4(Ipv4Addr::UNSPECIFIED),
            prefix_len: 0,
        };
        assert!(net.contains(IpAddr::V4(Ipv4Addr::new(1, 2, 3, 4))));
    }

    #[test]
    fn endpoint_hash_equality() {
        let a = Endpoint {
            address: IpAddr::V4(Ipv4Addr::new(10, 0, 0, 1)),
            port: 8080,
        };
        let b = Endpoint {
            address: IpAddr::V4(Ipv4Addr::new(10, 0, 0, 1)),
            port: 8080,
        };
        let c = Endpoint {
            address: IpAddr::V4(Ipv4Addr::new(10, 0, 0, 1)),
            port: 9090,
        };
        assert_eq!(a, b);
        assert_ne!(a, c);

        let mut set = HashSet::new();
        set.insert(a);
        assert!(set.contains(&b));
        assert!(!set.contains(&c));
    }

    #[test]
    fn private_networks_cover_rfc1918() {
        let nets = private_networks();
        assert!(nets.iter().any(|n| n.contains(IpAddr::V4(Ipv4Addr::new(10, 0, 0, 1)))));
        assert!(nets.iter().any(|n| n.contains(IpAddr::V4(Ipv4Addr::new(172, 16, 0, 1)))));
        assert!(nets.iter().any(|n| n.contains(IpAddr::V4(Ipv4Addr::new(192, 168, 1, 1)))));
        assert!(!nets.iter().any(|n| n.contains(IpAddr::V4(Ipv4Addr::new(8, 8, 8, 8)))));
    }
}
