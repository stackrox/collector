use anyhow::Result;
use libbpf_rs::skel::OpenSkel;
use libbpf_rs::skel::SkelBuilder;
use libbpf_rs::Iter;
use network::NetworkSkelBuilder;
use std::io;
use std::io::BufRead;
use std::mem::MaybeUninit;

use clap::{Parser, ValueEnum};

mod tasks {
    include!(concat!(
        env!("CARGO_MANIFEST_DIR"),
        "/src/bpf/tasks.bpf.skel.rs"
    ));
}

mod network {
    include!(concat!(
        env!("CARGO_MANIFEST_DIR"),
        "/src/bpf/network.bpf.skel.rs"
    ));
}

use tasks::*;

#[derive(Debug, ValueEnum, Clone)]
enum Probe {
    Process,
    Network,
}

#[derive(Parser, Debug)]
#[command(version, about, long_about=None)]
struct Args {
    probe: Probe,

    #[arg(short, long, action=clap::ArgAction::SetTrue)]
    debug: Option<bool>,
}

fn do_network(debug: bool) -> Result<()> {
    let mut builder = NetworkSkelBuilder::default();
    builder.obj_builder.debug(debug);

    let mut open_object = MaybeUninit::uninit();
    let open = builder.open(&mut open_object)?;

    let skel = open.load()?;

    let link = skel.progs.dump_tcp4.attach()?;

    let iter = Iter::new(&link)?;
    let reader = io::BufReader::new(iter);

    for line in reader.lines().map_while(Result::ok) {
        println!("{}", line);
    }

    Ok(())
}

fn do_process(debug: bool) -> Result<()> {
    let mut builder = TasksSkelBuilder::default();
    builder.obj_builder.debug(debug);

    let mut open_object = MaybeUninit::uninit();
    let open = builder.open(&mut open_object)?;

    let skel = open.load()?;

    let link = skel.progs.iter_tasks.attach()?;

    let iter = Iter::new(&link)?;
    let reader = io::BufReader::new(iter);

    for line in reader.lines().map_while(Result::ok) {
        println!("{}", line);
    }

    Ok(())
}

fn main() -> Result<()> {
    let args = Args::parse();

    let debug = args.debug.unwrap_or(false);

    match args.probe {
        Probe::Process => do_process(debug),
        Probe::Network => do_network(debug),
    }
}
