#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
PROJECT_DIR="$REPO_ROOT/test/project"
ADDON_SRC="${PIPER_ADDON_SRC:-$REPO_ROOT/addons/piper_plus}"
ADDON_BIN_SRC="${PIPER_ADDON_BIN_SRC:-$ADDON_SRC/bin}"
EXPORT_ROOT="${1:-${PIPER_WEB_EXPORT_ROOT:-$REPO_ROOT/build/web-smoke}}"
PRESETS="${PIPER_WEB_PRESETS:-Web,Web Threads}"
SCENARIOS="${PIPER_WEB_SMOKE_SCENARIOS:-en,ja}"
ENTRY_NAME="${PIPER_WEB_ENTRY_NAME:-piper-plus-tests.html}"
PROJECT_ADDON_DIR="$PROJECT_DIR/addons/piper_plus"

if [[ -z "${GODOT:-}" ]]; then
  echo "ERROR: GODOT is not set" >&2
  exit 1
fi

if ! command -v node >/dev/null 2>&1; then
  echo "ERROR: node is required for browser smoke. Install Node.js and Playwright first." >&2
  exit 1
fi

node "$SCRIPT_DIR/validate-web-smoke-presets.mjs" --project "$PROJECT_DIR"

if [[ ! -d "$ADDON_SRC" ]]; then
  echo "ERROR: addon source directory not found: $ADDON_SRC" >&2
  exit 1
fi

if [[ ! -d "$ADDON_BIN_SRC" ]]; then
  echo "ERROR: addon bin directory not found: $ADDON_BIN_SRC" >&2
  exit 1
fi

stage_web_runtime_payload() {
  local export_dir="$1"
  local export_addon_dir="$export_dir/addons/piper_plus"
  local export_addon_bin_dir="$export_addon_dir/bin"
  local export_addon_dict_dir="$export_addon_dir/dictionaries"
  local export_model_dir="$export_dir/models"
  local export_project_asset_dir="$export_dir/piper_plus_assets"

  if [[ ! -f "$PROJECT_ADDON_DIR/piper_plus.gdextension" ]]; then
    echo "ERROR: staged test project manifest not found: $PROJECT_ADDON_DIR/piper_plus.gdextension" >&2
    exit 1
  fi

  if [[ ! -d "$PROJECT_ADDON_DIR/bin" ]]; then
    echo "ERROR: staged test project bin directory not found: $PROJECT_ADDON_DIR/bin" >&2
    exit 1
  fi

  mkdir -p "$export_addon_bin_dir" "$export_addon_dict_dir"
  rm -rf "$export_model_dir" "$export_project_asset_dir"
  cp -f "$PROJECT_ADDON_DIR/piper_plus.gdextension" "$export_addon_dir/piper_plus.gdextension"
  find "$PROJECT_ADDON_DIR/bin" -mindepth 1 -maxdepth 1 ! -name '.gitignore' -exec cp -a {} "$export_addon_bin_dir"/ \;
  if [[ -d "$PROJECT_ADDON_DIR/dictionaries" ]]; then
    find "$PROJECT_ADDON_DIR/dictionaries" -mindepth 1 -maxdepth 1 -exec cp -a {} "$export_addon_dict_dir"/ \;
  fi
  if [[ -d "$PROJECT_DIR/models" ]]; then
    cp -a "$PROJECT_DIR/models" "$export_model_dir"
  fi
  if [[ -d "$PROJECT_DIR/piper_plus_assets" ]]; then
    cp -a "$PROJECT_DIR/piper_plus_assets" "$export_project_asset_dir"
  fi
}

export PIPER_ADDON_SRC="$ADDON_SRC"
export PIPER_ADDON_BIN_SRC="$ADDON_BIN_SRC"
export PIPER_TEST_STAGE_OPENJTALK_DICTIONARY=1

bash "$REPO_ROOT/test/prepare-assets.sh"

mkdir -p "$EXPORT_ROOT"

IFS=',' read -r -a preset_names <<< "$PRESETS"
IFS=',' read -r -a scenario_names <<< "$SCENARIOS"
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

  stage_web_runtime_payload "$preset_dir"
  node "$SCRIPT_DIR/patch-web-asm-const.mjs" "$preset_dir"

  for scenario_name in "${scenario_names[@]}"; do
    scenario_name="$(printf '%s' "$scenario_name" | sed 's/^[[:space:]]*//; s/[[:space:]]*$//')"
    [[ -n "$scenario_name" ]] || continue

    timeout_ms="240000"
    case "$scenario_name" in
      en)
        timeout_ms="240000"
        ;;
      ja)
        timeout_ms="300000"
        ;;
      *)
        echo "ERROR: unsupported PIPER_WEB_SMOKE_SCENARIOS entry: $scenario_name" >&2
        exit 1
        ;;
    esac

    node "$SCRIPT_DIR/run-web-smoke.mjs" \
      --root "$preset_dir" \
      --entry "$ENTRY_NAME" \
      --label "$preset_name-$scenario_name" \
      --scenario "$scenario_name" \
      --timeout-ms "$timeout_ms"
  done
done
