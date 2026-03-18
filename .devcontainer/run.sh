#!/usr/bin/env bash
# Launch Claude Code in the collector devcontainer with a task.
#
# Usage:
#   .devcontainer/run.sh "fix the connect() handler to capture IPv6 scope IDs"
#   .devcontainer/run.sh --interactive
#   .devcontainer/run.sh --shell
#
# The agent works on an isolated git worktree so your working tree is untouched.
# Changes are pushed to a branch and a PR is created for CI validation.
#
# GitHub access is via the official GitHub MCP server (OAuth, configured in .mcp.json).
# Authenticate once with: /mcp in Claude Code, then select GitHub → Authenticate.
#
# Prerequisites:
#   - Docker
#   - gcloud auth login && gcloud auth application-default login
#   - CLAUDE_CODE_USE_VERTEX=1 and related env vars (see CLAUDE.md)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
IMAGE="${COLLECTOR_DEV_IMAGE:-collector-dev:test}"

# --- Worktree isolation ---
setup_worktree() {
  local task_id
  task_id="agent-$(date +%s)-$$"
  local branch="claude/${task_id}"
  local worktree_dir="/tmp/collector-${task_id}"

  git -C "$REPO_ROOT" worktree add -b "$branch" "$worktree_dir" HEAD >/dev/null 2>&1
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

  # Forward Vertex AI env vars
  for var in CLAUDE_CODE_USE_VERTEX GOOGLE_CLOUD_PROJECT GOOGLE_CLOUD_LOCATION ANTHROPIC_VERTEX_PROJECT_ID; do
    if [[ -n "${!var:-}" ]]; then
      DOCKER_ARGS+=(-e "$var=${!var}")
    fi
  done
}

# --- Main ---
case "${1:-}" in
  --interactive|-i)
    WORKTREE=$(setup_worktree)
    trap "cleanup_worktree '$WORKTREE'" EXIT
    echo "Working in isolated worktree: $WORKTREE"
    echo "Branch: $(git -C "$WORKTREE" branch --show-current)"
    build_docker_args "$WORKTREE"
    docker run -it "${DOCKER_ARGS[@]}" "$IMAGE" \
      claude --dangerously-skip-permissions
    ;;

  --shell|-s)
    WORKTREE=$(setup_worktree)
    trap "cleanup_worktree '$WORKTREE'" EXIT
    echo "Working in isolated worktree: $WORKTREE"
    build_docker_args "$WORKTREE"
    docker run -it "${DOCKER_ARGS[@]}" "$IMAGE" zsh
    ;;

  --no-worktree)
    shift
    build_docker_args "$REPO_ROOT"
    if [[ -z "${1:-}" ]]; then
      docker run -it "${DOCKER_ARGS[@]}" "$IMAGE" \
        claude --dangerously-skip-permissions
    else
      docker run "${DOCKER_ARGS[@]}" "$IMAGE" \
        claude --dangerously-skip-permissions -p "$*"
    fi
    ;;

  ""|--help|-h)
    cat <<USAGE
Usage:
  $0 "task description"          Run a task autonomously (isolated worktree)
  $0 --interactive               Interactive Claude session (isolated worktree)
  $0 --shell                     Shell into the container (isolated worktree)
  $0 --no-worktree [task]        Run directly on repo (no isolation)

Environment:
  COLLECTOR_DEV_IMAGE            Docker image (default: collector-dev:test)
  CLAUDE_CODE_USE_VERTEX=1       Enable Vertex AI
  GOOGLE_CLOUD_PROJECT           GCP project ID
  GOOGLE_CLOUD_LOCATION          Vertex AI region (e.g., us-east5)

GitHub:
  Uses the official GitHub MCP server (OAuth). On first use, run /mcp
  inside Claude Code and authenticate with GitHub.
USAGE
    exit 0
    ;;

  *)
    WORKTREE=$(setup_worktree)
    BRANCH=$(git -C "$WORKTREE" branch --show-current)
    echo "Working in isolated worktree: $WORKTREE"
    echo "Branch: $BRANCH"
    echo "Task: $*"
    echo "---"

    trap "cleanup_worktree '$WORKTREE'" EXIT

    build_docker_args "$WORKTREE"
    docker run "${DOCKER_ARGS[@]}" "$IMAGE" \
      claude --dangerously-skip-permissions -p "$*"
    ;;
esac
