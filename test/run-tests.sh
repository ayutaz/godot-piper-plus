#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_DIR="$SCRIPT_DIR/project"
PREPARE_SCRIPT="$SCRIPT_DIR/prepare-assets.sh"

END_STRING="==== TESTS FINISHED ===="
FAILURE_STRING="******** FAILED ********"
RESULT_PREFIX="RESULT total="

if [[ -n "${GODOT:-}" ]]; then
  GODOT_BIN="$GODOT"
elif command -v godot4 >/dev/null 2>&1; then
  GODOT_BIN="$(command -v godot4)"
elif command -v godot >/dev/null 2>&1; then
  GODOT_BIN="$(command -v godot)"
else
  echo "ERROR: Godot executable not found. Set GODOT=/path/to/godot4."
  exit 1
fi

if [[ -z "${PIPER_FAIL_ON_SKIP_PATTERNS:-}" ]]; then
  export PIPER_FAIL_ON_SKIP_PATTERNS=$'PiperTTS class is unavailable\ntest model bundle is not available'
fi

if [[ -z "${PIPER_REQUIRE_PASS:-}" ]]; then
  export PIPER_REQUIRE_PASS=1
fi

"$PREPARE_SCRIPT"

PROJECT_ARG="$PROJECT_DIR"
GODOT_BIN_LOWER="$(printf '%s' "$GODOT_BIN" | tr '[:upper:]' '[:lower:]')"
if [[ "$GODOT_BIN_LOWER" == *.exe ]]; then
  if command -v cygpath >/dev/null 2>&1; then
    PROJECT_ARG="$(cygpath -w "$PROJECT_DIR")"
  elif command -v wslpath >/dev/null 2>&1; then
    PROJECT_ARG="$(wslpath -w "$PROJECT_DIR")"
  fi
fi

set +e
OUTPUT="$("$GODOT_BIN" --path "$PROJECT_ARG" --headless 2>&1)"
ERRCODE=$?
set -e

echo "$OUTPUT"

if [[ $ERRCODE -ne 0 ]]; then
  exit $ERRCODE
fi

if ! echo "$OUTPUT" | grep -e "$END_STRING" >/dev/null; then
  echo "ERROR: Tests failed to complete"
  exit 1
fi

if echo "$OUTPUT" | grep -e "$FAILURE_STRING" >/dev/null; then
  exit 1
fi

RESULT_LINE="$(echo "$OUTPUT" | grep -e "^$RESULT_PREFIX" | tail -n 1)"
if [[ -z "$RESULT_LINE" ]]; then
  echo "ERROR: Test summary was not emitted"
  exit 1
fi

PASS_COUNT="$(echo "$RESULT_LINE" | sed -n 's/.* pass=\([0-9][0-9]*\).*/\1/p')"
if [[ -z "$PASS_COUNT" ]]; then
  echo "ERROR: Could not parse pass count from summary: $RESULT_LINE"
  exit 1
fi

if [[ "$PASS_COUNT" -eq 0 ]]; then
  echo "ERROR: Test run completed without any passing tests"
  exit 1
fi

exit 0
