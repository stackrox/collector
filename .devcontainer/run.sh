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
#   --no-tui                                             Stream JSON instead of TUI
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
DEBUG=false
NO_TUI=false
BRANCH_NAME=""
ACTIVE_WORKTREE=""

# Parse global flags
ARGS=()
for arg in "$@"; do
    case "$arg" in
        --debug) DEBUG=true ;;
        --no-tui) NO_TUI=true ;;
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

CLAUDE_CMD=(claude --dangerously-skip-permissions)

if [[ "$DEBUG" == "true" ]]; then
    CLAUDE_CMD+=(--debug)
fi

if [[ "$NO_TUI" == "true" ]]; then
    CLAUDE_CMD+=(--output-format stream-json --verbose)
fi

# --- Preflight checks ---
check_docker() {
    if ! command -v docker &> /dev/null; then
        echo "ERROR: docker not found." >&2
        exit 1
    fi
    if ! docker info &> /dev/null 2>&1; then
        echo "ERROR: Docker daemon not running." >&2
        exit 1
    fi
}

check_image() {
    if ! docker image inspect "$IMAGE" &> /dev/null 2>&1; then
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
    git -C "$REPO_ROOT" worktree add -b "$branch" "$worktree_dir" HEAD > /dev/null 2>&1

    # Make worktree git dir writable by container user (different uid)
    local worktree_git_name
    worktree_git_name=$(basename "$worktree_dir")
    chmod -R a+rwX "$REPO_ROOT/.git/worktrees/$worktree_git_name"

    echo "Initializing submodules..." >&2
    git -C "$worktree_dir" submodule update --init --depth 1 \
        falcosecurity-libs \
        collector/proto/third_party/stackrox \
        2>&1 | sed 's/^/  /' >&2

    echo "$worktree_dir"
}

cleanup_worktree() {
    local worktree_dir="$1"
    if [[ -d "$worktree_dir" ]]; then
        local branch
        branch=$(git -C "$worktree_dir" branch --show-current 2> /dev/null || true)
        git -C "$REPO_ROOT" worktree remove --force "$worktree_dir" 2> /dev/null || true
        if [[ -n "$branch" ]]; then
            if ! git -C "$REPO_ROOT" config "branch.${branch}.remote" &> /dev/null; then
                git -C "$REPO_ROOT" branch -D "$branch" 2> /dev/null || true
            fi
        fi
    fi
}

on_exit() {
    if [[ -n "$ACTIVE_WORKTREE" ]]; then
        cleanup_worktree "$ACTIVE_WORKTREE"
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

    # Mount .git so worktree resolves (agent can't push — no SSH keys)
    DOCKER_ARGS+=(-v "$REPO_ROOT/.git:$REPO_ROOT/.git")

    for var in CLAUDE_CODE_USE_VERTEX GOOGLE_CLOUD_PROJECT GOOGLE_CLOUD_LOCATION ANTHROPIC_VERTEX_PROJECT_ID GITHUB_TOKEN; do
        if [[ -n "${!var:-}" ]]; then
            DOCKER_ARGS+=(-e "$var=${!var}")
        fi
    done
}

# --- Main ---
case "${1:-}" in
    --interactive | -i)
        preflight
        ACTIVE_WORKTREE=$(setup_worktree)
        trap on_exit EXIT
        BRANCH=$(git -C "$ACTIVE_WORKTREE" branch --show-current)
        echo "Working in isolated worktree: $ACTIVE_WORKTREE"
        echo "Branch: $BRANCH"
        build_docker_args "$ACTIVE_WORKTREE"
        docker run -it "${DOCKER_ARGS[@]}" "$IMAGE" "${CLAUDE_CMD[@]}"
        ;;

    --local | -l)
        shift
        if [[ -n "$BRANCH_NAME" ]]; then
            echo "ERROR: --branch cannot be used with --local" >&2
            exit 1
        fi
        preflight
        build_docker_args "$REPO_ROOT"
        if [[ -z "${1:-}" ]]; then
            docker run -it "${DOCKER_ARGS[@]}" "$IMAGE" "${CLAUDE_CMD[@]}"
        else
            docker run -it "${DOCKER_ARGS[@]}" "$IMAGE" "${CLAUDE_CMD[@]}" -p "$*"
        fi
        ;;

    --shell | -s)
        check_docker
        check_image
        ACTIVE_WORKTREE=$(setup_worktree)
        trap on_exit EXIT
        echo "Working in isolated worktree: $ACTIVE_WORKTREE"
        build_docker_args "$ACTIVE_WORKTREE"
        docker run -it "${DOCKER_ARGS[@]}" "$IMAGE" zsh
        ;;

    "" | --help | -h)
        cat << USAGE
Usage:
  $0 "task"                 Run /dev-loop with TUI (implement → PR → CI green)
  $0 "/skill args"          Run a specific skill with TUI
  $0 --interactive          Worktree + TUI (no task, manual control)
  $0 --local ["task"]       Edit working tree directly, TUI
  $0 --shell                Shell into the container

Options:
  --branch <name>           Branch name (default: claude/agent-<timestamp>)
  --no-tui                  Stream JSON output instead of TUI
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
        ACTIVE_WORKTREE=$(setup_worktree)
        trap on_exit EXIT
        BRANCH=$(git -C "$ACTIVE_WORKTREE" branch --show-current)
        TASK="$*"

        # Push branch for /dev-loop so the agent can use GitHub MCP push_files
        if [[ "$TASK" != /* ]]; then
            echo "Pushing branch $BRANCH..." >&2
            git -C "$ACTIVE_WORKTREE" push -u origin "$BRANCH" > /dev/null 2>&1
        fi

        echo "Working in isolated worktree: $ACTIVE_WORKTREE"
        echo "Branch: $BRANCH"
        echo "Task: $TASK"
        echo "---"

        build_docker_args "$ACTIVE_WORKTREE"
        PROMPT="/dev-loop $TASK"
        if [[ "$TASK" == /* ]]; then
            PROMPT="$TASK"
        fi

        if [[ "$NO_TUI" == "true" ]]; then
            docker run "${DOCKER_ARGS[@]}" "$IMAGE" "${CLAUDE_CMD[@]}" -p "$PROMPT"
        else
            docker run -it "${DOCKER_ARGS[@]}" "$IMAGE" "${CLAUDE_CMD[@]}" -p "$PROMPT"
        fi
        ;;
esac
