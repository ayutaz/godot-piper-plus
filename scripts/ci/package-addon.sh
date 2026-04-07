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

remove_forbidden_payloads() {
  rm -rf "$PACKAGE_ADDON_DIR/models"
  find "$PACKAGE_ADDON_DIR" -mindepth 1 \
    \( -path '*/open_jtalk_dic*' -o -name 'open_jtalk_dic*' \) \
    -exec rm -rf {} +
  find "$PACKAGE_ADDON_DIR" -mindepth 1 \
    \( -path '*/naist-jdic*' -o -name 'naist-jdic*' \) \
    -exec rm -rf {} +
  find "$PACKAGE_ADDON_DIR" -type f \
    \( -iname 'openjtalk_native*' -o -iname 'openjtalk-native*' \) \
    -exec rm -f {} +
}

collect_manifest_bin_files() {
  local gdextension_file="$1"

  grep -o 'res://addons/piper_plus/bin/[^"]*' "$gdextension_file" \
    | sed 's#res://addons/piper_plus/bin/##' \
    | sort -u
}

copy_runtime_sidecars() {
  local artifact_bin_dir="$1"
  local pattern=""
  local sidecar_path=""
  local -a extra_patterns=(
    "onnxruntime*.dll"
    "libonnxruntime*.so"
    "libonnxruntime*.so.*"
    "libonnxruntime*.dylib"
    "DirectML.dll"
  )

  for pattern in "${extra_patterns[@]}"; do
    for sidecar_path in "$artifact_bin_dir"/$pattern; do
      if [[ -f "$sidecar_path" ]]; then
        cp -f "$sidecar_path" "$PACKAGE_BIN_DIR"/
      fi
    done
  done
}

prune_manifest_for_available_binaries() {
  local input_manifest="$1"
  local output_manifest="$2"
  local bin_dir="$3"
  awk -v bin_dir="$bin_dir" '
function update_balance(text,    open_text, close_text, opens, closes) {
  open_text = text
  close_text = text
  opens = gsub(/\{/, "{", open_text)
  closes = gsub(/\}/, "}", close_text)
  return opens - closes
}

function entry_key(text,    parts, key) {
  split(text, parts, "=")
  key = parts[1]
  gsub(/[[:space:]]/, "", key)
  return key
}

function refs_exist(text,    remaining, ref, rel, full_path, quoted_path, cmd) {
  remaining = text
  while (match(remaining, /res:\/\/addons\/piper_plus\/bin\/[^" ,}]+/)) {
    ref = substr(remaining, RSTART, RLENGTH)
    rel = ref
    sub(/^res:\/\/addons\/piper_plus\/bin\//, "", rel)
    full_path = bin_dir "/" rel
    quoted_path = full_path
    gsub(/["\\]/, "\\\\&", quoted_path)
    cmd = "test -e \"" quoted_path "\""
    if (system(cmd) != 0) {
      return 0
    }
    remaining = substr(remaining, RSTART + RLENGTH)
  }

  return 1
}

function flush_dependency_block() {
  if (!dependency_block_active) {
    return
  }

  if (dependency_block_keep) {
    printf "%s", dependency_block
  }

  dependency_block = ""
  dependency_block_active = 0
  dependency_block_keep = 1
  dependency_block_balance = 0
}

BEGIN {
  current_section = ""
  dependency_block = ""
  dependency_block_active = 0
  dependency_block_keep = 1
  dependency_block_balance = 0
}

{
  sub(/\r$/, "", $0)

  if (dependency_block_active) {
    dependency_block = dependency_block $0 ORS
    if (!refs_exist($0)) {
      dependency_block_keep = 0
    }
    dependency_block_balance += update_balance($0)
    if (dependency_block_balance <= 0) {
      flush_dependency_block()
    }
    next
  }

  if ($0 ~ /^\[/) {
    flush_dependency_block()
    current_section = $0
    print $0
    next
  }

  if (current_section == "[libraries]" && $0 ~ /^[A-Za-z0-9_.]+\s*=\s*"/) {
    if (refs_exist($0)) {
      kept_library_keys[entry_key($0)] = 1
      print $0
    }
    next
  }

  if (current_section == "[dependencies]" && $0 ~ /^[A-Za-z0-9_.]+\s*=/) {
    dependency_block_key = entry_key($0)
    dependency_block_active = 1
    dependency_block = $0 ORS
    dependency_block_keep = refs_exist($0) && (dependency_block_key in kept_library_keys)
    dependency_block_balance = update_balance($0)
    if (dependency_block_balance <= 0) {
      flush_dependency_block()
    }
    next
  }

  print $0
}

END {
  flush_dependency_block()
}
' "$input_manifest" > "$output_manifest"
}

rm -rf "$PACKAGE_ROOT"
mkdir -p "$PACKAGE_BIN_DIR"

# Copy the full addon payload first, then replace bin contents with built artifacts.
cp -R "$ADDON_SRC"/. "$PACKAGE_ADDON_DIR"/
mkdir -p "$PACKAGE_BIN_DIR"
find "$PACKAGE_BIN_DIR" -mindepth 1 ! -name '.gitignore' -exec rm -rf {} +
remove_forbidden_payloads

shopt -s nullglob
artifact_dirs=("$ARTIFACTS_DIR"/*)
if [[ ${#artifact_dirs[@]} -eq 0 ]]; then
  echo "ERROR: no artifact directories found under $ARTIFACTS_DIR" >&2
  exit 1
fi

required_binaries=()
while IFS= read -r binary_name; do
  [[ -n "$binary_name" ]] && required_binaries+=("$binary_name")
done < <(collect_manifest_bin_files "$GDEXTENSION_FILE")

if [[ ${#required_binaries[@]} -eq 0 ]]; then
  echo "ERROR: no addon binaries were parsed from $GDEXTENSION_FILE" >&2
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

  copy_runtime_sidecars "$artifact_bin_dir"
done

remove_forbidden_payloads

if [[ $copied_any -eq 0 ]]; then
  echo "ERROR: no addon binaries were copied into $PACKAGE_BIN_DIR" >&2
  exit 1
fi

tmp_manifest="$(mktemp)"
prune_manifest_for_available_binaries "$GDEXTENSION_FILE" "$tmp_manifest" "$PACKAGE_BIN_DIR"
mv "$tmp_manifest" "$GDEXTENSION_FILE"

echo "Assembled addon package at: $PACKAGE_ADDON_DIR"
find "$PACKAGE_ADDON_DIR" -maxdepth 2 -type f | sort
