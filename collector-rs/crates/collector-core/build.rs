use std::path::PathBuf;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let manifest_dir = PathBuf::from(std::env::var("CARGO_MANIFEST_DIR")?);
    let proto_root = manifest_dir
        .join("../../../collector/proto/third_party/stackrox/proto")
        .canonicalize()?;

    let proto_files = [
        "api/v1/empty.proto",
        "api/v1/signal.proto",
        "internalapi/sensor/collector.proto",
        "internalapi/sensor/network_connection_info.proto",
        "internalapi/sensor/network_connection_iservice.proto",
        "internalapi/sensor/network_enums.proto",
        "internalapi/sensor/signal_iservice.proto",
        "storage/network_flow.proto",
        "storage/process_indicator.proto",
    ];

    let proto_paths: Vec<PathBuf> = proto_files
        .iter()
        .map(|f| proto_root.join(f))
        .collect();

    tonic_build::configure()
        .build_server(false)
        .compile_protos(&proto_paths, &[&proto_root])?;

    for file in &proto_files {
        println!("cargo::rerun-if-changed={}", proto_root.join(file).display());
    }

    Ok(())
}
