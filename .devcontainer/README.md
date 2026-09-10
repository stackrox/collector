# Collector Devcontainer

Sandboxed Claude Code environment for developing collector. The agent works
in an isolated git worktree inside a container with no SSH keys — code is
pushed via GitHub MCP only.

## Quick Start

```bash
# 1. Build the image (one time)
docker build --platform linux/$(uname -m | sed 's/x86_64/amd64/;s/aarch64/arm64/') \
  -t collector-dev:test -f .devcontainer/Dockerfile .devcontainer/

# 2. Set environment (add to shell profile)
export CLAUDE_CODE_USE_VERTEX=1
export GOOGLE_CLOUD_PROJECT=<your-gcp-project>
export GOOGLE_CLOUD_LOCATION=us-east5
export ANTHROPIC_VERTEX_PROJECT_ID=<your-gcp-project>
export GITHUB_TOKEN=github_pat_...

# 3. Authenticate GCP
gcloud auth login
gcloud auth application-default login

# 4. Run
.devcontainer/run.sh --interactive                       # manual control
.devcontainer/run.sh "add unit tests for ExternalIPsConfig"  # autonomous
```

## Modes

| Command | Description |
|---------|-------------|
| `run.sh "task"` | Autonomous `/dev-loop`: implement → push → PR → CI loop |
| `run.sh "/skill args"` | Run a specific skill |
| `run.sh --interactive` | Worktree + TUI, you drive |
| `run.sh --local` | Edit working tree directly, no worktree |
| `run.sh --shell` | Shell into the container |

### Options

| Flag | Description |
|------|-------------|
| `--branch <name>` | Custom branch name (default: `claude/agent-<timestamp>`) |
| `--no-tui` | Stream JSON output instead of TUI |
| `--debug` | Verbose MCP/auth logging |

## Skills

| Skill | Purpose |
|-------|---------|
| `/task` | Implement, build, test, format, commit. Stops after commit. |
| `/watch-ci` | Push via MCP, create PR, monitor CI, fix failures until green. |
| `/dev-loop` | Full cycle: `/task` then `/watch-ci` in one run. |

## GitHub Token Setup

Create a **fine-grained Personal Access Token** at:
https://github.com/settings/tokens?type=beta

### Repository access

Select the repositories the agent should work on (e.g., `stackrox/collector`).

### Required permissions

| Permission | Access | Why |
|-----------|--------|-----|
| **Contents** | Read and write | Push files to branches via MCP |
| **Pull requests** | Read and write | Create/update PRs, read PR status |
| **Actions** | Read and write | List workflow runs, get job logs |
| **Commit statuses** | Read-only | Check CI check status |
| **Metadata** | Read-only | Required by GitHub for all PATs |

### Optional permissions

| Permission | Access | Why |
|-----------|--------|-----|
| Issues | Read-only | Read issue context if task references one |
| Discussions | Read-only | Read discussion context |

### Permissions NOT needed

Do not grant these — they are denied in `.claude/settings.json`:

- ~~Administration~~ — agent should not manage repo settings
- ~~Merge queues~~ — agent cannot merge PRs
- ~~Pages~~ — not relevant
- ~~Environments~~ — not relevant
- ~~Secrets~~ — agent should not access repo secrets

## Security Model

| Layer | Protection |
|-------|-----------|
| Container isolation | Agent can't access host filesystem |
| No SSH keys | `git push` fails — only MCP `push_files` works |
| Read-only mounts | gcloud credentials, gitconfig can't be modified |
| MCP deny rules | merge, delete, fork, create-repo, trigger-actions blocked |
| Worktree isolation | Agent works on a separate branch, can't touch your checkout |
| .git mount scoping | Worktree git dir writable, shared objects read-only* |

*Note: `.git` is currently mounted read-write due to submodule init requirements.
The agent can't push (no SSH keys) so risk is limited to local git state.

## Environment Variables

| Variable | Required | Description |
|----------|----------|-------------|
| `GITHUB_TOKEN` | Yes (for MCP) | Fine-grained PAT (see above) |
| `CLAUDE_CODE_USE_VERTEX` | Yes | Set to `1` |
| `GOOGLE_CLOUD_PROJECT` | Yes | GCP project ID |
| `GOOGLE_CLOUD_LOCATION` | Yes | Vertex AI region (e.g., `us-east5`) |
| `ANTHROPIC_VERTEX_PROJECT_ID` | Yes | Usually same as `GOOGLE_CLOUD_PROJECT` |
| `COLLECTOR_DEV_IMAGE` | No | Docker image name (default: `collector-dev:test`) |

## Worktree Management

Worktrees are created in `/tmp/collector-worktrees/` and cleaned up on exit.

```bash
# List active worktrees
git worktree list

# Clean up stale worktrees
git worktree prune

# Remove a specific worktree
git worktree remove /tmp/collector-worktrees/<name>
```

## Rebuilding the Image

```bash
docker build --platform linux/$(uname -m | sed 's/x86_64/amd64/;s/aarch64/arm64/') \
  -t collector-dev:test -f .devcontainer/Dockerfile .devcontainer/

# Clear cached Claude state (MCP registrations, theme, etc.)
docker volume rm collector-dev-claude
```
