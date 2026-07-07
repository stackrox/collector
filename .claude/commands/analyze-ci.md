# Analyze CI Results

You are helping investigate CI failures for the StackRox collector project.

## Quick Investigation Workflow

1. Identify failing platforms from PR checks
2. Download log artifacts for each failing platform
3. Check JUnit XML: low test count + high skip count = startup crash; high test count = specific test failure
4. Read `collector.log`: for crashes search `failed to load`; for test failures read the specific test's log
5. Check kernel version (first lines of collector.log)
6. Cross-reference platforms: same failure on multiple = code bug; single platform = platform-specific
7. Compare with master's artifacts for the same platform to confirm regression

## CI Structure

The main CI workflow is **"Main collector CI"** (`integration-tests`). There is also **"Test Konflux builds"** (`run-konflux-tests`) which runs the same integration tests. Both must pass.

### Finding Failures

```bash
# PR number for current branch
gh pr view --json number,title --jq '"\(.number) | \(.title)"'

# List non-passing checks
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

```bash
gh run view <LINT_RUN_ID> --log 2>&1 | grep -A30 "All changes made by hooks:"
```

## Downloading Log Artifacts

**This is the most important step.** CI output truncates collector logs. The full `collector.log` with verifier output is only in the artifacts.

```bash
# List artifacts
gh api repos/stackrox/collector/actions/runs/<RUN_ID>/artifacts \
  --jq '.artifacts[] | "\(.id) | \(.name) | \(.size_in_bytes)"'

# Download and extract
gh api repos/stackrox/collector/actions/artifacts/<ARTIFACT_ID>/zip > /tmp/<name>.zip
unzip -o /tmp/<name>.zip -d /tmp/<name>
```

Artifact names: `rhel-logs`, `rhel-sap-logs`, `ubuntu-os-logs`, `ubuntu-arm-logs`, `cos-logs`, `cos-arm64-logs`, `rhcos-logs`, `rhcos-arm64-logs`, `flatcar-logs`, `fcarm-logs`, `rhel-s390x-logs`, `rhel-ppc64le-logs`.

**Note**: Some artifacts contain logs from **multiple VMs**. The `rhcos-logs` artifact has subdirectories for rhcos-4.12, 4.14, 4.16, 4.18, and 9.6. A single VM failure fails the whole job. Always check which specific VM failed.

### Artifact Structure

```
container-logs/
  <platform>_<vm-name>/
    core-bpf/
      TestProcessNetwork/
        collector.log
        events.log
      TestProcessListeningOnPort/
        collector.log          # Sometimes here
        TestProcessListeningOnPort/
          collector.log        # Sometimes nested
      ...
      perf.json
  integration-test-report-<platform>_<vm>.xml
  integration-test-<platform>_<vm>.log
```

**Collector log locations vary** — some tests put `collector.log` at the suite level, others nest it under the subtest. Use `find /tmp/<name> -name collector.log -path '*<TestName>*'` if unsure.

### Check Test Results

```bash
# Quick summary
for f in /tmp/<name>/container-logs/integration-test-report-*.xml; do
  echo "=== $(basename $f) ==="; head -3 "$f"
done

