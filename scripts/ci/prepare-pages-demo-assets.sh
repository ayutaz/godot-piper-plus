#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
PROJECT_DIR="${PIPER_PAGES_PROJECT_DIR:-$REPO_ROOT/pages_demo}"
PROJECT_GODOT_DIR="$PROJECT_DIR/.godot"
EXTENSION_LIST_DST_PATH="$PROJECT_GODOT_DIR/extension_list.cfg"
ADDON_SRC="${PIPER_ADDON_SRC:-$REPO_ROOT/addons/piper_plus}"
ADDON_BIN_SRC="${PIPER_ADDON_BIN_SRC:-$ADDON_SRC/bin}"
ADDON_DST="$PROJECT_DIR/addons/piper_plus"
ADDON_BIN_DST="$ADDON_DST/bin"
ADDON_DICT_DST="$ADDON_DST/dictionaries"
OPENJTALK_DICT_DST="$PROJECT_DIR/piper_plus_assets/dictionaries/open_jtalk_dic_utf_8-1.11"
MODEL_KEY="${PIPER_PAGES_MODEL_KEY:-multilingual-test-medium}"
MODEL_STAGE_ROOT="$PROJECT_DIR/piper_plus_assets/models/$MODEL_KEY"
MODEL_CACHE_ROOT="${PIPER_PAGES_MODEL_CACHE:-$REPO_ROOT/.cache/pages-demo/models}"
CMUDICT_SRC_PATH="$ADDON_SRC/dictionaries/cmudict_data.json"
CMUDICT_DST_PATH="$ADDON_DICT_DST/cmudict_data.json"
PINYIN_SINGLE_SRC_PATH="$ADDON_SRC/dictionaries/pinyin_single.json"
PINYIN_SINGLE_DST_PATH="$ADDON_DICT_DST/pinyin_single.json"
PINYIN_PHRASES_SRC_PATH="$ADDON_SRC/dictionaries/pinyin_phrases.json"
PINYIN_PHRASES_DST_PATH="$ADDON_DICT_DST/pinyin_phrases.json"
DEFAULT_MODEL_OVERRIDE="$REPO_ROOT/test/project/models/$MODEL_KEY.onnx"
DEFAULT_CONFIG_OVERRIDE="$REPO_ROOT/test/project/models/$MODEL_KEY.onnx.json"

