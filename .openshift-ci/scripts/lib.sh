#!/usr/bin/env bash

# A library of CI related reusable bash functions

die() {
    echo >&2 "$@"
    exit 1
}

get_repo_full_name() {
    if [[ -n "${REPO_OWNER:-}" ]]; then
        # presubmit, postsubmit and batch runs
        # (ref: https://github.com/kubernetes/test-infra/blob/master/prow/jobs.md#job-environment-variables)
        [[ -n "${REPO_NAME:-}" ]] || die "expect: REPO_NAME"
        echo "${REPO_OWNER}/${REPO_NAME}"
    elif [[ -n "${CLONEREFS_OPTIONS:-}" ]]; then
        # periodics - CLONEREFS_OPTIONS exists in binary_build_commands and images.
        local org
        local repo
        org="$(jq -r '.refs[0].org' <<< "${CLONEREFS_OPTIONS}")" || die "invalid CLONEREFS_OPTIONS yaml"
        repo="$(jq -r '.refs[0].repo' <<< "${CLONEREFS_OPTIONS}")" || die "invalid CLONEREFS_OPTIONS yaml"
        if [[ "$org" == "null" ]] || [[ "$repo" == "null" ]]; then
            die "expect: org and repo in CLONEREFS_OPTIONS.refs[0]"
        fi
        echo "${org}/${repo}"
    else
        die "Expect REPO_OWNER/NAME or CLONEREFS_OPTIONS"
    fi
}

pr_has_label() {
    if [[ -z "${1:-}" ]]; then
        die "usage: pr_has_label <expected label> [<pr details>]"
    fi

    local expected_label="$1"
    local pr_details
    local exitstatus=0
    pr_details="${2:-$(get_pr_details)}" || exitstatus="$?"
    if [[ "$exitstatus" != "0" ]]; then
        info "Warning: checking for a label in a non PR context"
        return 1
    fi

    if is_openshift_CI_rehearse_PR; then
        pr_has_label_in_body "${expected_label}" "$pr_details"
    else
        jq '([.labels | .[].name]  // []) | .[]' -r <<< "$pr_details" | grep -qx "${expected_label}"
    fi
}

pr_has_label_in_body() {
    if [[ "$#" -ne 2 ]]; then
        die "usage: pr_has_label_in_body <expected label> <pr details>"
    fi

    local expected_label="$1"
    local pr_details="$2"

    [[ "$(jq -r '.body' <<< "$pr_details")" =~ \/label:[[:space:]]*$expected_label ]]
}

is_in_PR_context() {
    if [[ -n "${PULL_NUMBER:-}" ]]; then
        return 0
    elif [[ -n "${CLONEREFS_OPTIONS:-}" ]]; then
        # bin, test-bin, images
        local pull_request
        pull_request=$(jq -r '.refs[0].pulls[0].number' <<< "$CLONEREFS_OPTIONS" 2>&1) || return 1
        [[ "$pull_request" =~ ^[0-9]+$ ]] && return 0
    fi

    return 1
}

is_openshift_CI_rehearse_PR() {
    [[ "$(get_repo_full_name)" == "openshift/release" ]]
}

get_branch() {
    if is_in_PR_context; then
        pr_details="$(get_pr_details)"
        head_ref="$(jq -r '.head.ref' <<< "$pr_details")"
        echo "${head_ref}"
    elif [[ -n "${CLONEREFS_OPTIONS:-}" ]]; then
        # periodics - CLONEREFS_OPTIONS exists in binary_build_commands and images.
        local base_ref
        base_ref="$(jq -r '.refs[] | select(.repo=="collector") | .base_ref' <<< "${CLONEREFS_OPTIONS}")" || die "invalid CLONEREFS_OPTIONS json"
        if [[ "$base_ref" == "null" ]]; then
            die "expect: base_ref in CLONEREFS_OPTIONS.refs[collector]"
        fi
        echo "${base_ref}"
    elif [[ -n "${PULL_BASE_REF:-}" ]]; then
        # presubmit, postsubmit and batch runs
        # (ref: https://github.com/kubernetes/test-infra/blob/master/prow/jobs.md#job-environment-variables)
        echo "${PULL_BASE_REF}"
    else
        die "Expect PULL_BASE_REF or CLONEREFS_OPTIONS"
    fi
}

