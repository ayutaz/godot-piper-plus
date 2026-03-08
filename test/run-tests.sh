#!/usr/bin/env bash

set -euo pipefail

GODOT="${GODOT:-godot}"
END_STRING="==== TESTS FINISHED ===="
FAILURE_STRING="******** FAILED ********"

set +e
OUTPUT="$("$GODOT" --path test/project --headless --quit 2>&1)"
ERRCODE=$?
set -e

echo "$OUTPUT"

if ! echo "$OUTPUT" | grep -F "$END_STRING" >/dev/null; then
    echo "ERROR: Tests failed to complete"
    exit 1
fi

if echo "$OUTPUT" | grep -F "$FAILURE_STRING" >/dev/null; then
    exit 1
fi

exit "$ERRCODE"
