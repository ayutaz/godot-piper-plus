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
  "$PACKAGE_ADDON_DIR/download_catalog.gd"
  "$PACKAGE_ADDON_DIR/download_catalog.json"
  "$PACKAGE_ADDON_DIR/dictionary_editor.gd"
  "$PACKAGE_ADDON_DIR/piper_asset_paths.gd"
  "$PACKAGE_ADDON_DIR/piper_tts_inspector_plugin.gd"
  "$PACKAGE_ADDON_DIR/preset_service.gd"
  "$PACKAGE_ADDON_DIR/preview_controller.gd"
  "$PACKAGE_ADDON_DIR/test_speech_dialog.gd"
  "$PACKAGE_ADDON_DIR/icon.svg"
  "$PACKAGE_ADDON_DIR/README.md"
  "$PACKAGE_ADDON_DIR/LICENSE"
  "$PACKAGE_ADDON_DIR/THIRD_PARTY_LICENSES.txt"
  "$GDEXTENSION_FILE"
)

forbidden_paths=(
  "$PACKAGE_ADDON_DIR/models"
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

for forbidden in "${forbidden_paths[@]}"; do
  if [[ -e "$forbidden" ]]; then
    echo "ERROR: forbidden payload is present in package: $forbidden" >&2
    exit 1
  fi
done

if find "$PACKAGE_ADDON_DIR" -mindepth 1 \( -path '*/naist-jdic*' -o -name 'naist-jdic*' \) | grep -q .; then
  echo "ERROR: forbidden naist-jdic payload is present in package" >&2
  exit 1
fi

if find "$PACKAGE_ADDON_DIR" -mindepth 1 \( -path '*/open_jtalk_dic*' -o -name 'open_jtalk_dic*' \) | grep -q .; then
  echo "ERROR: forbidden OpenJTalk dictionary payload is present in package" >&2
  exit 1
fi

if find "$PACKAGE_ADDON_DIR" -type f \( -iname 'openjtalk_native*' -o -iname 'openjtalk-native*' \) | grep -q .; then
  echo "ERROR: forbidden openjtalk-native payload is present in package" >&2
  exit 1
fi

collect_manifest_bin_files() {
  local gdextension_file="$1"

  grep -o 'res://addons/piper_plus/bin/[^"]*' "$gdextension_file" \
    | sed 's#res://addons/piper_plus/bin/##' \
    | sort -u
}

validate_web_matrix() {
  local gdextension_file="$1"
  local -a web_keys=(
    "web.debug.threads.wasm32"
    "web.release.threads.wasm32"
    "web.debug.wasm32"
    "web.release.wasm32"
  )
  local key=""
  local web_entry_count=0

  web_entry_count="$(grep -c '^web\.' "$gdextension_file" || true)"
  if [[ "${web_entry_count}" -eq 0 ]]; then
    return 0
  fi

  for key in "${web_keys[@]}"; do
    if ! grep -q "^${key}[[:space:]]*=" "$gdextension_file"; then
      echo "ERROR: Web manifest matrix is incomplete, missing ${key}" >&2
      exit 1
    fi
  done
}

required_binaries=()
while IFS= read -r binary_name; do
  [[ -n "$binary_name" ]] && required_binaries+=("$binary_name")
done < <(collect_manifest_bin_files "$GDEXTENSION_FILE")

if [[ ${#required_binaries[@]} -eq 0 ]]; then
  echo "ERROR: no binaries or dependencies were parsed from $GDEXTENSION_FILE" >&2
  exit 1
fi

validate_web_matrix "$GDEXTENSION_FILE"

for binary_name in "${required_binaries[@]}"; do
  if [[ ! -f "$PACKAGE_BIN_DIR/$binary_name" ]]; then
    echo "ERROR: required release/package binary is missing: $PACKAGE_BIN_DIR/$binary_name" >&2
    exit 1
  fi
done

echo "Validated addon package:"
printf '  %s\n' "${required_files[@]}"
printf '  %s\n' "${required_binaries[@]/#/$PACKAGE_BIN_DIR/}"
