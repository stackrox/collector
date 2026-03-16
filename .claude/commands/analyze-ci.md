# Analyze CI Results

You are helping investigate CI failures for the StackRox collector project. This skill describes how to navigate the CI infrastructure, download logs, and diagnose common failure modes.

## CI Structure

The main CI workflow is **"Main collector CI"** (`integration-tests`). It runs integration tests across multiple platforms as separate jobs.

### Finding the PR

```bash
# Look up PR number for a branch (run from the collector repo root)
gh pr view <BRANCH_NAME> --json number,title --jq '"\(.number) | \(.title)"'

# Or for the current branch
gh pr view --json number,title --jq '"\(.number) | \(.title)"'
```

### Listing Failed Checks

```bash
# Get PR check status — list non-passing checks
gh pr view <PR_NUMBER> --json statusCheckRollup \
  --jq '.statusCheckRollup[] | select(.conclusion | IN("SUCCESS","SKIPPED","NEUTRAL") | not) | "\(.name): \(.conclusion) / \(.status)"'

# List workflow runs for a branch
gh run list --branch <BRANCH> --limit 5 \
  --json databaseId,name,conclusion,status \
  --jq '.[] | "\(.databaseId) | \(.name) | \(.conclusion) | \(.status)"'

# Get failed jobs from a run
gh run view <RUN_ID> --json jobs \
  --jq '.jobs[] | select(.conclusion == "failure") | "\(.databaseId) | \(.name)"'
```

### Lint Failures

The **Lint** workflow runs `pre-commit` hooks including `clang-format`. To see what failed:

```bash
gh run view <LINT_RUN_ID> --log 2>&1 | grep -A30 "All changes made by hooks:"
```

This shows the exact diff that clang-format wants applied.

## Downloading and Navigating Log Artifacts

**This is the most important step.** The GitHub Actions log output truncates collector logs to just the crash backtrace. The full `collector.log` with verifier output is only in the artifacts.

### Step 1: List Artifacts

```bash
gh api repos/stackrox/collector/actions/runs/<RUN_ID>/artifacts \
  --jq '.artifacts[] | "\(.id) | \(.name) | \(.size_in_bytes)"'
```

Artifact names follow the pattern `<platform>-logs`, e.g.:
- `rhel-logs`, `rhel-sap-logs`
- `ubuntu-os-logs`, `ubuntu-arm-logs`
- `cos-logs`, `cos-arm64-logs`
- `rhcos-logs`, `rhcos-arm64-logs`
- `flatcar-logs`, `fcarm-logs`
- `rhel-s390x-logs`, `rhel-ppc64le-logs`

### Step 2: Download and Extract

```bash
gh api repos/stackrox/collector/actions/artifacts/<ARTIFACT_ID>/zip > /tmp/<name>.zip
unzip -o /tmp/<name>.zip -d /tmp/<name>
```

### Step 3: Artifact Directory Structure

Each artifact contains:

```
<platform>-logs/
  container-logs/
    <platform>_<vm-name>/
      core-bpf/
        TestProcessNetwork/
          collector.log          # Full collector log for this test
          events.log             # Event stream log
          TestNetworkFlows/
          TestProcessViz/
          TestProcessLineageInfo/
        TestUdpNetworkFlow/
          TestUdpNetorkflow/     # Note: typo in directory name is intentional
            sendto_recvfrom/
              collector.log
              udp-client.log
              udp-server.log
            sendmsg_recvmsg/
            sendmmsg_recvmmsg/
          events.log
        TestSocat/
          collector.log
        ...
        perf.json               # Performance metrics
    integration-test-report-<platform>_<vm>.xml   # JUnit XML results
    integration-test-<platform>_<vm>.log          # Ansible runner log
```

**The `collector.log` file is the primary diagnostic source.** Each test suite gets its own `collector.log` because the collector container is restarted per suite.

### Step 4: Check Test Results Summary

JUnit XML requires xmllint or a simple grep (jq cannot parse XML):

