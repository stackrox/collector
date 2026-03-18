#!/usr/bin/env bash
# Launch Claude Code in the collector devcontainer with a task.
#
# Usage:
#   .devcontainer/run.sh "task description"              Full: worktree + PR + CI loop
#   .devcontainer/run.sh --headless "task description"   Worktree + stream output, no PR
#   .devcontainer/run.sh --interactive                   Worktree + TUI, no PR
#   .devcontainer/run.sh --local ["task"]                Edit working tree directly
#   .devcontainer/run.sh --shell                         Shell into container
#
# Prerequisites:
#   - Docker
#   - gh (GitHub CLI, authenticated — only needed for default task mode)
#   - gcloud auth login && gcloud auth application-default login
#   - CLAUDE_CODE_USE_VERTEX=1 and related env vars (see CLAUDE.md)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
IMAGE="${COLLECTOR_DEV_IMAGE:-collector-dev:test}"
PLUGIN_DIR="/workspace/.claude/plugins/collector-dev"

CLAUDE_INTERACTIVE=(claude --dangerously-skip-permissions --plugin-dir "$PLUGIN_DIR")
CLAUDE_AUTONOMOUS=(claude --dangerously-skip-permissions --plugin-dir "$PLUGIN_DIR" --output-format stream-json --verbose)

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

  git -C "$worktree_dir" push -u origin "$branch" >/dev/null 2>&1

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
      "${CLAUDE_INTERACTIVE[@]}"
    ;;

  --headless|-H)
    shift
    if [[ -z "${1:-}" ]]; then
      echo "Usage: $0 --headless \"task description\"" >&2
      exit 1
    fi
    WORKTREE=$(setup_worktree)
    trap "cleanup_worktree '$WORKTREE'" EXIT
    BRANCH=$(git -C "$WORKTREE" branch --show-current)
    echo "Working in isolated worktree: $WORKTREE" >&2
    echo "Branch: $BRANCH" >&2
    build_docker_args "$WORKTREE"
    docker run "${DOCKER_ARGS[@]}" "$IMAGE" \
      "${CLAUDE_AUTONOMOUS[@]}" -p "$*"
    ;;

  --local|-l)
    shift
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
    WORKTREE=$(setup_worktree)
    trap "cleanup_worktree '$WORKTREE'" EXIT
    echo "Working in isolated worktree: $WORKTREE"
    build_docker_args "$WORKTREE"
    docker run -it "${DOCKER_ARGS[@]}" "$IMAGE" zsh
    ;;

  ""|--help|-h)
    cat <<USAGE
Usage:
  $0 "task"                      Full: worktree + draft PR + CI loop (stream-json)
  $0 --headless "task"           Worktree + stream output, no PR
  $0 --interactive               Worktree + TUI, no PR
  $0 --local ["task"]            Edit working tree directly, TUI
  $0 --shell                     Shell into the container

Environment:
  COLLECTOR_DEV_IMAGE            Docker image (default: collector-dev:test)
  CLAUDE_CODE_USE_VERTEX=1       Enable Vertex AI
  GOOGLE_CLOUD_PROJECT           GCP project ID
  GOOGLE_CLOUD_LOCATION          Vertex AI region (e.g., us-east5)

Prerequisites:
  gh auth login                  GitHub CLI (only for default task mode)
  gcloud auth login              Vertex AI authentication
USAGE
    exit 0
    ;;

  *)
    WORKTREE=$(setup_worktree)
    BRANCH=$(git -C "$WORKTREE" branch --show-current)
    TASK="$*"

    echo "Working in isolated worktree: $WORKTREE" >&2
    echo "Branch: $BRANCH" >&2
    echo "Task: $TASK" >&2
    echo "---" >&2
    echo "Creating draft PR..." >&2
    PR_URL=$(setup_pr "$WORKTREE" "$TASK")
    echo "PR: $PR_URL" >&2
    echo "---" >&2

    trap "cleanup_worktree '$WORKTREE'" EXIT

    build_docker_args "$WORKTREE"
    docker run "${DOCKER_ARGS[@]}" "$IMAGE" \
      "${CLAUDE_AUTONOMOUS[@]}" -p \
      "/collector-dev:task You are working on branch '$BRANCH'. A draft PR has been created at: $PR_URL

Your task: $TASK

The branch is already pushed. Do not create new branches or PRs. Commit and push with git."
    ;;
esac
