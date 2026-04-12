#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
CATALOG_PATH="${PIPER_DOWNLOAD_CATALOG_PATH:-$REPO_ROOT/addons/piper_plus/download_catalog.json}"
DICTIONARY_KEY="${PIPER_OPENJTALK_DICTIONARY_KEY:-naist-jdic}"
DEFAULT_DICTIONARY_SOURCE_DIR="$REPO_ROOT/addons/piper_plus/dictionaries/open_jtalk_dic_utf_8-1.11"
DICTIONARY_SOURCE_DIR="${PIPER_OPENJTALK_DICTIONARY_SOURCE_DIR:-${PIPER_TEST_DICT_PATH:-}}"
CACHE_ROOT="${PIPER_OPENJTALK_DICTIONARY_CACHE:-$REPO_ROOT/.cache/openjtalk-dictionary}"
ARCHIVE_CACHE_DIR="$CACHE_ROOT/archives"
EXTRACT_CACHE_DIR="$CACHE_ROOT/extracted"

if [[ -z "$DICTIONARY_SOURCE_DIR" && -d "$DEFAULT_DICTIONARY_SOURCE_DIR" ]]; then
  DICTIONARY_SOURCE_DIR="$DEFAULT_DICTIONARY_SOURCE_DIR"
fi

if [[ $# -lt 1 ]]; then
  echo "usage: stage-openjtalk-dictionary.sh <dest-dir> [<dest-dir> ...]" >&2
  exit 1
fi

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

if [[ -n "${CURL_BIN:-}" ]]; then
  CURL_CMD="$CURL_BIN"
elif command -v curl >/dev/null 2>&1; then
  CURL_CMD="curl"
else
  echo "ERROR: curl is required to download the OpenJTalk dictionary archive." >&2
  exit 1
fi

verify_dictionary_dir() {
  local dir_path="$1"

  [[ -d "$dir_path" ]] || return 1

  for required_file in sys.dic unk.dic matrix.bin char.bin; do
    [[ -f "$dir_path/$required_file" ]] || return 1
  done

  return 0
}

resolve_extracted_dictionary_dir() {
  local extracted_root="$1"
  local nested_root="$extracted_root/$install_directory"

  if verify_dictionary_dir "$extracted_root"; then
    printf '%s\n' "$extracted_root"
    return 0
  fi

  if verify_dictionary_dir "$nested_root"; then
    printf '%s\n' "$nested_root"
    return 0
  fi

  return 1
}

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

read_catalog_field() {
  local field_name="$1"
  "$PYTHON_CMD" - "$CATALOG_PATH" "$DICTIONARY_KEY" "$field_name" <<'PY'
import json
import sys

catalog_path, item_key, field_name = sys.argv[1], sys.argv[2], sys.argv[3]
with open(catalog_path, "r", encoding="utf-8") as handle:
    catalog = json.load(handle)

item = catalog["items"].get(item_key)
if not item:
    raise SystemExit(f"dictionary key not found in catalog: {item_key}")

file_entry = next((entry for entry in item.get("files", []) if entry.get("extract")), None)
if file_entry is None:
    raise SystemExit(f"extractable dictionary file is missing for key: {item_key}")

if field_name == "install_directory":
    print(item.get("install_directory", ""))
else:
    print(file_entry.get(field_name, ""))
PY
}

extract_archive() {
  local archive_path="$1"
  local destination_root="$2"

  rm -rf "$destination_root"
  mkdir -p "$destination_root"

  "$PYTHON_CMD" - "$archive_path" "$destination_root" <<'PY'
import pathlib
import sys
import zipfile

archive_path = pathlib.Path(sys.argv[1]).resolve()
destination_root = pathlib.Path(sys.argv[2]).resolve()

with zipfile.ZipFile(archive_path, "r") as archive:
    for member in archive.infolist():
        member_path = destination_root / member.filename
        resolved_path = member_path.resolve()
        if destination_root not in resolved_path.parents and resolved_path != destination_root:
            raise SystemExit(f"zip entry escapes destination: {member.filename}")
    archive.extractall(destination_root)
PY
}

install_directory="$(read_catalog_field install_directory)"
archive_filename="$(read_catalog_field filename)"
archive_url="$(read_catalog_field url)"
archive_sha256="$(read_catalog_field sha256)"

if [[ -z "$install_directory" || -z "$archive_filename" || -z "$archive_url" ]]; then
  echo "ERROR: dictionary catalog entry is incomplete for $DICTIONARY_KEY" >&2
  exit 1
fi

mkdir -p "$ARCHIVE_CACHE_DIR" "$EXTRACT_CACHE_DIR"

source_dir=""
if [[ -n "$DICTIONARY_SOURCE_DIR" ]]; then
  if ! verify_dictionary_dir "$DICTIONARY_SOURCE_DIR"; then
    echo "ERROR: provided dictionary source is not a compiled OpenJTalk dictionary: $DICTIONARY_SOURCE_DIR" >&2
    exit 1
  fi
  source_dir="$DICTIONARY_SOURCE_DIR"
else
  archive_path="$ARCHIVE_CACHE_DIR/$archive_filename"
  extracted_root="$EXTRACT_CACHE_DIR/$install_directory"
  resolved_extracted_dir=""

  if [[ -f "$archive_path" ]]; then
    verify_sha256 "$archive_path" "$archive_sha256"
  else
    "$CURL_CMD" -L --fail --retry 3 --retry-delay 2 -o "$archive_path.tmp" "$archive_url"
    verify_sha256 "$archive_path.tmp" "$archive_sha256"
    mv "$archive_path.tmp" "$archive_path"
  fi

  if ! resolved_extracted_dir="$(resolve_extracted_dictionary_dir "$extracted_root")"; then
    extract_archive "$archive_path" "$EXTRACT_CACHE_DIR"
  fi

  if ! resolved_extracted_dir="$(resolve_extracted_dictionary_dir "$extracted_root")"; then
    echo "ERROR: extracted dictionary is not ready: $extracted_root" >&2
    exit 1
  fi

  source_dir="$resolved_extracted_dir"
fi

for destination_dir in "$@"; do
  [[ -n "$destination_dir" ]] || continue
  rm -rf "$destination_dir"
  mkdir -p "$destination_dir"
  cp -R "$source_dir"/. "$destination_dir"/
done

printf 'OPENJTALK_DICTIONARY_READY %s\n' "$source_dir"
