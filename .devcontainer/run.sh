#!/usr/bin/env bash
# Launch Claude Code in the collector devcontainer with a task.
#
# Usage:
#   .devcontainer/run.sh "task description"              Worktree + /collector-dev:task (stream-json)
#   .devcontainer/run.sh --interactive                   Worktree + TUI
#   .devcontainer/run.sh --local ["task"]                Edit working tree directly, TUI
#   .devcontainer/run.sh --shell                         Shell into container
#
# Prerequisites:
#   - Docker
#   - gcloud auth login && gcloud auth application-default login
#   - CLAUDE_CODE_USE_VERTEX=1 and related env vars (see CLAUDE.md)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
IMAGE="${COLLECTOR_DEV_IMAGE:-collector-dev:test}"
PLUGIN_DIR="/workspace/.claude/plugins/collector-dev"

CLAUDE_INTERACTIVE=(claude --dangerously-skip-permissions --plugin-dir "$PLUGIN_DIR")
CLAUDE_AUTONOMOUS=(claude --dangerously-skip-permissions --plugin-dir "$PLUGIN_DIR" --output-format stream-json --verbose)

# --- Preflight checks ---
check_docker() {
  if ! command -v docker &>/dev/null; then
    echo "ERROR: docker not found. Install Docker Desktop, OrbStack, or Colima." >&2
    exit 1
  fi
  if ! docker info &>/dev/null 2>&1; then
    echo "ERROR: Docker daemon not running." >&2
    exit 1
  fi
}

check_image() {
  if ! docker image inspect "$IMAGE" &>/dev/null 2>&1; then
    echo "ERROR: Docker image '$IMAGE' not found." >&2
    echo "Build it with: docker build --platform linux/$(uname -m | sed 's/x86_64/amd64/;s/aarch64/arm64/') -t $IMAGE -f .devcontainer/Dockerfile .devcontainer/" >&2
    exit 1
  fi
}

check_gcloud() {
  local adc="$HOME/.config/gcloud/application_default_credentials.json"
  if [[ ! -f "$adc" ]]; then
    echo "ERROR: GCloud application default credentials not found at $adc" >&2
    echo "Run: gcloud auth application-default login" >&2
    exit 1
  fi
}

