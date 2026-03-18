#!/usr/bin/env bash
# Launch Claude Code in the collector devcontainer with a task.
#
# Usage:
#   .devcontainer/run.sh "fix the connect() handler to capture IPv6 scope IDs"
#   .devcontainer/run.sh --interactive
#   .devcontainer/run.sh --shell
#
# The agent works on an isolated git worktree so your working tree is untouched.
# A draft PR is created upfront so the agent only needs to commit and push.
#
# Prerequisites:
#   - Docker
#   - gh (GitHub CLI, authenticated)
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

# --- Create branch + draft PR ---
setup_pr() {
  local worktree_dir="$1"
  local task="$2"
  local branch
  branch=$(git -C "$worktree_dir" branch --show-current)

  # Push the branch
  git -C "$worktree_dir" push -u origin "$branch" >/dev/null 2>&1

  # Create draft PR
  local pr_url
  pr_url=$(gh pr create \
    --repo stackrox/collector \
    --head "$branch" \
    --draft \
    --title "claude: ${task:0:70}" \
    --body "$(cat <<BODY
## Task
${task}

---
*Automated by Claude Code agent. Branch: \`${branch}\`*
BODY
)" 2>&1) || true

  echo "$pr_url"
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
    BRANCH=$(git -C "$WORKTREE" branch --show-current)
    echo "Working in isolated worktree: $WORKTREE"
    echo "Branch: $BRANCH"
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
  $0 "task description"          Run a task autonomously (isolated worktree + draft PR)
  $0 --interactive               Interactive Claude session (isolated worktree)
  $0 --shell                     Shell into the container (isolated worktree)
  $0 --no-worktree [task]        Run directly on repo (no isolation)

Environment:
  COLLECTOR_DEV_IMAGE            Docker image (default: collector-dev:test)
  CLAUDE_CODE_USE_VERTEX=1       Enable Vertex AI
  GOOGLE_CLOUD_PROJECT           GCP project ID
  GOOGLE_CLOUD_LOCATION          Vertex AI region (e.g., us-east5)

Prerequisites:
  gh auth login                  GitHub CLI (for draft PR creation)
  gcloud auth login              Vertex AI authentication
USAGE
    exit 0
    ;;

  *)
    WORKTREE=$(setup_worktree)
    BRANCH=$(git -C "$WORKTREE" branch --show-current)
    TASK="$*"

    echo "Working in isolated worktree: $WORKTREE"
    echo "Branch: $BRANCH"
    echo "Task: $TASK"
    echo "---"
    echo "Creating draft PR..."
    PR_URL=$(setup_pr "$WORKTREE" "$TASK")
    echo "PR: $PR_URL"
    echo "---"

    trap "cleanup_worktree '$WORKTREE'" EXIT

    build_docker_args "$WORKTREE"
    docker run "${DOCKER_ARGS[@]}" "$IMAGE" \
      claude --dangerously-skip-permissions -p "You are working on branch '$BRANCH'. A draft PR has been created at: $PR_URL

Your task: $TASK

The branch is already pushed. After making changes, commit and push with git. Use /collector-dev:ci-status to check CI results. Do not create new branches or PRs."
    ;;
esac
