#!/usr/bin/env bash
# Launch Claude Code in the collector devcontainer with a task.
#
# Usage:
#   .devcontainer/run.sh "task description"              Autonomous: /task then /watch-ci
#   .devcontainer/run.sh --interactive                   Isolated clone + TUI
#   .devcontainer/run.sh --local ["task"]                Edit working tree directly
#   .devcontainer/run.sh --shell                         Shell into container
#
# Options:
#   --skip-submodules                                    Skip submodule init
#   --debug                                              Verbose MCP/auth logging
#
# Prerequisites:
#   - Docker
#   - gcloud auth login && gcloud auth application-default login
#   - CLAUDE_CODE_USE_VERTEX=1 and related env vars (see CLAUDE.md)
#   - GITHUB_TOKEN for GitHub MCP (PR creation, CI status)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
IMAGE="${COLLECTOR_DEV_IMAGE:-collector-dev:test}"
SKIP_SUBMODULES=false
DEBUG=false

# Parse global flags
ARGS=()
for arg in "$@"; do
  case "$arg" in
    --skip-submodules) SKIP_SUBMODULES=true ;;
    --debug) DEBUG=true ;;
    *) ARGS+=("$arg") ;;
  esac
done
set -- "${ARGS[@]+"${ARGS[@]}"}"

CLAUDE_INTERACTIVE=(claude --dangerously-skip-permissions)
CLAUDE_AUTONOMOUS=(claude --dangerously-skip-permissions --output-format stream-json --verbose)

if [[ "$DEBUG" == "true" ]]; then
  CLAUDE_INTERACTIVE+=(--debug)
  CLAUDE_AUTONOMOUS+=(--debug)
fi

# --- Preflight checks ---
check_docker() {
  if ! command -v docker &>/dev/null; then
    echo "ERROR: docker not found." >&2
    exit 1
  fi
  if ! docker info &>/dev/null 2>&1; then
    echo "ERROR: Docker daemon not running." >&2
    exit 1
  fi
}

check_image() {
  if ! docker image inspect "$IMAGE" &>/dev/null 2>&1; then
    echo "ERROR: Image '$IMAGE' not found. Build with:" >&2
    echo "  docker build --platform linux/$(uname -m | sed 's/x86_64/amd64/;s/aarch64/arm64/') -t $IMAGE -f .devcontainer/Dockerfile .devcontainer/" >&2
    exit 1
  fi
}

check_gcloud() {
  if [[ ! -f "$HOME/.config/gcloud/application_default_credentials.json" ]]; then
    echo "ERROR: Run: gcloud auth application-default login" >&2
    exit 1
  fi
}