normalize_download_url() {
  local url="$1"

  if [[ "$url" == https://huggingface.co/*/resolve/* ]] && [[ "$url" != *\?* ]]; then
    printf '%s?download=true\n' "$url"
    return
  fi

  printf '%s\n' "$url"
}

if [[ -n "${PYTHON_BIN:-}" ]]; then
  PYTHON_CMD="$PYTHON_BIN"
elif command -v python3 >/dev/null 2>&1; then
  PYTHON_CMD="python3"
elif command -v python >/dev/null 2>&1; then
  PYTHON_CMD="python"
else
  echo "ERROR: python3 or python is required." >&2
  exit 1
fi

verify_sha256() {
  local file_path="$1"
  local expected_sha="$2"

  if [[ -z "$expected_sha" ]]; then
    echo "ERROR: missing sha256 for $(basename "$file_path") in download_catalog.json" >&2
    exit 1
  fi

  "$PYTHON_CMD" - "$file_path" "$expected_sha" <<'PY'
import hashlib
import pathlib
import sys

file_path = pathlib.Path(sys.argv[1])
expected = sys.argv[2].strip().lower()
actual = hashlib.sha256(file_path.read_bytes()).hexdigest()
if actual != expected:
    raise SystemExit(f"sha256 mismatch for {file_path.name}: expected {expected}, got {actual}")
PY
}

for required_file in \
  project.godot \
  main.gd \
  main.tscn \
  export_presets.cfg
do
  if [[ ! -f "$PROJECT_DIR/$required_file" ]]; then
    echo "ERROR: Pages demo project is missing $PROJECT_DIR/$required_file" >&2
    exit 1
  fi
done

if [[ ! -d "$ADDON_SRC" ]]; then
  echo "ERROR: addon source directory not found: $ADDON_SRC" >&2
  exit 1
fi

if [[ ! -d "$ADDON_BIN_SRC" ]]; then
  echo "ERROR: addon bin directory not found: $ADDON_BIN_SRC" >&2
  exit 1
fi

mkdir -p "$ADDON_DST" "$ADDON_BIN_DST" "$ADDON_DICT_DST" "$MODEL_STAGE_ROOT" "$PROJECT_GODOT_DIR" "$MODEL_CACHE_ROOT"
mkdir -p "$ADDON_DST/model_descriptors"

find "$ADDON_DST" -mindepth 1 -maxdepth 1 ! -name 'bin' -exec rm -rf {} +
find "$ADDON_BIN_DST" -mindepth 1 ! -name '.gitignore' -exec rm -rf {} +
find "$PROJECT_DIR/piper_plus_assets" -mindepth 1 -exec rm -rf {} +
mkdir -p "$ADDON_DICT_DST" "$ADDON_DST/model_descriptors" "$MODEL_STAGE_ROOT" "$OPENJTALK_DICT_DST"

for addon_file in \
  LICENSE \
  README.md \
  THIRD_PARTY_LICENSES.txt \
	download_catalog.gd \
	download_catalog.json \
	icon.svg \
	model_descriptor.gd \
	model_descriptors/multilingual-test-medium.json \
	multilingual_sample_text_catalog.gd \
	multilingual_sample_text_catalog.json \
	piper_asset_paths.gd \
  piper_plus.gdextension
do
  if [[ -f "$ADDON_SRC/$addon_file" ]]; then
    cp -f "$ADDON_SRC/$addon_file" "$ADDON_DST/$addon_file"
  fi
done

if [[ -d "$ADDON_BIN_SRC" ]]; then
  find "$ADDON_BIN_SRC" \( -type f -o -type l \) ! -name '.gitignore' -exec cp -a {} "$ADDON_BIN_DST"/ \;
fi

if [[ -f "$CMUDICT_SRC_PATH" ]]; then
  cp -f "$CMUDICT_SRC_PATH" "$CMUDICT_DST_PATH"
fi

if [[ -f "$PINYIN_SINGLE_SRC_PATH" ]]; then
  cp -f "$PINYIN_SINGLE_SRC_PATH" "$PINYIN_SINGLE_DST_PATH"
fi

if [[ -f "$PINYIN_PHRASES_SRC_PATH" ]]; then
  cp -f "$PINYIN_PHRASES_SRC_PATH" "$PINYIN_PHRASES_DST_PATH"
fi

if [[ ! -f "$ADDON_DST/icon.svg" ]]; then
  echo "ERROR: staged addon icon is missing: $ADDON_DST/icon.svg" >&2
  exit 1
fi

resolve_model_bundle() {
  local cache_dir="$MODEL_CACHE_ROOT/$MODEL_KEY"
  local model_override="${PIPER_PAGES_MODEL_PATH:-}"
  local config_override="${PIPER_PAGES_CONFIG_PATH:-}"
  local model_filename="$MODEL_KEY.onnx"
  local config_filename="$MODEL_KEY.onnx.json"

  if [[ -z "$model_override" && -f "$DEFAULT_MODEL_OVERRIDE" ]]; then
    model_override="$DEFAULT_MODEL_OVERRIDE"
  fi

  if [[ -z "$config_override" && -f "$DEFAULT_CONFIG_OVERRIDE" ]]; then
    config_override="$DEFAULT_CONFIG_OVERRIDE"
  fi

  if [[ -n "$model_override" && -f "$model_override" ]]; then
    mkdir -p "$cache_dir"
    cp -f "$model_override" "$cache_dir/$model_filename"
    if [[ -n "$config_override" && -f "$config_override" ]]; then
      cp -f "$config_override" "$cache_dir/$config_filename"
    elif [[ -f "${model_override}.json" ]]; then
      cp -f "${model_override}.json" "$cache_dir/$config_filename"
    fi
  fi

  mkdir -p "$cache_dir"

  "$PYTHON_CMD" - "$ADDON_SRC/download_catalog.json" "$MODEL_KEY" <<'PY' | while IFS=$'\t' read -r url filename sha256; do
import json
import sys

catalog_path, model_key = sys.argv[1], sys.argv[2]
with open(catalog_path, "r", encoding="utf-8") as handle:
    catalog = json.load(handle)

item = catalog["items"].get(model_key)
if not item:
    raise SystemExit(f"model key not found in catalog: {model_key}")

for file_entry in item.get("files", []):
    filename = file_entry.get("filename", "")
    if filename.endswith(".onnx") or filename.endswith(".onnx.json"):
        print(f"{file_entry['url']}\t{filename}\t{file_entry.get('sha256', '')}")
PY
    request_url="$(normalize_download_url "$url")"
    target_path="$cache_dir/$filename"
    if [[ -f "$target_path" ]]; then
      verify_sha256 "$target_path" "$sha256"
      continue
    fi
    curl -L --fail --retry 3 --retry-delay 2 -o "$target_path.tmp" "$request_url"
    verify_sha256 "$target_path.tmp" "$sha256"
    mv "$target_path.tmp" "$target_path"
  done

  if [[ ! -f "$cache_dir/$model_filename" || ! -f "$cache_dir/$config_filename" ]]; then
    echo "ERROR: failed to stage $MODEL_KEY model bundle" >&2
    exit 1
  fi

  printf '%s\n' "$cache_dir"
}

MODEL_CACHE_DIR="$(resolve_model_bundle)"
cp -f "$MODEL_CACHE_DIR/$MODEL_KEY.onnx" "$MODEL_STAGE_ROOT/$MODEL_KEY.onnx"
cp -f "$MODEL_CACHE_DIR/$MODEL_KEY.onnx.json" "$MODEL_STAGE_ROOT/$MODEL_KEY.onnx.json"

bash "$SCRIPT_DIR/stage-openjtalk-dictionary.sh" "$OPENJTALK_DICT_DST"

printf '%s\n' 'res://addons/piper_plus/piper_plus.gdextension' > "$EXTENSION_LIST_DST_PATH"
