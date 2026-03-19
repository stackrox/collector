#!/usr/bin/env bash
# Launch Claude Code in the collector devcontainer with a task.
#
# Usage:
#   .devcontainer/run.sh "task description"              Autonomous: /task then /watch-ci
#   .devcontainer/run.sh --interactive                   Worktree + TUI
#   .devcontainer/run.sh --local ["task"]                Edit working tree directly
#   .devcontainer/run.sh --shell                         Shell into container
#
# Options:
#   --branch <name>                                      Branch name (default: claude/agent-<timestamp>)
#   --symlink-submodules                                 Mount submodules from main repo (fast, read-only)
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
WORKTREE_BASE="/tmp/collector-worktrees"
SYMLINK_SUBMODULES=false
DEBUG=false
BRANCH_NAME=""

# Parse global flags
ARGS=()
for arg in "$@"; do
  case "$arg" in
    --symlink-submodules) SYMLINK_SUBMODULES=true ;;
    --debug) DEBUG=true ;;
    --branch=*) BRANCH_NAME="${arg#--branch=}" ;;
    --branch) BRANCH_NAME="__NEXT__" ;;
    *)
      if [[ "$BRANCH_NAME" == "__NEXT__" ]]; then
        BRANCH_NAME="$arg"
      else
        ARGS+=("$arg")
      fi
      ;;
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

# --- Worktree ---
setup_worktree() {
  local branch
  if [[ -n "$BRANCH_NAME" ]]; then
    branch="$BRANCH_NAME"
  else
    branch="claude/agent-$(date +%s)-$$"
  fi
  local safe_name="${branch//\//-}"
  local worktree_dir="${WORKTREE_BASE}/${safe_name}"

  mkdir -p "$WORKTREE_BASE"
  git -C "$REPO_ROOT" worktree add -b "$branch" "$worktree_dir" HEAD >/dev/null 2>&1

  if [[ "$SYMLINK_SUBMODULES" != "true" ]]; then
    echo "Initializing submodules..." >&2
    git -C "$worktree_dir" submodule update --init --depth 1 \
      falcosecurity-libs \
      collector/proto/third_party/stackrox \
      >/dev/null 2>&1
  fi

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

  # Mount .git at the same absolute path so worktree .git file resolves
  DOCKER_ARGS+=(-v "$REPO_ROOT/.git:$REPO_ROOT/.git:ro")

  # Optionally mount submodules from main repo instead of cloning
  if [[ "$SYMLINK_SUBMODULES" == "true" ]]; then
    DOCKER_ARGS+=(-v "$REPO_ROOT/falcosecurity-libs:/workspace/falcosecurity-libs:ro")
    DOCKER_ARGS+=(-v "$REPO_ROOT/collector/proto/third_party/stackrox:/workspace/collector/proto/third_party/stackrox:ro")
  fi

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
    WORKTREE=$(setup_worktree)
    trap "cleanup_worktree '$WORKTREE'" EXIT
    BRANCH=$(git -C "$WORKTREE" branch --show-current)
    echo "Working in isolated worktree: $WORKTREE"
    echo "Branch: $BRANCH"
    build_docker_args "$WORKTREE"
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
    WORKTREE=$(setup_worktree)
    trap "cleanup_worktree '$WORKTREE'" EXIT
    echo "Working in isolated worktree: $WORKTREE"
    build_docker_args "$WORKTREE"
    docker run -it "${DOCKER_ARGS[@]}" "$IMAGE" zsh
    ;;

  ""|--help|-h)
    cat <<USAGE
Usage:
  $0 "task"                 Autonomous: implement, PR, CI loop (stream-json)
  $0 --interactive          Worktree + TUI (manual control)
  $0 --local ["task"]       Edit working tree directly, TUI
  $0 --shell                Shell into the container

Options:
  --branch <name>           Branch name (default: claude/agent-<timestamp>)
  --symlink-submodules      Mount submodules from main repo (faster, read-only)
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
      "/task $TASK

When /task completes, run /watch-ci to push, create a PR, and monitor CI until green."
    ;;
esac