```bash
# Quick summary from the XML attributes
head -3 /tmp/<name>/container-logs/integration-test-report-*.xml

# Find which tests failed
grep -B1 'failure\|error' /tmp/<name>/container-logs/integration-test-report-*.xml | head -20
```

**Key pattern**: If you see `tests="4" failures="1" errors="1" skipped="2"`, the collector crashed on the first test (TestProcessNetwork) and everything else was skipped. This means a BPF loading failure or early startup crash.

## Diagnosing Failure Modes

### 1. BPF Verifier Rejection (Collector Crashes)

**Symptoms**:
- Collector exits with code 139 (SIGSEGV/SIGABRT)
- `tests="4"` in JUnit XML (crash on first test)
- Stack trace shows `KernelDriverCOREEBPF::Setup` -> `sinsp_exception` -> `abort`

**How to find the verifier error in collector.log**:

```bash
# Find the failing program and verifier output
grep -n "BPF program load failed\|failed to load\|BEGIN PROG LOAD LOG\|END PROG LOAD LOG" collector.log

# Get the actual rejection reason (usually the last line before END PROG LOAD LOG)
grep -B5 "END PROG LOAD LOG" collector.log
```

The verifier log is between `BEGIN PROG LOAD LOG` and `END PROG LOAD LOG`. It can be thousands of lines of BPF instruction trace. The **rejection reason is always the last line before `END PROG LOAD LOG`**.

Common verifier rejection messages:
- `R2 min value is negative, either use unsigned or 'var &= const'` — signed value used as size arg to bpf helper
- `BPF program is too large. Processed 1000001 insn` — exceeded 1M instruction verifier limit
- `R0 invalid mem access 'map_value_or_null'` — null check optimized away by clang
- `reg type unsupported for arg#0` — BTF type mismatch (often a warning, not the real error — check end of verifier log)

**After the verifier log**, look for the cascade:

```
libbpf: prog '<name>': failed to load: -13       # -13 = EACCES (Permission denied)
libbpf: prog '<name>': failed to load: -7        # -7 = E2BIG (program too large)
libbpf: failed to load object 'bpf_probe'        # Whole BPF skeleton fails
libpman: failed to load BPF object                # libpman reports failure
terminate called after throwing 'sinsp_exception' # C++ exception
  what(): Initialization issues during scap_init
```

### 2. Self-Check Health Timeout (Collector Runs But Not Healthy)

**Symptoms**:
- Collector starts and loads BPF programs successfully
- `Failed to detect any self-check process events within the timeout.`
- `Failed to detect any self-check networking events within the timeout.`
- Test framework times out: `Timed out waiting for container collector to become health=healthy`

**What to look for in collector.log**:

```bash
grep -n "SelfCheck\|self-check\|Failed to detect\|healthy" collector.log
```

This means the BPF programs loaded but aren't capturing events correctly. Check for:
- Tracepoint attachment failures: `failed to create tracepoint 'syscalls/sys_enter_connect'`
- Missing programs: `unable to find BPF program '<name>'`
- Container ID issues: `unable to initialize the state table API: failed to find dynamic field 'container_id'`

### 3. Test Logic Failures (Collector Healthy, Test Assertions Fail)

**Symptoms**:
- Most tests pass, individual test fails
- Collector is healthy and running
- Test output shows assertion mismatches

**Where to look**:
- The specific test's `collector.log` for event processing
- `events.log` for the raw event stream
- For UDP tests: check `udp-client.log` and `udp-server.log` in the test subdirectory
- JUnit XML for the error message

### 4. Startup/Infrastructure Failures

**Symptoms**:
- `fatal: [<vm>]: FAILED!` in the GitHub Actions log (Ansible failure)
- No collector.log at all for a test
- Image pull failures

**Where to look**:
- The Ansible runner log: `integration-test-<platform>.log` in the artifact
- The GitHub Actions log: `gh run view <RUN_ID> --log --job <JOB_ID>`

## Platform-Specific Notes

### RHEL 8 (kernel 4.18) / s390x / ppc64le
- **Oldest and strictest BPF verifier** — most likely to hit verifier rejections
- RHEL 8 uses kernel 4.18 which has limited BPF type tracking
- s390x and ppc64le also use 4.18-based kernels
- These platforms fail first, so their verifier errors are the canonical ones to fix

