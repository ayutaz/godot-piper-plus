#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_DIR="$SCRIPT_DIR/project"
PREPARE_SCRIPT="$SCRIPT_DIR/prepare-assets.sh"

END_STRING="==== TESTS FINISHED ===="
FAILURE_STRING="******** FAILED ********"

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

"$PREPARE_SCRIPT"

set +e
OUTPUT="$("$GODOT_BIN" --path "$PROJECT_DIR" --headless --quit 2>&1)"
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

exit 0
