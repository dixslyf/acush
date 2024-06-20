#!/bin/sh

for test_exe in "$@"; do
  if "$test_exe"; then
    echo "PASS $(basename "$test_exe")"
  else
    echo "FAIL $(basename "$test_exe")"
  fi
done
