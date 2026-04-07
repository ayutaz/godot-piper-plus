#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
PROJECT_DIR="$REPO_ROOT/test/project"
ADDON_SRC="${PIPER_ADDON_SRC:-$REPO_ROOT/addons/piper_plus}"
ADDON_BIN_SRC="${PIPER_ADDON_BIN_SRC:-$ADDON_SRC/bin}"
EXPORT_ROOT="${1:-${PIPER_WEB_EXPORT_ROOT:-$REPO_ROOT/build/web-smoke}}"
PRESETS="${PIPER_WEB_PRESETS:-Web,Web Threads}"
ENTRY_NAME="${PIPER_WEB_ENTRY_NAME:-piper-plus-tests.html}"

if [[ -z "${GODOT:-}" ]]; then
  echo "ERROR: GODOT is not set" >&2
  exit 1
fi

if ! command -v node >/dev/null 2>&1; then
  echo "ERROR: node is required for browser smoke. Install Node.js and Playwright first." >&2
  exit 1
fi

if [[ ! -d "$ADDON_SRC" ]]; then
  echo "ERROR: addon source directory not found: $ADDON_SRC" >&2
  exit 1
fi

if [[ ! -d "$ADDON_BIN_SRC" ]]; then
  echo "ERROR: addon bin directory not found: $ADDON_BIN_SRC" >&2
  exit 1
fi

export PIPER_ADDON_SRC="$ADDON_SRC"
export PIPER_ADDON_BIN_SRC="$ADDON_BIN_SRC"

bash "$REPO_ROOT/test/prepare-assets.sh"

mkdir -p "$EXPORT_ROOT"

IFS=',' read -r -a preset_names <<< "$PRESETS"
for preset_name in "${preset_names[@]}"; do
  preset_name="$(printf '%s' "$preset_name" | sed 's/^[[:space:]]*//; s/[[:space:]]*$//')"
  [[ -n "$preset_name" ]] || continue

  preset_slug="$(printf '%s' "$preset_name" | tr '[:upper:]' '[:lower:]' | tr ' ' '-')"
  preset_dir="$EXPORT_ROOT/$preset_slug"
  export_path="$preset_dir/$ENTRY_NAME"

  mkdir -p "$preset_dir"
  rm -f "$export_path"

  "$GODOT" --headless --path "$PROJECT_DIR" --export-release "$preset_name" "$export_path"

  if [[ ! -f "$export_path" ]]; then
    echo "ERROR: web export for preset '$preset_name' did not produce $export_path" >&2
    exit 1
  fi

  node "$SCRIPT_DIR/run-web-smoke.mjs" \
    --root "$preset_dir" \
    --entry "$ENTRY_NAME" \
    --label "$preset_name"
done
