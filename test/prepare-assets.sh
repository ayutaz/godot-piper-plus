#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_DIR="$SCRIPT_DIR/project"
ADDON_BIN_SRC="$REPO_ROOT/addons/piper_plus/bin"
ADDON_BIN_DST="$PROJECT_DIR/addons/piper_plus/bin"
MODEL_DST_DIR="$PROJECT_DIR/models"
MODEL_DST_PATH="$MODEL_DST_DIR/ja_JP-test-medium.onnx"
CONFIG_DST_PATH="$MODEL_DST_DIR/ja_JP-test-medium.onnx.json"
DICT_DST_PATH="$MODEL_DST_DIR/openjtalk_dic"

mkdir -p "$ADDON_BIN_DST" "$MODEL_DST_DIR"

if [[ -d "$ADDON_BIN_SRC" ]]; then
  find "$ADDON_BIN_DST" -mindepth 1 ! -name '.gitignore' -exec rm -rf {} +
  cp -f "$ADDON_BIN_SRC"/* "$ADDON_BIN_DST"/ 2>/dev/null || true
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