# Find which tests failed
grep -B2 '<failure' /tmp/<name>/container-logs/integration-test-report-*.xml
```

## Failure Modes

### 1. BPF Loading Failure (Collector Crashes on Startup)

**Symptoms**: Collector exits with non-zero code (132=SIGILL, 134=SIGABRT, 139=SIGSEGV). Low test count in JUnit XML with most tests skipped. Stack trace shows `sinsp_exception` / `scap_init`.

**Three sub-types:**

#### a) BPF Verifier Rejection

```bash
grep -n "failed to load\|BEGIN PROG LOAD LOG\|END PROG LOAD LOG" collector.log
grep -B5 "END PROG LOAD LOG" collector.log  # Rejection reason
```

Common rejection messages:
- `R2 min value is negative` — signed value used as size arg to bpf helper
- `BPF program is too large. Processed 1000001 insn` — exceeded 1M instruction limit
- `R0 invalid mem access 'map_value_or_null'` — clang optimized away a null check
- `invalid size of register fill` / 32-bit sub-register issues — kernel 4.18 loses bounds on `w` register moves (see BPF Verifier Notes below)

#### b) CO-RE Relocation Failure

```bash
grep "invalid CO-RE relocation" collector.log
```

Means the BPF program references a kernel struct/field that doesn't exist in the running kernel's BTF. Example: `struct open_how` (used by `openat2`) doesn't exist on kernels < 5.6. Fix: disable autoloading for the affected program in `lifecycle.c`.

#### c) Map Allocation Failure (ENOMEM)

```bash
grep "Cannot allocate memory\|ENOMEM" collector.log
```

`bpf_prob.rodata: failed to set initial contents: Cannot allocate memory` — the BPF skeleton's read-only data map can't be allocated. Usually **transient memory pressure** on the CI runner, not a code bug. Re-run the test first. If persistent, check if `.rodata` section grew significantly.

### 2. Self-Check Health Timeout

**Symptoms**: BPF programs load successfully but collector never becomes healthy.

```bash
grep -n "SelfCheck\|self-check\|Failed to detect\|healthy" collector.log
```

Check for tracepoint attachment failures or missing programs.

### 3. Test Assertion Failures

**Symptoms**: Most tests pass, individual test fails with assertion mismatch.

Read the JUnit XML for the error message, then check the specific test's `collector.log` and `events.log`.

**Common patterns:**
- **Wrong process name/path**: e.g., expected `"flask"` got `"6"`, expected `/usr/local/bin/flask` got `/dev/fd/6`. This is the **phantom exec** issue — container runtimes use fexecve (`/dev/fd/N`) as an intermediate exec step. Check if the exepath fallback in `parsers.cpp` is filtering these.
- **Missing network connections**: Check `is_socket_failed()` handling in `NetworkSignalHandler.cpp`. UDP sockets marked "failed" on EAGAIN are never unmarked.
- **Endpoint/process count mismatch**: May be a timing issue. Check if the test allows sufficient sleep time.

### 4. Infrastructure Failures

**Symptoms**: `fatal: [<vm>]: FAILED!`, no collector.log, image pull failures, OCI runtime errors, port conflicts.

These are usually **flaky** — re-run the job. Check the Ansible log: `integration-test-<platform>.log`.

## BPF Verifier Notes

### 32-bit Sub-register Bounds (Kernel 4.18)

On kernel 4.18, when clang emits a 32-bit register move (`w2 = w8`), the verifier loses the upper-bound tracking from the source register. A `uint16_t` with `smax_value=15999` gets moved to R2 which then has `umax_value=4294967295`. The `bpf_probe_read` helper rejects this because the size argument appears unbounded.

**Key insight**: Clang is smarter than the verifier. It knows a `uint16_t` can't exceed 65535, so it removes any `& 0xFFFF` mask as a no-op — even through casts to `unsigned long`. The mask never appears in the BPF bytecode. Only `volatile` on the variable prevents this optimization.

**Tradeoff**: `volatile` increases instruction count. Verify affected programs don't exceed 1M instructions. Check with `grep "Processed.*insn" collector.log`.

### Clang Null-Check Elimination

When multiple `__always_inline` functions each call `bpf_map_lookup_elem()` on the same map, clang deduces that if the first lookup succeeded, subsequent ones can't return NULL. It removes the null checks, but the verifier tracks each lookup independently. Fix: do a single lookup in the caller, pass the pointer down.

## Platform-Specific Notes

| Platform | Kernel | Key Notes |
|---|---|---|
| RHEL 8 | 4.18 | **Most restrictive verifier**. 32-bit register tracking issues. |
| RHCOS 4.12 | 4.18 | Same kernel as RHEL 8. Different container runtime (CRI-O). |
| ppc64le | 4.18 | Same kernel. Container runtime uses fexecve (`/dev/fd/N`). |
| s390x | 4.18 | Same kernel. No ia32 compat. Can hit ENOMEM on constrained runners. |
| RHEL 9 / RHEL SAP | 5.14 | SAP has different config — can hit 1M insn limit when RHEL 9 passes. |
| COS | 6.6 | **Clang-compiled kernel** — RCU/BTF differences vs GCC kernels. |
| Ubuntu 22.04 | 6.8 | Stricter than 24.04. `ubuntu-os` job runs BOTH 22.04 and 24.04. |
| Ubuntu 24.04 | 6.17 | Generally permissive. |
| Flatcar / Fedora CoreOS | latest | Most permissive. If these fail, something is fundamentally broken. |
| ARM64 variants | varies | No ia32 compat. Self-check timeouts can be timing-related. |

## Common Non-Fatal Log Messages

These are expected and harmless:
```
unable to initialize the state table API: failed to find dynamic field 'container_id' in threadinfo
failed to determine tracepoint 'syscalls/sys_enter_connect' perf event ID: No such file or directory
Partial TLS config: CACertPath="", ClientCertPath="", ClientKeyPath=""; will not use TLS
Could not set container filter: proc.vpid is not a valid number
unable to find BPF program '<name>'
```

## Build Exclusion Mechanism

```cmake
# collector/CMakeLists.txt
set(MODERN_BPF_EXCLUDE_PROGS "^(openat2|ppoll|...)$" CACHE STRING "..." FORCE)
```

Matches BPF source file stems. Only works for tail-called programs, NOT for `attached/` programs like TOCTOU mitigations (those need explicit disable in `lifecycle.c`). Only exclude syscalls collector doesn't subscribe to (see `CollectorConfig.h`: `kSyscalls[]`, `kSendRecvSyscalls[]`).

## Cleanup

After analysis, delete downloaded artifacts:
```bash
rm -rf /tmp/*-logs /tmp/*-logs.zip
```
