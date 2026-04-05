#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_DIR="$SCRIPT_DIR/project"
ADDON_SRC="${PIPER_ADDON_SRC:-$REPO_ROOT/addons/piper_plus}"
ADDON_DST="$PROJECT_DIR/addons/piper_plus"
ADDON_EXTENSION_SRC="$ADDON_SRC/piper_plus.gdextension"
ADDON_EXTENSION_DST="$ADDON_DST/piper_plus.gdextension"
ADDON_BIN_SRC="${PIPER_ADDON_BIN_SRC:-$ADDON_SRC/bin}"
ADDON_BIN_DST="$ADDON_DST/bin"
ADDON_DICT_DST="$ADDON_DST/dictionaries"
TEST_MODEL_SRC_DIR="$SCRIPT_DIR/models"
MODEL_DST_DIR="$PROJECT_DIR/models"
MODEL_DST_PATH="$MODEL_DST_DIR/multilingual-test-medium.onnx"
CONFIG_DST_PATH="$MODEL_DST_DIR/multilingual-test-medium.onnx.json"
DICT_DST_PATH="$MODEL_DST_DIR/openjtalk_dic"
ADDON_MODEL_DST_DIR="$ADDON_DST/models/multilingual-test-medium"
ADDON_MODEL_DST_PATH="$ADDON_MODEL_DST_DIR/multilingual-test-medium.onnx"
ADDON_CONFIG_DST_PATH="$ADDON_MODEL_DST_DIR/multilingual-test-medium.onnx.json"
BUNDLED_MODEL_SRC_PATH="$TEST_MODEL_SRC_DIR/multilingual-test-medium.onnx"
BUNDLED_CONFIG_SRC_PATH="$TEST_MODEL_SRC_DIR/multilingual-test-medium.onnx.json"
BUNDLED_DICT_SRC_PATH="$TEST_MODEL_SRC_DIR/openjtalk_dic"
CMUDICT_SRC_PATH="$ADDON_SRC/dictionaries/cmudict_data.json"
CMUDICT_DST_PATH="$ADDON_DICT_DST/cmudict_data.json"

mkdir -p "$ADDON_DST" "$ADDON_BIN_DST" "$ADDON_DICT_DST" "$MODEL_DST_DIR" "$ADDON_MODEL_DST_DIR"

find "$ADDON_DST" -mindepth 1 -maxdepth 1 ! -name 'bin' -exec rm -rf {} +
mkdir -p "$ADDON_DICT_DST" "$ADDON_MODEL_DST_DIR"

for addon_file in \
  plugin.cfg \
  piper_plus_plugin.gd \
  piper_plus_plugin.gd.uid \
  model_downloader.gd \
  model_downloader.gd.uid \
  dictionary_editor.gd \
  dictionary_editor.gd.uid \
  piper_plus.gdextension \
  piper_plus.gdextension.uid \
  piper_tts_inspector_plugin.gd \
  piper_tts_inspector_plugin.gd.uid \
  test_speech_dialog.gd \
  test_speech_dialog.gd.uid \
  icon.svg \
  README.md \
  LICENSE \
  THIRD_PARTY_LICENSES.txt
do
  if [[ -f "$ADDON_SRC/$addon_file" ]]; then
    cp -f "$ADDON_SRC/$addon_file" "$ADDON_DST/$addon_file"
  fi
done

if [[ -f "$ADDON_EXTENSION_SRC" ]]; then
  cp -f "$ADDON_EXTENSION_SRC" "$ADDON_EXTENSION_DST"
fi

if [[ -d "$ADDON_BIN_SRC" ]]; then
  find "$ADDON_BIN_DST" -mindepth 1 ! -name '.gitignore' -exec rm -rf {} +
  find "$ADDON_BIN_SRC" -type f ! -name '.gitignore' -exec cp -f {} "$ADDON_BIN_DST"/ \;
  if [[ ! -f "$ADDON_BIN_DST/onnxruntime.dll" && -f "$ADDON_BIN_DST/onnxruntime.windows.x86_64.dll" ]]; then
    cp -f "$ADDON_BIN_DST/onnxruntime.windows.x86_64.dll" "$ADDON_BIN_DST/onnxruntime.dll"
  fi

  # GitHub artifact download normalizes file modes, so restore execute bits for native binaries.
  find "$ADDON_BIN_DST" -maxdepth 1 -type f \
    \( -name '*.so' -o -name '*.so.*' -o -name '*.dylib' -o -name '*.dll' \) \
    -exec chmod +x {} + 2>/dev/null || true
fi

find "$MODEL_DST_DIR" -mindepth 1 ! -name '.gitkeep' -exec rm -rf {} +

if [[ -f "$BUNDLED_MODEL_SRC_PATH" ]]; then
  cp -f "$BUNDLED_MODEL_SRC_PATH" "$MODEL_DST_PATH"
  cp -f "$BUNDLED_MODEL_SRC_PATH" "$ADDON_MODEL_DST_PATH"
fi

if [[ -f "$BUNDLED_CONFIG_SRC_PATH" ]]; then
  cp -f "$BUNDLED_CONFIG_SRC_PATH" "$CONFIG_DST_PATH"
  cp -f "$BUNDLED_CONFIG_SRC_PATH" "$ADDON_CONFIG_DST_PATH"
fi

if [[ -d "$BUNDLED_DICT_SRC_PATH" ]]; then
  mkdir -p "$DICT_DST_PATH"
  cp -R "$BUNDLED_DICT_SRC_PATH"/. "$DICT_DST_PATH"/
fi

if [[ -n "${PIPER_TEST_MODEL_PATH:-}" && -f "${PIPER_TEST_MODEL_PATH}" ]]; then
  cp -f "${PIPER_TEST_MODEL_PATH}" "$MODEL_DST_PATH"
  cp -f "${PIPER_TEST_MODEL_PATH}" "$ADDON_MODEL_DST_PATH"
fi

if [[ -n "${PIPER_TEST_CONFIG_PATH:-}" && -f "${PIPER_TEST_CONFIG_PATH}" ]]; then
  cp -f "${PIPER_TEST_CONFIG_PATH}" "$CONFIG_DST_PATH"
  cp -f "${PIPER_TEST_CONFIG_PATH}" "$ADDON_CONFIG_DST_PATH"
elif [[ -n "${PIPER_TEST_MODEL_PATH:-}" && -f "${PIPER_TEST_MODEL_PATH}.json" ]]; then
  cp -f "${PIPER_TEST_MODEL_PATH}.json" "$CONFIG_DST_PATH"
  cp -f "${PIPER_TEST_MODEL_PATH}.json" "$ADDON_CONFIG_DST_PATH"
fi

if [[ -n "${PIPER_TEST_DICT_PATH:-}" && -d "${PIPER_TEST_DICT_PATH}" ]]; then
  rm -rf "$DICT_DST_PATH"
  mkdir -p "$DICT_DST_PATH"
  cp -R "${PIPER_TEST_DICT_PATH}"/. "$DICT_DST_PATH"/
fi

if [[ -f "$CMUDICT_SRC_PATH" ]]; then
  cp -f "$CMUDICT_SRC_PATH" "$CMUDICT_DST_PATH"
fi
