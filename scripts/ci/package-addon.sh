#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

ARTIFACTS_DIR="${1:-$REPO_ROOT/artifacts}"
PACKAGE_ROOT="${2:-$REPO_ROOT/package}"
ADDON_SRC="${3:-$REPO_ROOT/addons/piper_plus}"
PACKAGE_ADDON_DIR="$PACKAGE_ROOT/addons/piper_plus"
PACKAGE_BIN_DIR="$PACKAGE_ADDON_DIR/bin"
GDEXTENSION_FILE="$PACKAGE_ADDON_DIR/piper_plus.gdextension"

if [[ ! -d "$ADDON_SRC" ]]; then
  echo "ERROR: addon source directory not found: $ADDON_SRC" >&2
  exit 1
fi

rm -rf "$PACKAGE_ROOT"
mkdir -p "$PACKAGE_BIN_DIR"

# Copy the full addon payload first, then replace bin contents with built artifacts.
cp -R "$ADDON_SRC"/. "$PACKAGE_ADDON_DIR"/
mkdir -p "$PACKAGE_BIN_DIR"
find "$PACKAGE_BIN_DIR" -mindepth 1 ! -name '.gitignore' -exec rm -rf {} +

shopt -s nullglob
artifact_dirs=("$ARTIFACTS_DIR"/*)
if [[ ${#artifact_dirs[@]} -eq 0 ]]; then
  echo "ERROR: no artifact directories found under $ARTIFACTS_DIR" >&2
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
  echo "ERROR: no release binaries were parsed from $GDEXTENSION_FILE" >&2
  exit 1
fi

copied_any=0
for artifact_dir in "${artifact_dirs[@]}"; do
  artifact_bin_dir=""
  if [[ -d "$artifact_dir/bin" ]]; then
    artifact_bin_dir="$artifact_dir/bin"
  elif [[ -d "$artifact_dir/addons/piper_plus/bin" ]]; then
    artifact_bin_dir="$artifact_dir/addons/piper_plus/bin"
  else
    artifact_bin_dir="$artifact_dir"
  fi

  for binary_name in "${required_binaries[@]}"; do
    if [[ -f "$artifact_bin_dir/$binary_name" ]]; then
      cp -f "$artifact_bin_dir/$binary_name" "$PACKAGE_BIN_DIR"/
      copied_any=1
    fi
  done
done

if [[ $copied_any -eq 0 ]]; then
  echo "ERROR: no addon binaries were copied into $PACKAGE_BIN_DIR" >&2
  exit 1
fi

echo "Assembled addon package at: $PACKAGE_ADDON_DIR"
find "$PACKAGE_ADDON_DIR" -maxdepth 2 -type f | sort
