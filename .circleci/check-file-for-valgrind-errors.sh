#!/usr/bin/env bash

log_file=$1
use_valgrind=$2
use_helgrind=$3

if [[ "$use_valgrind" == "true" ]]; then
    valgrind_summary_lines="$({ grep 'ERROR SUMMARY' "$log_file" || true; } | wc -l)"
    if ((valgrind_summary_lines == 0)); then
        echo "Error: Valgrind summary not found"
        exit 2
    fi
    valgrind_errors="$(grep 'ERROR SUMMARY' "$log_file" | awk '{print $7}')"
    if ((valgrind_errors > 0)); then
        echo "Found $valgrind_errors Valgrind errors"
        exit 3
    fi
fi

if [[ "$use_helgrind" == "true" ]]; then
    data_race_lines="$({ grep "^==[0-9].*data\ race" "$log_file" || true; } | wc -l)"
    if ((data_race_lines > 0)); then
        echo "Found $data_race_lines possible data races"
        exit 4
    fi
    valgrind_summary_lines="$({ grep 'ERROR SUMMARY' "$log_file" || true; } | wc -l)"
    if ((valgrind_summary_lines > 0)); then
        valgrind_errors="$(grep 'ERROR SUMMARY' "$log_file" | awk '{print $7}')"
        if ((valgrind_errors > 0)); then
            echo "Found $valgrind_errors Valgrind errors"
            exit 3
        fi
    fi
fi
