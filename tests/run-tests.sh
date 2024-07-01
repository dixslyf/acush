#!/bin/sh

for test_exe in "$@"; do
  test_output="$("$test_exe" 2>&1)"
  exit_code="$?"
  if [ "$exit_code" -eq 0 ]; then
    echo "PASS $(basename "$test_exe")"
  else
    echo "FAIL $(basename "$test_exe") (exit code: $exit_code)"
    echo "$test_output"
  fi
done