check_vertex_env() {
  local missing=()
  [[ -z "${CLAUDE_CODE_USE_VERTEX:-}" ]] && missing+=(CLAUDE_CODE_USE_VERTEX)
  [[ -z "${GOOGLE_CLOUD_PROJECT:-}" ]] && missing+=(GOOGLE_CLOUD_PROJECT)
  [[ -z "${GOOGLE_CLOUD_LOCATION:-}" ]] && missing+=(GOOGLE_CLOUD_LOCATION)

  if [[ ${#missing[@]} -gt 0 ]]; then
    echo "ERROR: Missing Vertex AI environment variables: ${missing[*]}" >&2
    echo "Set them in your shell profile (see CLAUDE.md):" >&2
    echo "  export CLAUDE_CODE_USE_VERTEX=1" >&2
    echo "  export GOOGLE_CLOUD_PROJECT=<your-project>" >&2
    echo "  export GOOGLE_CLOUD_LOCATION=<region>  # e.g., us-east5" >&2
    echo "  export ANTHROPIC_VERTEX_PROJECT_ID=<your-project>" >&2
    exit 1
  fi
}

check_git_config() {
  if [[ ! -f "$HOME/.gitconfig" ]]; then
    echo "WARNING: ~/.gitconfig not found. Git operations inside container may fail." >&2
  fi
  if [[ ! -d "$HOME/.ssh" ]]; then
    echo "WARNING: ~/.ssh not found. Git push via SSH will not work." >&2
  fi
}

preflight() {
  check_docker
  check_image
  check_gcloud
  check_vertex_env
  check_git_config
}

# --- Worktree isolation ---
setup_worktree() {
  local task_id
  task_id="agent-$(date +%s)-$$"
  local branch="claude/${task_id}"
  local worktree_dir="/tmp/collector-${task_id}"

  git -C "$REPO_ROOT" worktree add -b "$branch" "$worktree_dir" HEAD >/dev/null 2>&1

  echo "Initializing submodules..." >&2
  git -C "$worktree_dir" submodule update --init \
    falcosecurity-libs \
    collector/proto/third_party/stackrox \
    >/dev/null 2>&1

  echo "$worktree_dir"
}

cleanup_worktree() {
  local worktree_dir="$1"
  if [[ -d "$worktree_dir" ]]; then
    local branch
    branch=$(git -C "$worktree_dir" branch --show-current 2>/dev/null || true)
    git -C "$REPO_ROOT" worktree remove --force "$worktree_dir" 2>/dev/null || true
    if [[ -n "$branch" ]]; then
      if ! git -C "$REPO_ROOT" config "branch.${branch}.remote" &>/dev/null; then
        git -C "$REPO_ROOT" branch -D "$branch" 2>/dev/null || true
      fi
    fi
  fi
}

# --- Docker args ---
build_docker_args() {
  local workspace="$1"
  DOCKER_ARGS=(
    --rm
    -v "$workspace:/workspace"
    -v "$HOME/.config/gcloud:/home/dev/.config/gcloud:ro"
    -v "$HOME/.gitconfig:/home/dev/.gitconfig:ro"
    -v "$HOME/.ssh:/home/dev/.ssh:ro"
    -e CLOUDSDK_CONFIG=/home/dev/.config/gcloud
    -e GOOGLE_APPLICATION_CREDENTIALS=/home/dev/.config/gcloud/application_default_credentials.json
    -w /workspace
  )

  for var in CLAUDE_CODE_USE_VERTEX GOOGLE_CLOUD_PROJECT GOOGLE_CLOUD_LOCATION ANTHROPIC_VERTEX_PROJECT_ID; do
    if [[ -n "${!var:-}" ]]; then
      DOCKER_ARGS+=(-e "$var=${!var}")
    fi
  done
}

# --- Main ---
case "${1:-}" in
  --interactive|-i)
    preflight
    WORKTREE=$(setup_worktree)
    trap "cleanup_worktree '$WORKTREE'" EXIT
    BRANCH=$(git -C "$WORKTREE" branch --show-current)
    echo "Working in isolated worktree: $WORKTREE"
    echo "Branch: $BRANCH"
    build_docker_args "$WORKTREE"
    docker run -it "${DOCKER_ARGS[@]}" "$IMAGE" \
      "${CLAUDE_INTERACTIVE[@]}"
    ;;

  --local|-l)
    shift
    preflight
    build_docker_args "$REPO_ROOT"
    if [[ -z "${1:-}" ]]; then
      docker run -it "${DOCKER_ARGS[@]}" "$IMAGE" \
        "${CLAUDE_INTERACTIVE[@]}"
    else
      docker run -it "${DOCKER_ARGS[@]}" "$IMAGE" \
        "${CLAUDE_INTERACTIVE[@]}" -p "$*"
    fi
    ;;

  --shell|-s)
    check_docker
    check_image
    WORKTREE=$(setup_worktree)
    trap "cleanup_worktree '$WORKTREE'" EXIT
    echo "Working in isolated worktree: $WORKTREE"
    build_docker_args "$WORKTREE"
    docker run -it "${DOCKER_ARGS[@]}" "$IMAGE" zsh
    ;;

  ""|--help|-h)
    cat <<USAGE
Usage:
  $0 "task"                      Run task end-to-end: worktree → implement → PR → CI (stream-json)
  $0 --interactive               Worktree + TUI (manual control)
  $0 --local ["task"]            Edit working tree directly, TUI
  $0 --shell                     Shell into the container

The agent creates the PR via GitHub MCP server — no gh CLI needed.

Environment:
  COLLECTOR_DEV_IMAGE            Docker image (default: collector-dev:test)
  CLAUDE_CODE_USE_VERTEX=1       Enable Vertex AI
  GOOGLE_CLOUD_PROJECT           GCP project ID
  GOOGLE_CLOUD_LOCATION          Vertex AI region (e.g., us-east5)

Prerequisites:
  gcloud auth login              Vertex AI authentication
  gcloud auth application-default login
USAGE
    exit 0
    ;;

  *)
    preflight
    WORKTREE=$(setup_worktree)
    BRANCH=$(git -C "$WORKTREE" branch --show-current)
    TASK="$*"

    echo "Working in isolated worktree: $WORKTREE" >&2
    echo "Branch: $BRANCH" >&2
    echo "Task: $TASK" >&2
    echo "---" >&2

    trap "cleanup_worktree '$WORKTREE'" EXIT

    build_docker_args "$WORKTREE"
    docker run "${DOCKER_ARGS[@]}" "$IMAGE" \
      "${CLAUDE_AUTONOMOUS[@]}" -p \
      "/collector-dev:task You are working on branch '$BRANCH'.

Your task: $TASK

After implementing and testing, push with git and create a draft PR via the GitHub MCP server. Do not use gh CLI."
    ;;
esac