check_vertex_env() {
  local missing=()
  [[ -z "${CLAUDE_CODE_USE_VERTEX:-}" ]] && missing+=(CLAUDE_CODE_USE_VERTEX)
  [[ -z "${GOOGLE_CLOUD_PROJECT:-}" ]] && missing+=(GOOGLE_CLOUD_PROJECT)
  [[ -z "${GOOGLE_CLOUD_LOCATION:-}" ]] && missing+=(GOOGLE_CLOUD_LOCATION)
  if [[ ${#missing[@]} -gt 0 ]]; then
    echo "ERROR: Missing env vars: ${missing[*]} (see CLAUDE.md)" >&2
    exit 1
  fi
}

preflight() {
  check_docker
  check_image
  check_gcloud
  check_vertex_env
  if [[ -z "${GITHUB_TOKEN:-}" ]]; then
    echo "WARNING: GITHUB_TOKEN not set. GitHub MCP (PR creation, CI) will not work." >&2
  fi
}

# --- Isolated clone (replaces worktree) ---
setup_clone() {
  local task_id="agent-$(date +%s)-$$"
  local branch="claude/${task_id}"
  local clone_dir="/tmp/collector-${task_id}"

  echo "Cloning repo..." >&2
  git clone --local --no-checkout "$REPO_ROOT" "$clone_dir" >/dev/null 2>&1
  # Fix remote to point to GitHub, not the local path
  local github_url
  github_url=$(git -C "$REPO_ROOT" remote get-url origin)
  git -C "$clone_dir" remote set-url origin "$github_url"
  git -C "$clone_dir" checkout -b "$branch" HEAD >/dev/null 2>&1

  if [[ "$SKIP_SUBMODULES" != "true" ]]; then
    echo "Initializing submodules..." >&2
    git -C "$clone_dir" submodule update --init \
      falcosecurity-libs \
      collector/proto/third_party/stackrox \
      >/dev/null 2>&1
  fi

  echo "$clone_dir"
}

cleanup_clone() {
  local clone_dir="$1"
  if [[ -d "$clone_dir" ]]; then
    rm -rf "$clone_dir"
  fi
}

# --- Docker ---
build_docker_args() {
  local workspace="$1"
  DOCKER_ARGS=(
    --rm
    -v "$workspace:/workspace"
    -v "$HOME/.config/gcloud:/home/dev/.config/gcloud:ro"
    -v "$HOME/.gitconfig:/home/dev/.gitconfig:ro"
    -v "collector-dev-claude:/home/dev/.claude"
    -e CLOUDSDK_CONFIG=/home/dev/.config/gcloud
    -e GOOGLE_APPLICATION_CREDENTIALS=/home/dev/.config/gcloud/application_default_credentials.json
    -w /workspace
  )
  for var in CLAUDE_CODE_USE_VERTEX GOOGLE_CLOUD_PROJECT GOOGLE_CLOUD_LOCATION ANTHROPIC_VERTEX_PROJECT_ID GITHUB_TOKEN; do
    if [[ -n "${!var:-}" ]]; then
      DOCKER_ARGS+=(-e "$var=${!var}")
    fi
  done
}

# --- Main ---
case "${1:-}" in
  --interactive|-i)
    preflight
    CLONE=$(setup_clone)
    trap "cleanup_clone '$CLONE'" EXIT
    BRANCH=$(git -C "$CLONE" branch --show-current)
    echo "Working in isolated clone: $CLONE"
    echo "Branch: $BRANCH"
    build_docker_args "$CLONE"
    docker run -it "${DOCKER_ARGS[@]}" "$IMAGE" "${CLAUDE_INTERACTIVE[@]}"
    ;;

  --local|-l)
    shift
    preflight
    build_docker_args "$REPO_ROOT"
    if [[ -z "${1:-}" ]]; then
      docker run -it "${DOCKER_ARGS[@]}" "$IMAGE" "${CLAUDE_INTERACTIVE[@]}"
    else
      docker run -it "${DOCKER_ARGS[@]}" "$IMAGE" "${CLAUDE_INTERACTIVE[@]}" -p "$*"
    fi
    ;;

  --shell|-s)
    check_docker; check_image
    CLONE=$(setup_clone)
    trap "cleanup_clone '$CLONE'" EXIT
    echo "Working in isolated clone: $CLONE"
    build_docker_args "$CLONE"
    docker run -it "${DOCKER_ARGS[@]}" "$IMAGE" zsh
    ;;

  ""|--help|-h)
    cat <<USAGE
Usage:
  $0 "task"                 Autonomous: implement → PR → CI loop (stream-json)
  $0 --interactive          Isolated clone + TUI (manual control)
  $0 --local ["task"]       Edit working tree directly, TUI
  $0 --shell                Shell into the container

Options:
  --skip-submodules         Skip submodule init (faster startup)
  --debug                   Verbose MCP/auth logging

Environment:
  COLLECTOR_DEV_IMAGE       Docker image (default: collector-dev:test)
  GITHUB_TOKEN              Fine-grained PAT for GitHub MCP
  CLAUDE_CODE_USE_VERTEX=1  Enable Vertex AI
  GOOGLE_CLOUD_PROJECT      GCP project ID
  GOOGLE_CLOUD_LOCATION     Vertex AI region (e.g., us-east5)
USAGE
    exit 0
    ;;

  *)
    preflight
    CLONE=$(setup_clone)
    BRANCH=$(git -C "$CLONE" branch --show-current)
    TASK="$*"

    echo "Working in isolated clone: $CLONE" >&2
    echo "Branch: $BRANCH" >&2
    echo "Task: $TASK" >&2
    echo "---" >&2

    trap "cleanup_clone '$CLONE'" EXIT

    build_docker_args "$CLONE"
    docker run "${DOCKER_ARGS[@]}" "$IMAGE" \
      "${CLAUDE_AUTONOMOUS[@]}" -p \
      "/task $TASK

When /task completes, run /watch-ci to push, create a PR, and monitor CI until green."
    ;;
esac
