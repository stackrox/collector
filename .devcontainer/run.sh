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

check_gh() {
  if ! command -v gh &>/dev/null; then
    echo "ERROR: gh (GitHub CLI) not found. Install: brew install gh" >&2
    exit 1
  fi
  if ! gh auth status &>/dev/null 2>&1; then
    echo "ERROR: gh not authenticated. Run: gh auth login" >&2
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
  local need_gh="${1:-false}"
  check_docker
  check_image
  check_gcloud
  check_vertex_env
  check_git_config
  if [[ "$need_gh" == "true" ]]; then
    check_gh
  fi
}

# --- Worktree isolation ---
setup_worktree() {
  local task_id
  task_id="agent-$(date +%s)-$$"
  local branch="claude/${task_id}"
  local worktree_dir="/tmp/collector-${task_id}"

  git -C "$REPO_ROOT" worktree add -b "$branch" "$worktree_dir" HEAD >/dev/null 2>&1

  # Only init submodules needed for building collector (not builder/third_party)
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

# --- Create branch + draft PR ---
setup_pr() {
  local worktree_dir="$1"
  local task="$2"
  local branch
  branch=$(git -C "$worktree_dir" branch --show-current)

  echo "Pushing branch $branch..." >&2
  if ! git -C "$worktree_dir" push -u origin "$branch" 2>&1 >&2; then
    echo "ERROR: Failed to push branch $branch" >&2
    exit 1
  fi

  echo "Creating draft PR..." >&2
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
)" 2>&1)

  if [[ $? -ne 0 || -z "$pr_url" ]]; then
    echo "ERROR: Failed to create draft PR" >&2
    echo "$pr_url" >&2
    exit 1
  fi

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

# --- Task prompt ---
task_prompt() {
  local branch="$1"
  local task="$2"
  local pr_url="${3:-}"

  local prompt="/collector-dev:task You are working on branch '$branch'."
  if [[ -n "$pr_url" ]]; then
    prompt="$prompt A draft PR has been created at: $pr_url"
  fi
  prompt="$prompt

Your task: $task

The branch is already pushed. Do not create new branches or PRs. Commit and push with git."
  echo "$prompt"
}

# --- Main ---
case "${1:-}" in
  --interactive|-i)
    preflight false
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
    preflight false
    WORKTREE=$(setup_worktree)
    trap "cleanup_worktree '$WORKTREE'" EXIT
    BRANCH=$(git -C "$WORKTREE" branch --show-current)
    TASK="$*"
    echo "Working in isolated worktree: $WORKTREE" >&2
    echo "Branch: $BRANCH" >&2
    echo "Task: $TASK" >&2
    echo "---" >&2
    PROMPT=$(task_prompt "$BRANCH" "$TASK")
    build_docker_args "$WORKTREE"
    docker run "${DOCKER_ARGS[@]}" "$IMAGE" \
      "${CLAUDE_AUTONOMOUS[@]}" -p "$PROMPT"
    ;;

  --local|-l)
    shift
    preflight false
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
  $0 "task"                      Full: worktree + draft PR + CI loop (stream-json)
  $0 --headless "task"           Same workflow, no PR (stream-json)
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
    preflight true
    WORKTREE=$(setup_worktree)
    BRANCH=$(git -C "$WORKTREE" branch --show-current)
    TASK="$*"

    echo "Working in isolated worktree: $WORKTREE" >&2
    echo "Branch: $BRANCH" >&2
    echo "Task: $TASK" >&2
    echo "---" >&2
    PR_URL=$(setup_pr "$WORKTREE" "$TASK")
    echo "PR: $PR_URL" >&2
    echo "---" >&2

    trap "cleanup_worktree '$WORKTREE'" EXIT

    PROMPT=$(task_prompt "$BRANCH" "$TASK" "$PR_URL")
    build_docker_args "$WORKTREE"
    docker run "${DOCKER_ARGS[@]}" "$IMAGE" \
      "${CLAUDE_AUTONOMOUS[@]}" -p "$PROMPT"
    ;;
esac