# get_pr_details() from GitHub and display the result. Exits 1 if not run in CI in a PR context.
_PR_DETAILS=""
get_pr_details() {
    local pull_request
    local org
    local repo

    if [[ -n "${_PR_DETAILS}" ]]; then
        echo "${_PR_DETAILS}"
        return
    fi

    _not_a_PR() {
        echo '{ "msg": "this is not a PR" }'
        exit 1
    }

    if [[ -n "${JOB_SPEC:-}" ]]; then
        pull_request=$(jq -r '.refs.pulls[0].number' <<< "$JOB_SPEC")
        org=$(jq -r '.refs.org' <<< "$JOB_SPEC")
        repo=$(jq -r '.refs.repo' <<< "$JOB_SPEC")
    elif [[ -n "${CLONEREFS_OPTIONS:-}" ]]; then
        pull_request=$(jq -r '.refs[0].pulls[0].number' <<< "$CLONEREFS_OPTIONS")
        org=$(jq -r '.refs[0].org' <<< "$CLONEREFS_OPTIONS")
        repo=$(jq -r '.refs[0].repo' <<< "$CLONEREFS_OPTIONS")
    else
        echo "Expect a JOB_SPEC or CLONEREFS_OPTIONS"
        exit 2
    fi
    [[ "${pull_request}" == "null" ]] && _not_a_PR

    url="https://api.github.com/repos/${org}/${repo}/pulls/${pull_request}"
    pr_details=$(curl --retry 5 -sS "${url}")
    if [[ "$(jq .id <<< "$pr_details")" == "null" ]]; then
        # A valid PR response is expected at this point
        echo "Invalid response from GitHub: $pr_details"
        exit 2
    fi
    _PR_DETAILS="$pr_details"
    echo "$pr_details"
}

import_creds() {
    shopt -s nullglob
    for cred in /tmp/secret/**/[A-Z]*; do
        export "$(basename "$cred")"="$(cat "$cred")"
    done
}

ensure_secret_exists() {
    local secret_name="$1"

    if [[ -z "${secret_name}" ]]; then
        import_creds
        if [[ -z "${secret_name}" ]]; then
            die "No such secret called ${secret_name}"
        fi
    fi
}

copy_secret_to_file() {
    local secret_name="$1"
    shift
    local destination="$1"
    shift
    local permissions="$1"
    shift

    ensure_secret_exists "$secret_name"

    echo "${secret_name}" > "${destination}"
    chmod "${permissions}" "${destination}"
}

get_secret_content() {
    local secret_name="$1"

    ensure_secret_exists "$secret_name"
    echo "${secret_name}"
}

get_secret_file() {
    local secret_name="$1"

    for cred in /tmp/secrets/**/[A-Z]*; do
        if [[ "$cred" == "${secret_name}$" ]]; then
            echo "${cred}"
            return
        fi
    done

    die "No such secret called $secret_name}"
}

registry_rw_login() {
    if [[ "$#" -ne 1 ]]; then
        die "missing arg. usage: registry_rw_login <registry>"
    fi

    local registry="$1"

    case "$registry" in
        quay.io/rhacs-eng)
            if [[ -z "$QUAY_RHACS_ENG_RW_USERNAME" ]]; then
                echo "QUAY_RHACS_ENG_RW_USERNAME is not defined"
                exit 1
            fi
            if [[ -z "$QUAY_RHACS_ENG_RW_PASSWORD" ]]; then
                echo "QUAY_RHACS_ENG_RW_PASSWORD is not defined"
                exit 1
            fi
            docker login --username "$QUAY_RHACS_ENG_RW_USERNAME" --password-stdin quay.io <<< "$QUAY_RHACS_ENG_RW_PASSWORD"
            ;;
        quay.io/stackrox-io)
            if [[ -z "$QUAY_STACKROX_IO_RW_USERNAME" ]]; then
                echo "QUAY_STACKROX_IO_RW_USERNAME is not defined"
                exit 1
            fi
            if [[ -z "$QUAY_STACKROX_IO_RW_PASSWORD" ]]; then
                echo "QUAY_STACKROX_IO_RW_PASSWORD is not defined"
                exit 1
            fi
            docker login --username "$QUAY_STACKROX_IO_RW_USERNAME" --password-stdin quay.io <<< "$QUAY_STACKROX_IO_RW_PASSWORD"
            ;;
        *)
            echo "Unsupported registry login: $registry"
            ;;
    esac
}
