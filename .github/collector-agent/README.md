# Collector Agent

Human-directed agent workflows for Collector issue implementation and
PR revision. Each invocation performs one bounded operation and stops.

## Operations

### spike

Implement a bounded first solution for an existing issue.

```bash
./scripts/collector-agent spike <issue-number> --dry-run
```

### revise

Make at most one revision based on PR feedback.

```bash
./scripts/collector-agent revise <pr-number> --dry-run
```

## Dry-run mode

Both operations currently run in dry-run mode only. They produce
artifacts but do not push, comment, or change any GitHub state.

## Artifacts

Each run produces:

| File | Description |
|---|---|
| `artifacts/request.json` | Prepared request with issue/PR snapshot |
| `artifacts/result.json` | Structured result with status and details |
| `artifacts/summary.md` | Human-readable summary |
| `artifacts/change.patch` | Proposed changes (may be empty) |

## Result statuses

| Status | Exit code | Meaning |
|---|---|---|
| `complete` | 0 | Finished, including no-change revisions |
| `blocked` | 2 | Human direction needed |
| `transient_failure` | 4 | Temporary failure, safe to retry |
| `terminal_failure` | 5 | Contract or setup failure |

Exit code 3 indicates invalid input (bad arguments).

## GitHub Actions

The `collector-agent.yml` workflow provides the same operations via
manual dispatch:

1. Select **spike** or **revise**
2. Enter the issue or PR number
3. The workflow runs read-only and uploads result artifacts

## Request and result contracts

See `examples/` for the JSON schemas used by both operations.

The contracts are backend-independent. The current execution backend
is Claude Code; the contracts support future backends (OpenShell, ACP)
without changing skills or result formats.

## Scope exclusions

The agent will return `blocked` if a task reaches:

- eBPF, BTF, falcosecurity-libs, or kernel probe loading
- capabilities or privileged execution
- Collector/Sensor protocol or event semantics
- lifecycle, threading, or backpressure logic
- build images, dependencies, or release infrastructure
- workflow infrastructure or CI configuration

See `AGENTS.md` for the complete exclusion list.
