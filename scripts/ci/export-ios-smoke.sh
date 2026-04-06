#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
PROJECT_DIR="$REPO_ROOT/test/project"
PACKAGE_ADDON_DIR="${1:-$REPO_ROOT/package/addons/piper_plus}"
EXPORT_DIR="$PROJECT_DIR/build/ios"
EXPORT_NAME="PiperPlusTests"
PROJECT_PATH="$EXPORT_DIR/${EXPORT_NAME}"
XCODE_PROJECT_PATH="${PROJECT_PATH}.xcodeproj"

if [[ ! -d "$PACKAGE_ADDON_DIR" ]]; then
  echo "ERROR: packaged addon directory not found: $PACKAGE_ADDON_DIR" >&2
  exit 1
fi

if [[ -z "${GODOT:-}" ]]; then
  echo "ERROR: GODOT is not set" >&2
  exit 1
fi

mkdir -p "$EXPORT_DIR"

export PIPER_ADDON_SRC="$PACKAGE_ADDON_DIR"
export PIPER_ADDON_BIN_SRC="$PACKAGE_ADDON_DIR/bin"

bash "$REPO_ROOT/test/prepare-assets.sh"
rm -rf "$PROJECT_PATH" "$XCODE_PROJECT_PATH"

"$GODOT" --headless --path "$PROJECT_DIR" --export-debug "iOS" "$PROJECT_PATH"

if [[ ! -d "$XCODE_PROJECT_PATH" ]]; then
  echo "ERROR: iOS export did not produce ${XCODE_PROJECT_PATH}" >&2
  exit 1
fi

xcodebuild \
  -project "$XCODE_PROJECT_PATH" \
  -scheme "$EXPORT_NAME" \
  -configuration Debug \
  -destination "generic/platform=iOS" \
  CODE_SIGNING_ALLOWED=NO \
  CODE_SIGNING_REQUIRED=NO \
  CODE_SIGN_IDENTITY="" \
  build
