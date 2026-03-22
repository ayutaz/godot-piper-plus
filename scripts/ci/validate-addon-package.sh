#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

PACKAGE_ADDON_DIR="${1:-$REPO_ROOT/package/addons/piper_plus}"
PACKAGE_BIN_DIR="$PACKAGE_ADDON_DIR/bin"
GDEXTENSION_FILE="$PACKAGE_ADDON_DIR/piper_plus.gdextension"

required_files=(
  "$PACKAGE_ADDON_DIR/plugin.cfg"
  "$PACKAGE_ADDON_DIR/piper_plus_plugin.gd"
  "$PACKAGE_ADDON_DIR/model_downloader.gd"
  "$PACKAGE_ADDON_DIR/dictionary_editor.gd"
  "$PACKAGE_ADDON_DIR/piper_tts_inspector_plugin.gd"
  "$PACKAGE_ADDON_DIR/test_speech_dialog.gd"
  "$PACKAGE_ADDON_DIR/icon.svg"
  "$PACKAGE_ADDON_DIR/README.md"
  "$PACKAGE_ADDON_DIR/LICENSE"
  "$PACKAGE_ADDON_DIR/THIRD_PARTY_LICENSES.txt"
  "$GDEXTENSION_FILE"
)

for required in "${required_files[@]}"; do
  if [[ ! -f "$required" ]]; then
    echo "ERROR: required addon file is missing: $required" >&2
    exit 1
  fi
done

if [[ ! -d "$PACKAGE_BIN_DIR" ]]; then
  echo "ERROR: addon bin directory is missing: $PACKAGE_BIN_DIR" >&2
  exit 1
fi

required_binaries=()
while IFS= read -r binary_name; do
  [[ -n "$binary_name" ]] && required_binaries+=("$binary_name")
done < <(
  grep '\.release' "$GDEXTENSION_FILE" \
    | sed -n 's/.*"res:\/\/addons\/piper_plus\/bin\/\([^"]*\)".*/\1/p' \
    | sort -u
)

if [[ ${#required_binaries[@]} -eq 0 ]]; then
  echo "ERROR: no binaries or dependencies were parsed from $GDEXTENSION_FILE" >&2
  exit 1
fi

for binary_name in "${required_binaries[@]}"; do
  if [[ ! -f "$PACKAGE_BIN_DIR/$binary_name" ]]; then
    echo "ERROR: required release/package binary is missing: $PACKAGE_BIN_DIR/$binary_name" >&2
    exit 1
  fi
done

echo "Validated addon package:"
printf '  %s\n' "${required_files[@]}"
printf '  %s\n' "${required_binaries[@]/#/$PACKAGE_BIN_DIR/}"
