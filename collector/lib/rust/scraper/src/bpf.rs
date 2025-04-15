use anyhow::Result;

use std::io::{self, Read};
use std::mem::MaybeUninit;
mod bpf_kernel {
    include!(concat!(env!("OUT_DIR"), "/bpf.bpf.skel.rs"));
}

use bpf_kernel::*;
use libbpf_rs::skel::OpenSkel;
use libbpf_rs::skel::SkelBuilder;
use libbpf_rs::Iter;

use crate::common;

#[allow(dead_code)]
pub struct BPFScraper {
    link: Option<libbpf_rs::Link>,
    debug: bool,
    reader: Option<io::BufReader<Iter>>,
}

#[allow(dead_code)]
impl BPFScraper {
    pub fn new(debug: bool) -> Self {
        Self {
            debug,
            link: None,
            reader: None,
        }
    }

    pub fn start(&mut self) -> Result<()> {
        let mut builder = BpfSkelBuilder::default();
        builder.obj_builder.debug(self.debug);

        let mut open_object = MaybeUninit::uninit();
        let open = builder.open(&mut open_object)?;

        let skel = open.load()?;

        self.link = Some(skel.progs.dump_bpf_prog.attach()?);

        Ok(())
    }

    pub fn stop(&self) -> Result<()> {
        Ok(())
    }
}

impl IntoIterator for BPFScraper {
    type Item = common::bpf::bpf_prog_result;
    type IntoIter = std::vec::IntoIter<Self::Item>;

    fn into_iter(self) -> Self::IntoIter {
        let mut buffer = vec![0u8; size_of::<common::bpf::bpf_prog_result>()];
        let iter = Iter::new(&self.link.unwrap()).unwrap();
        let mut r = io::BufReader::new(iter);
        let mut v = Vec::new();

        while r.read_exact(&mut buffer).is_ok() {
            let item: common::bpf::bpf_prog_result =
                unsafe { std::ptr::read(buffer.as_ptr() as *const _) };
            v.push(item);
        }

        v.into_iter()
    }
}
