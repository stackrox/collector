#!/usr/bin/env bash

log_file=$1
use_valgrind=$2

if [[ "$use_valgrind" == "true" ]]; then
  valgrind_summary_lines="$({ grep 'ERROR SUMMARY' "$log_file" || true ; } | wc -l)"
  valgrind_invalid_errors="$({ grep '== Invalid' "$log_file" || true ; } | wc -l)"
  if (( $valgrind_invalid_error > 0)); then
    echo "Found invalid valgrind error"
    exit 2
  fi
fi

