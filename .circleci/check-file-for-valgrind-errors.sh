#!/usr/bin/env bash

log_file=$1
use_valgrind=$2

if [[ "$use_valgrind" == "true" ]]; then
  valgrind_summary_lines="$({ grep 'ERROR SUMMARY' "$log_file" || true ; } | wc -l)"
  if (( valgrind_summary_lines == 0 )); then
    echo "Error: Valgrind summary not found"
    exit 2
  fi
  valgrind_errors="$(grep 'ERROR SUMMARY' "$log_file" | awk '{print $7}')"
  if (( valgrind_errors > 0 ))
  then
    echo "Found $valgrind_errors Valgrind errors"
    exit 3
  fi
fi

