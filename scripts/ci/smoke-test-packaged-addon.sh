#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

PACKAGE_ADDON_DIR="${1:-$REPO_ROOT/package/addons/piper_plus}"

if [[ ! -d "$PACKAGE_ADDON_DIR" ]]; then
  echo "ERROR: packaged addon directory not found: $PACKAGE_ADDON_DIR" >&2
  exit 1
fi

export PIPER_ADDON_SRC="$PACKAGE_ADDON_DIR"
export PIPER_ADDON_BIN_SRC="$PACKAGE_ADDON_DIR/bin"
export PIPER_FAIL_ON_SKIP_PATTERNS="${PIPER_FAIL_ON_SKIP_PATTERNS:-$'PiperTTS class is unavailable\ntest model bundle is not available'}"
export PIPER_REQUIRE_PASS="${PIPER_REQUIRE_PASS:-1}"

bash "$REPO_ROOT/test/run-tests.sh"
