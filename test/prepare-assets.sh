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
TEST_MODEL_SRC_DIR="$SCRIPT_DIR/models"
MODEL_DST_DIR="$PROJECT_DIR/models"
MODEL_DST_PATH="$MODEL_DST_DIR/multilingual-test-medium.onnx"
CONFIG_DST_PATH="$MODEL_DST_DIR/multilingual-test-medium.onnx.json"
DICT_DST_PATH="$MODEL_DST_DIR/openjtalk_dic"
BUNDLED_MODEL_SRC_PATH="$TEST_MODEL_SRC_DIR/multilingual-test-medium.onnx"
BUNDLED_CONFIG_SRC_PATH="$TEST_MODEL_SRC_DIR/multilingual-test-medium.onnx.json"
BUNDLED_DICT_SRC_PATH="$TEST_MODEL_SRC_DIR/openjtalk_dic"

mkdir -p "$ADDON_DST" "$ADDON_BIN_DST" "$MODEL_DST_DIR"

if [[ -f "$ADDON_EXTENSION_SRC" ]]; then
  cp -f "$ADDON_EXTENSION_SRC" "$ADDON_EXTENSION_DST"
fi

if [[ -d "$ADDON_BIN_SRC" ]]; then
  find "$ADDON_BIN_DST" -mindepth 1 ! -name '.gitignore' -exec rm -rf {} +
  cp -f "$ADDON_BIN_SRC"/* "$ADDON_BIN_DST"/ 2>/dev/null || true
  if [[ ! -f "$ADDON_BIN_DST/onnxruntime.dll" && -f "$ADDON_BIN_DST/onnxruntime.windows.x86_64.dll" ]]; then
    cp -f "$ADDON_BIN_DST/onnxruntime.windows.x86_64.dll" "$ADDON_BIN_DST/onnxruntime.dll"
  fi
fi

find "$MODEL_DST_DIR" -mindepth 1 ! -name '.gitkeep' -exec rm -rf {} +

if [[ -f "$BUNDLED_MODEL_SRC_PATH" ]]; then
  cp -f "$BUNDLED_MODEL_SRC_PATH" "$MODEL_DST_PATH"
fi

if [[ -f "$BUNDLED_CONFIG_SRC_PATH" ]]; then
  cp -f "$BUNDLED_CONFIG_SRC_PATH" "$CONFIG_DST_PATH"
fi

if [[ -d "$BUNDLED_DICT_SRC_PATH" ]]; then
  mkdir -p "$DICT_DST_PATH"
  cp -R "$BUNDLED_DICT_SRC_PATH"/. "$DICT_DST_PATH"/
fi

if [[ -n "${PIPER_TEST_MODEL_PATH:-}" && -f "${PIPER_TEST_MODEL_PATH}" ]]; then
  cp -f "${PIPER_TEST_MODEL_PATH}" "$MODEL_DST_PATH"
fi

if [[ -n "${PIPER_TEST_CONFIG_PATH:-}" && -f "${PIPER_TEST_CONFIG_PATH}" ]]; then
  cp -f "${PIPER_TEST_CONFIG_PATH}" "$CONFIG_DST_PATH"
elif [[ -n "${PIPER_TEST_MODEL_PATH:-}" && -f "${PIPER_TEST_MODEL_PATH}.json" ]]; then
  cp -f "${PIPER_TEST_MODEL_PATH}.json" "$CONFIG_DST_PATH"
fi

if [[ -n "${PIPER_TEST_DICT_PATH:-}" && -d "${PIPER_TEST_DICT_PATH}" ]]; then
  rm -rf "$DICT_DST_PATH"
  mkdir -p "$DICT_DST_PATH"
  cp -R "${PIPER_TEST_DICT_PATH}"/. "$DICT_DST_PATH"/
fi
