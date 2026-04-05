#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
PROJECT_DIR="$REPO_ROOT/test/project"
PACKAGE_ADDON_DIR="${1:-$REPO_ROOT/package/addons/piper_plus}"
EXPORT_DIR="$PROJECT_DIR/build/android"
OUTPUT_APK="$EXPORT_DIR/piper-plus-tests.apk"
PROJECT_KEYSTORE_PATH="$PROJECT_DIR/android-debug.keystore"

if [[ ! -d "$PACKAGE_ADDON_DIR" ]]; then
  echo "ERROR: packaged addon directory not found: $PACKAGE_ADDON_DIR" >&2
  exit 1
fi

if [[ -z "${GODOT:-}" ]]; then
  echo "ERROR: GODOT is not set" >&2
  exit 1
fi

resolve_native_path() {
  local input_path="$1"
  if command -v cygpath >/dev/null 2>&1; then
    cygpath -m "$input_path"
  elif command -v wslpath >/dev/null 2>&1; then
    wslpath -m "$input_path"
  else
    printf '%s\n' "$input_path"
  fi
}

editor_settings_dir() {
  if [[ -n "${APPDATA:-}" ]]; then
    printf '%s/Godot\n' "$(resolve_native_path "$APPDATA")"
  elif [[ "$(uname -s)" == "Darwin" ]]; then
    printf '%s/Library/Application Support/Godot\n' "$HOME"
  else
    printf '%s/.config/godot\n' "$HOME"
  fi
}

editor_settings_paths() {
  local settings_dir="$1"
  local version_line=""
  local version_minor=""

  printf '%s/editor_settings-4.tres\n' "$settings_dir"

  version_line="$("$GODOT" --version 2>/dev/null | head -n 1 || true)"
  version_minor="$(printf '%s\n' "$version_line" | sed -n 's/^\([0-9]\+\.[0-9]\+\).*/\1/p')"
  if [[ -n "$version_minor" ]]; then
    printf '%s/editor_settings-%s.tres\n' "$settings_dir" "$version_minor"
  fi
}

pick_android_sdk_root() {
  local candidates=()
  if [[ -n "${ANDROID_SDK_ROOT:-}" ]]; then
    candidates+=("${ANDROID_SDK_ROOT}")
  fi
  if [[ -n "${ANDROID_HOME:-}" ]]; then
    candidates+=("${ANDROID_HOME}")
  fi
  candidates+=(
    "$HOME/Android/Sdk"
    "$HOME/AppData/Local/Android/Sdk"
    "/usr/local/lib/android/sdk"
    "/opt/android-sdk"
  )

  for candidate in "${candidates[@]}"; do
    if [[ -d "$candidate" ]]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done

  return 1
}

pick_debug_keystore_path() {
  local candidates=()
  if [[ -n "${GODOT_ANDROID_KEYSTORE_DEBUG_PATH:-}" ]]; then
    candidates+=("${GODOT_ANDROID_KEYSTORE_DEBUG_PATH}")
  fi
  if [[ -n "${USERPROFILE:-}" ]]; then
    candidates+=("${USERPROFILE}/.android/debug.keystore")
  fi
  if [[ -n "${APPDATA:-}" ]]; then
    candidates+=("$(dirname "$(dirname "${APPDATA}")")/.android/debug.keystore")
  fi
  candidates+=("$REPO_ROOT/.ci/android-debug.keystore")

  for candidate in "${candidates[@]}"; do
    if [[ -f "$candidate" ]]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done

  printf '%s\n' "$REPO_ROOT/.ci/android-debug.keystore"
}

pick_keytool_bin() {
  if [[ -n "${JAVA_HOME:-}" ]]; then
    if [[ -x "${JAVA_HOME}/bin/keytool" ]]; then
      printf '%s\n' "${JAVA_HOME}/bin/keytool"
      return 0
    fi
    if [[ -f "${JAVA_HOME}/bin/keytool.exe" ]]; then
      printf '%s\n' "${JAVA_HOME}/bin/keytool.exe"
      return 0
    fi
  fi

  if command -v keytool >/dev/null 2>&1; then
    command -v keytool
    return 0
  fi

  return 1
}

ensure_editor_setting() {
  local file_path="$1"
  local key="$2"
  local value="$3"

  mkdir -p "$(dirname "$file_path")"
  if [[ ! -s "$file_path" ]]; then
    cat > "$file_path" <<'EOF'
[gd_resource type="EditorSettings" format=3]

[resource]
EOF
  fi
  local tmp_file
  tmp_file="$(mktemp)"
  grep -v "^${key} = " "$file_path" > "$tmp_file" || true
  printf '%s = "%s"\n' "$key" "$value" >> "$tmp_file"
  mv "$tmp_file" "$file_path"
}

ensure_editor_setting_all() {
  local settings_dir="$1"
  local key="$2"
  local value="$3"
  local file_path=""

  while IFS= read -r file_path; do
    [[ -n "$file_path" ]] || continue
    ensure_editor_setting "$file_path" "$key" "$value"
  done < <(editor_settings_paths "$settings_dir")
}

