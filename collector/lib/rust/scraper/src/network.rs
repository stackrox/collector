use std::io::{self, BufRead};
use std::net::{Ipv4Addr, Ipv6Addr};
use std::{mem::MaybeUninit, net::IpAddr};

use libbpf_rs::skel::OpenSkel;
use libbpf_rs::skel::SkelBuilder;

use anyhow::Result;
use libbpf_rs::Iter;
use network_kernel::NetworkSkelBuilder;

mod network_kernel {
    include!(concat!(env!("OUT_DIR"), "/network.bpf.skel.rs"));
}

#[derive(Debug)]
#[allow(dead_code)]
pub struct Connection {
    local: IpAddr,
    remote: IpAddr,
    state: u8,
}

#[allow(dead_code)]
pub struct NetworkScraper {
    link: Option<libbpf_rs::Link>,
    debug: bool,
    reader: Option<io::BufReader<Iter>>,
}

impl NetworkScraper {
    pub fn new(debug: bool) -> Self {
        Self {
            link: None,
            reader: None,
            debug,
        }
    }

    pub fn start(&mut self) -> Result<()> {
        let mut builder = NetworkSkelBuilder::default();
        builder.obj_builder.debug(self.debug);

        let mut open_object = MaybeUninit::uninit();
        let open = builder.open(&mut open_object)?;

        let skel = open.load()?;

        self.link = Some(skel.progs.dump_tcp4.attach()?);

        println!("started");
        Ok(())
    }

    #[allow(dead_code)]
    pub fn stop(&self) -> Result<()> {
        Ok(())
    }

    fn parse_connection(&self, line: &str) -> Option<Connection> {
        let parts: Vec<&str> = line.split_whitespace().collect();

        let local_raw = parts[1];
        let remote_raw = parts[2];
        let state = parts[3];

        Some(Connection {
            local: self.parse_addr(local_raw),
            remote: self.parse_addr(remote_raw),
            state: u8::from_str_radix(state, 16).unwrap_or(0),
        })
    }

    fn parse_addr(&self, raw: &str) -> IpAddr {
        let sections: Vec<&str> = raw.split(':').collect();

        if sections[0].len() == 8 {
            let ip = u32::from_str_radix(sections[0], 16).unwrap();
            Ipv4Addr::from_bits(ip).into()
        } else {
            let ip = u128::from_str_radix(sections[0], 16).unwrap();
            Ipv6Addr::from_bits(ip).into()
        }
    }
}

impl Iterator for NetworkScraper {
    type Item = Connection;

    fn next(&mut self) -> Option<Connection> {
        let Some(l) = &self.link else {
            println!("no link");
            return None;
        };
        let iter = Iter::new(l).unwrap();
        let mut r = io::BufReader::new(iter);

        let mut buf = String::new();
        let Ok(_) = r.read_line(&mut buf) else {
            println!("line read failed");
            return None;
        };

        self.parse_connection(&buf)
    }
}