### RHEL SAP (kernel 5.14)
- Same base kernel as RHEL 9 but **different kernel config** (SAP-tuned)
- Has hit verifier instruction limit (1M insns) when RHEL 9 passes
- `reg type unsupported for arg#0` is often a warning, not the real error — check end of verifier log for `BPF program is too large`

### COS / Google Container-Optimized OS (kernel 6.6)
- **Clang-compiled kernel** — different BTF attributes than GCC-compiled kernels
- RCU pointer annotations cause different verifier behavior
- Has rejected programs that pass on same-version GCC-compiled kernels

### ARM64 platforms (ubuntu-arm, rhcos-arm64, cos-arm64, fcarm)
- No ia32 compat syscalls — `ia32_*` programs are correctly disabled
- `sys_enter_connect` tracepoint may not exist — expected, handled gracefully
- Self-check timeouts can be timing-related on slower ARM VMs
- cos-arm64 and fcarm tend to pass when ubuntu-arm and rhcos-arm64 fail — may be Docker vs Podman timing differences

### Ubuntu (ubuntu-os)
- Runs on **both Ubuntu 22.04 and 24.04** VMs
- The artifact contains logs from multiple VMs (check the subdirectory names)
- Ubuntu 22.04 (kernel 6.8) is stricter than 24.04 (kernel 6.17)

### Flatcar / Fedora CoreOS
- Generally the most permissive — if these fail, something is fundamentally broken

## Common Non-Fatal Log Messages

These appear on all platforms and are expected/harmless:

```
# Container plugin not loaded (by design — collector uses cgroup extraction)
unable to initialize the state table API: failed to find dynamic field 'container_id' in threadinfo

# Enter events removed in modern BPF (by design)
failed to determine tracepoint 'syscalls/sys_enter_connect' perf event ID: No such file or directory

# TLS not configured (expected in integration tests)
Partial TLS config: CACertPath="", ClientCertPath="", ClientKeyPath=""; will not use TLS

# Container filter uses proc.vpid not container.id (by design)
Could not set container filter: proc.vpid is not a valid number

# Programs excluded from build via MODERN_BPF_EXCLUDE_PROGS
unable to find BPF program '<name>'
```

## Quick Investigation Workflow

1. **Identify failing platforms**: Check PR status checks
2. **Download artifacts**: For each failing platform, download the `<platform>-logs` artifact
3. **Check JUnit XML first**: `tests="4"` = crash, higher number = specific test failures
4. **Read collector.log**: For crashes, search for `failed to load` and read the verifier log above it. For test failures, read the specific test's collector.log
5. **Check kernel version**: First lines of collector.log show OS and kernel version
6. **Cross-reference platforms**: If RHEL 9 passes but RHEL SAP fails, it's likely a verifier limit issue. If all arm64 fail, check self-check timing. If everything fails, check BPF program structure
7. **Compare with master**: Download master's artifacts for the same platform to confirm regression

## Build Exclusion Mechanism

Collector can exclude BPF programs from compilation via CMake:

```cmake
# collector/CMakeLists.txt
set(MODERN_BPF_EXCLUDE_PROGS "^(openat2|ppoll|...)$" CACHE STRING "..." FORCE)
```

The regex matches against BPF source file stems (e.g., `pread64` matches `pread64.bpf.c`). Excluded programs are not compiled into the skeleton. The loader in `maps.c:add_bpf_program_to_tail_table()` handles missing programs gracefully (logs debug message, returns success).

Only exclude programs for syscalls that collector does not subscribe to. Collector's syscall list is in `collector/lib/CollectorConfig.h` (`kSyscalls[]` and `kSendRecvSyscalls[]`).

## Cleanup

Once the analysis is complete and you have reported your findings, delete all downloaded log artifacts (zip files and extracted directories) from `/tmp/`:

```bash
rm -rf /tmp/*-logs /tmp/*-logs.zip
```

This prevents stale logs from accumulating across investigations.