ANDROID_SDK_ROOT="$(pick_android_sdk_root)"
export ANDROID_SDK_ROOT
export ANDROID_HOME="${ANDROID_HOME:-$ANDROID_SDK_ROOT}"

KEYSTORE_PATH="$(pick_debug_keystore_path)"
ANDROID_SDK_ROOT_NATIVE="$(resolve_native_path "$ANDROID_SDK_ROOT")"
JAVA_HOME_NATIVE="$(resolve_native_path "${JAVA_HOME:-}")"
EDITOR_SETTINGS_DIR="$(editor_settings_dir)"
KEYTOOL_BIN="$(pick_keytool_bin)"

mkdir -p "$(dirname "$KEYSTORE_PATH")" "$EXPORT_DIR"
if [[ ! -f "$KEYSTORE_PATH" ]]; then
  "$KEYTOOL_BIN" -genkeypair \
    -alias androiddebugkey \
    -keyalg RSA \
    -keysize 2048 \
    -validity 10000 \
    -dname "CN=Android Debug,O=Android,C=US" \
    -keystore "$KEYSTORE_PATH" \
    -storepass android \
    -keypass android >/dev/null 2>&1
fi

ensure_editor_setting_all "$EDITOR_SETTINGS_DIR" "export/android/android_sdk_path" "$ANDROID_SDK_ROOT_NATIVE"
cp -f "$KEYSTORE_PATH" "$PROJECT_KEYSTORE_PATH"

ensure_editor_setting_all "$EDITOR_SETTINGS_DIR" "export/android/debug_keystore" "$(resolve_native_path "$PROJECT_KEYSTORE_PATH")"
ensure_editor_setting_all "$EDITOR_SETTINGS_DIR" "export/android/debug_keystore_user" "androiddebugkey"
ensure_editor_setting_all "$EDITOR_SETTINGS_DIR" "export/android/debug_keystore_pass" "android"
if [[ -n "${JAVA_HOME_NATIVE}" ]]; then
  ensure_editor_setting_all "$EDITOR_SETTINGS_DIR" "export/android/java_sdk_path" "$JAVA_HOME_NATIVE"
fi

export GODOT_ANDROID_KEYSTORE_DEBUG_PATH="$(resolve_native_path "$PROJECT_KEYSTORE_PATH")"
export GODOT_ANDROID_KEYSTORE_DEBUG_USER="${GODOT_ANDROID_KEYSTORE_DEBUG_USER:-androiddebugkey}"
export GODOT_ANDROID_KEYSTORE_DEBUG_PASSWORD="${GODOT_ANDROID_KEYSTORE_DEBUG_PASSWORD:-android}"
export GODOT_ANDROID_KEYSTORE_RELEASE_PATH="${GODOT_ANDROID_KEYSTORE_RELEASE_PATH:-$(resolve_native_path "$PROJECT_KEYSTORE_PATH")}"
export GODOT_ANDROID_KEYSTORE_RELEASE_USER="${GODOT_ANDROID_KEYSTORE_RELEASE_USER:-androiddebugkey}"
export GODOT_ANDROID_KEYSTORE_RELEASE_PASSWORD="${GODOT_ANDROID_KEYSTORE_RELEASE_PASSWORD:-android}"
export PIPER_ADDON_SRC="$PACKAGE_ADDON_DIR"
export PIPER_ADDON_BIN_SRC="$PACKAGE_ADDON_DIR/bin"

if [[ ! -f "$PACKAGE_ADDON_DIR/bin/libonnxruntime.android.arm64.so" ]]; then
  if [[ -f "$PACKAGE_ADDON_DIR/bin/libonnxruntime.so" ]]; then
    cp -f "$PACKAGE_ADDON_DIR/bin/libonnxruntime.so" "$PACKAGE_ADDON_DIR/bin/libonnxruntime.android.arm64.so"
  else
    ort_candidate="$(find "$REPO_ROOT" -path '*/onnxruntime/*/lib/libonnxruntime.so' | head -n 1)"
    if [[ -n "$ort_candidate" && -f "$ort_candidate" ]]; then
      cp -f "$ort_candidate" "$PACKAGE_ADDON_DIR/bin/libonnxruntime.android.arm64.so"
    fi
  fi
fi

bash "$REPO_ROOT/test/prepare-assets.sh"
rm -f "$OUTPUT_APK"

"$GODOT" --headless --path "$(resolve_native_path "$PROJECT_DIR")" --export-debug "Android" "$(resolve_native_path "$OUTPUT_APK")"

if [[ ! -f "$OUTPUT_APK" ]]; then
  echo "ERROR: Android export did not produce ${OUTPUT_APK}" >&2
  exit 1
fi

APK_LISTING="$(unzip -l "$OUTPUT_APK")"
echo "$APK_LISTING"

echo "$APK_LISTING" | grep -F "libpiper_plus" >/dev/null || {
  echo "ERROR: exported APK does not contain piper_plus native library" >&2
  exit 1
}

echo "$APK_LISTING" | grep -F "onnxruntime" >/dev/null || {
  echo "ERROR: exported APK does not contain ONNX Runtime native library" >&2
  exit 1
}
