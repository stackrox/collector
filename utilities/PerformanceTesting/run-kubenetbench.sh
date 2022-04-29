#!/bin/bash
set -e

# This script runs the netperf tcp_crr test via cillium/kubenetbench
# with a user configured number of streams.

die() {
    echo >&2 "$@"
    exit 1
}

log() {
    echo "$*" >&2
}

artifacts_dir="$1"
test_name="$2"
num_streams="$3"

[[ -n "$artifacts_dir" && -n "$test_name" && -n "${num_streams}" ]] \
    || die "Usage: $0 <artifacts-dir> <test-name> <num-streams>"

knb_base_url="https://github.com/cilium/kubenetbench"
knb_url="${knb_base_url}/archive/refs/heads/master.zip"
knb_zip="$(mktemp)"
knb_base_dir="$(mktemp -d)"
knb_dir="${knb_base_dir}/kubenetbench-master/"
wget "${knb_url}" -O "${knb_zip}"

unzip -d "${knb_base_dir}" "${knb_zip}"
rm -rf "${knb_zip}"

pushd "${knb_dir}"

# Patch tolerations in kubenetbench/core/monitor.go to not run on master nodes
patch -p1 << 'EOF'
diff --git a/kubenetbench/core/monitor.go b/kubenetbench/core/monitor.go
index edbe57b..0e914b2 100644
--- a/kubenetbench/core/monitor.go
+++ b/kubenetbench/core/monitor.go
@@ -39,11 +39,11 @@ spec:
         {{.sessLabel}}
         role: monitor
     spec:
-      # tolerations:
-      # # this toleration is to have the daemonset runnable on master nodes
-      # # remove it if your masters can't run pods
-      # - key: node-role.kubernetes.io/master
-      #   effect: NoSchedule
+      tolerations:
+      # this toleration is to have the daemonset runnable on master nodes
+      # remove it if your masters can't run pods
+      - key: node-role.kubernetes.io/master
+        effect: NoSchedule
EOF

log "build kubenetbench"
make

log "deploy kubenetbench"

export KUBECONFIG="${artifacts_dir}/kubeconfig"
kubenetbench/kubenetbench -s "${test_name}" init --port-forward

log "run netperf tcp_crr with ${num_streams} streams"
"${test_name}"/knb pod2pod -b netperf --netperf-type tcp_crr --netperf-nstreams "${num_streams}"
popd

log "teardown knb-monitor"
kubectl delete ds/knb-monitor

# $knb_dir contains test results that may be useful
rm -rf "${knb_dir}"
