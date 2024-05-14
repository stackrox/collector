#!/usr/bin/env bash

set -euo pipefail

podman run --rm -d \
    -e COLLECTOR_CONFIG='{"tlsConfig":{"caCertPath":"/var/run/secrets/stackrox.io/certs/ca.pem","clientCertPath":"/var/run/secrets/stackrox.io/certs/cert.pem","clientKeyPath":"/var/run/secrets/stackrox.io/certs/key.pem"},"logLevel":"Debug"}' \
    -e COLLECTION_METHOD=CORE_BPF \
    -e GRPC_SERVER="sensor.stackrox.svc:443" \
    -e SNI_HOSTNAME="sensor.stackrox.svc" \
    -v /dev:/host/dev:ro \
    -v /etc:/host/etc:ro \
    -v /proc:/host/proc:ro \
    -v /sys:/host/sys:ro \
    -v /usr/lib:/host/usr/lib:ro \
    --mount type=tmpfs,destination=/modules \
    quay.io/stackrox-io/collector:3.18.x-179-g871fcaec01-slim
