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
# Prerequisites:
#   - Docker
#   - gcloud auth login && gcloud auth application-default login
#   - CLAUDE_CODE_USE_VERTEX=1 and related env vars (see CLAUDE.md)
#   - GITHUB_TOKEN with repo scope, or gh auth login on host

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
IMAGE="${COLLECTOR_DEV_IMAGE:-collector-dev:test}"

# --- Worktree isolation ---
# Create a temporary git worktree so the agent doesn't touch your checkout.
# The worktree is cleaned up when the container exits.
setup_worktree() {
  local task_id
  task_id="agent-$(date +%s)-$$"
  local branch="claude/${task_id}"
  local worktree_dir="/tmp/collector-${task_id}"

  git -C "$REPO_ROOT" worktree add -b "$branch" "$worktree_dir" HEAD 2>/dev/null
  echo "$worktree_dir"
}

cleanup_worktree() {
  local worktree_dir="$1"
  if [[ -d "$worktree_dir" ]]; then
    local branch
    branch=$(git -C "$worktree_dir" branch --show-current 2>/dev/null || true)
    git -C "$REPO_ROOT" worktree remove --force "$worktree_dir" 2>/dev/null || true
    if [[ -n "$branch" ]]; then
      # Only delete the branch if it was never pushed
      if ! git -C "$REPO_ROOT" config "branch.${branch}.remote" &>/dev/null; then
        git -C "$REPO_ROOT" branch -D "$branch" 2>/dev/null || true
      fi
    fi
  fi
}

# --- Docker args ---
build_docker_args() {
  local workspace="$1"
  local -a args=(
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
      args+=(-e "$var=${!var}")
    fi
  done

  # Forward GitHub token (fine-grained PAT preferred)
  if [[ -n "${GITHUB_TOKEN:-}" ]]; then
    args+=(-e "GITHUB_TOKEN=$GITHUB_TOKEN" -e "GH_TOKEN=$GITHUB_TOKEN")
  elif [[ -d "$HOME/.config/gh" ]]; then
    args+=(-v "$HOME/.config/gh:/home/dev/.config/gh:ro")
  fi

  echo "${args[@]}"
}

# --- Main ---
case "${1:-}" in
  --interactive|-i)
    WORKTREE=$(setup_worktree)
    trap "cleanup_worktree '$WORKTREE'" EXIT
    echo "Working in isolated worktree: $WORKTREE"
    echo "Branch: $(git -C "$WORKTREE" branch --show-current)"
    DOCKER_ARGS=$(build_docker_args "$WORKTREE")
    eval docker run -it $DOCKER_ARGS "$IMAGE" \
      claude --dangerously-skip-permissions
    ;;

  --shell|-s)
    WORKTREE=$(setup_worktree)
    trap "cleanup_worktree '$WORKTREE'" EXIT
    echo "Working in isolated worktree: $WORKTREE"
    DOCKER_ARGS=$(build_docker_args "$WORKTREE")
    eval docker run -it $DOCKER_ARGS "$IMAGE" zsh
    ;;

  --no-worktree)
    # Run directly on the repo (no isolation, for debugging)
    shift
    DOCKER_ARGS=$(build_docker_args "$REPO_ROOT")
    if [[ -z "${1:-}" ]]; then
      eval docker run -it $DOCKER_ARGS "$IMAGE" \
        claude --dangerously-skip-permissions
    else
      eval docker run $DOCKER_ARGS "$IMAGE" \
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
  GITHUB_TOKEN                   Fine-grained PAT for push/PR (recommended)
  CLAUDE_CODE_USE_VERTEX=1       Enable Vertex AI
  GOOGLE_CLOUD_PROJECT           GCP project ID
  GOOGLE_CLOUD_LOCATION          Vertex AI region (e.g., us-east5)

GitHub Token:
  Create a fine-grained PAT at https://github.com/settings/tokens?type=beta
  Repository: stackrox/collector
  Permissions: Contents (write), Pull requests (write), Actions (read)
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

    # Cleanup worktree on exit, but only delete branch if it wasn't pushed
    trap "cleanup_worktree '$WORKTREE'" EXIT

    DOCKER_ARGS=$(build_docker_args "$WORKTREE")
    eval docker run $DOCKER_ARGS "$IMAGE" \
      claude --dangerously-skip-permissions -p "$*"
    ;;
esac
