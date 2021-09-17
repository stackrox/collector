VALGRIND_UNIT_TESTS=$1

if [[ "$VALGRIND_UNIT_TESTS" == "true" ]]; then
  valgrind_summary_lines="$({ grep 'ERROR SUMMARY' "$UNITTEST_OUTPUT_FILE" || true ; } | wc -l)"
  if (( valgrind_summary_lines == 0 )); then
    echo "Error: Valgrind summary not found"
    exit 2
  fi
  valgrind_errors="$(grep 'ERROR SUMMARY' "$UNITTEST_OUTPUT_FILE" | awk '{print $7}')"
  if (( valgrind_errors > 0 ))
  then
    echo "Found $valgrind_errors Valgrind errors"
    exit 3
  fi
fi

