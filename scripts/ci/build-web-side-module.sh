#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

detect_parallel_level() {
  if command -v nproc >/dev/null 2>&1; then
    nproc
    return
  fi

  if command -v getconf >/dev/null 2>&1; then
    getconf _NPROCESSORS_ONLN
    return
  fi

  if command -v sysctl >/dev/null 2>&1; then
    sysctl -n hw.ncpu
    return
  fi

  printf '2\n'
}

BUILD_ROOT="${1:-$REPO_ROOT/build-web}"
STAGING_ROOT="${PIPER_PLUS_WEB_STAGING_ROOT:-$REPO_ROOT/artifacts}"
PARALLEL_LEVEL="${PIPER_PLUS_WEB_PARALLEL_LEVEL:-$(detect_parallel_level)}"
THREAD_MATRIX="${PIPER_PLUS_WEB_THREAD_MATRIX:-threads,nothreads}"
CONFIG_MATRIX="${PIPER_PLUS_WEB_CONFIG_MATRIX:-Debug,Release}"
ONNXRUNTIME_DIR="${ONNXRUNTIME_DIR:-${PIPER_PLUS_WEB_ONNXRUNTIME_DIR:-}}"
ONNXRUNTIME_DIR_THREADS="${ONNXRUNTIME_DIR_THREADS:-${PIPER_PLUS_WEB_ONNXRUNTIME_DIR_THREADS:-}}"
ONNXRUNTIME_DIR_NOTHREADS="${ONNXRUNTIME_DIR_NOTHREADS:-${PIPER_PLUS_WEB_ONNXRUNTIME_DIR_NOTHREADS:-}}"
ONNXRUNTIME_WEB_STATIC_LIB="${ONNXRUNTIME_WEB_STATIC_LIB:-}"
ONNXRUNTIME_WEB_STATIC_LIB_THREADS="${ONNXRUNTIME_WEB_STATIC_LIB_THREADS:-}"
ONNXRUNTIME_WEB_STATIC_LIB_NOTHREADS="${ONNXRUNTIME_WEB_STATIC_LIB_NOTHREADS:-}"

if ! command -v emcmake >/dev/null 2>&1; then
  echo "ERROR: emcmake is not available. Activate emsdk before running this script." >&2
  exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
  echo "ERROR: cmake is not available." >&2
  exit 1
fi

trim_csv_item() {
  local value="$1"
  value="${value#"${value%%[![:space:]]*}"}"
  value="${value%"${value##*[![:space:]]}"}"
  printf '%s\n' "$value"
}

expected_output_name() {
  local build_type="$1"
  local thread_mode="$2"
  local target_suffix="template_release"
  local thread_suffix=""

  if [[ "$build_type" == "Debug" ]]; then
    target_suffix="template_debug"
  fi

  if [[ "$thread_mode" == "nothreads" ]]; then
    thread_suffix=".nothreads"
  fi

  printf 'libpiper_plus.web.%s.wasm32%s.wasm\n' "$target_suffix" "$thread_suffix"
}

resolve_onnxruntime_dir() {
  local thread_mode="$1"

  case "$thread_mode" in
    threads)
      if [[ -n "$ONNXRUNTIME_DIR_THREADS" ]]; then
        printf '%s\n' "$ONNXRUNTIME_DIR_THREADS"
        return
      fi
      ;;
    nothreads)
      if [[ -n "$ONNXRUNTIME_DIR_NOTHREADS" ]]; then
        printf '%s\n' "$ONNXRUNTIME_DIR_NOTHREADS"
        return
      fi
      ;;
  esac

  if [[ -n "$ONNXRUNTIME_DIR" ]]; then
    printf '%s\n' "$ONNXRUNTIME_DIR"
    return
  fi

  echo "ERROR: ONNXRUNTIME_DIR is required for Web builds. Set ONNXRUNTIME_DIR or mode-specific ONNXRUNTIME_DIR_THREADS / ONNXRUNTIME_DIR_NOTHREADS." >&2
  exit 1
}

resolve_onnxruntime_static_lib() {
  local thread_mode="$1"
  local onnxruntime_dir="$2"

  case "$thread_mode" in
    threads)
      if [[ -n "$ONNXRUNTIME_WEB_STATIC_LIB_THREADS" ]]; then
        printf '%s\n' "$ONNXRUNTIME_WEB_STATIC_LIB_THREADS"
        return
      fi
      ;;
    nothreads)
      if [[ -n "$ONNXRUNTIME_WEB_STATIC_LIB_NOTHREADS" ]]; then
        printf '%s\n' "$ONNXRUNTIME_WEB_STATIC_LIB_NOTHREADS"
        return
      fi
      ;;
  esac

  if [[ -n "$ONNXRUNTIME_WEB_STATIC_LIB" ]]; then
    printf '%s\n' "$ONNXRUNTIME_WEB_STATIC_LIB"
    return
  fi

  printf '%s\n' "$onnxruntime_dir/lib/libonnxruntime_webassembly.a"
}

build_variant() {
  local build_type="$1"
  local thread_mode="$2"
  local godot_target="template_release"
  local godot_threads="ON"
  local onnxruntime_dir=""
  local onnxruntime_static_lib=""
  local build_dir="$BUILD_ROOT/${build_type,,}-${thread_mode}"
  local staging_dir="$STAGING_ROOT/piper-plus-bin-web-wasm32-${thread_mode}-${build_type,,}/bin"
  local output_name=""

  if [[ "$build_type" == "Debug" ]]; then
    godot_target="template_debug"
  fi

  if [[ "$thread_mode" == "nothreads" ]]; then
    godot_threads="OFF"
  fi

  output_name="$(expected_output_name "$build_type" "$thread_mode")"
  onnxruntime_dir="$(resolve_onnxruntime_dir "$thread_mode")"
  onnxruntime_static_lib="$(resolve_onnxruntime_static_lib "$thread_mode" "$onnxruntime_dir")"

  if [[ ! -f "$onnxruntime_static_lib" ]]; then
    echo "ERROR: ONNXRUNTIME_WEB_STATIC_LIB was not found for thread mode '$thread_mode': $onnxruntime_static_lib" >&2
    exit 1
  fi

  mkdir -p "$build_dir" "$staging_dir"

  emcmake cmake -S "$REPO_ROOT" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE="$build_type" \
    -DGODOTCPP_TARGET="$godot_target" \
    -DGODOTCPP_THREADS="$godot_threads" \
    -DONNXRUNTIME_DIR="$onnxruntime_dir" \
    -DONNXRUNTIME_WEB_STATIC_LIB="$onnxruntime_static_lib"

  cmake --build "$build_dir" --config "$build_type" --parallel "$PARALLEL_LEVEL" --target piper_plus

  if [[ ! -f "$REPO_ROOT/addons/piper_plus/bin/$output_name" ]]; then
    echo "ERROR: expected Web side module was not produced: $REPO_ROOT/addons/piper_plus/bin/$output_name" >&2
    exit 1
  fi

  cp -f "$REPO_ROOT/addons/piper_plus/bin/$output_name" "$staging_dir/$output_name"
}

IFS=',' read -r -a thread_modes <<< "$THREAD_MATRIX"
IFS=',' read -r -a build_types <<< "$CONFIG_MATRIX"

for build_type in "${build_types[@]}"; do
  build_type="$(trim_csv_item "$build_type")"
  [[ -n "$build_type" ]] || continue

  for thread_mode in "${thread_modes[@]}"; do
    thread_mode="$(trim_csv_item "$thread_mode")"
    [[ -n "$thread_mode" ]] || continue

    case "$thread_mode" in
      threads|nothreads) ;;
      *)
        echo "ERROR: unsupported thread mode '$thread_mode'. Use 'threads' or 'nothreads'." >&2
        exit 1
        ;;
    esac

    case "$build_type" in
      Debug|Release) ;;
      *)
        echo "ERROR: unsupported build type '$build_type'. Use 'Debug' or 'Release'." >&2
        exit 1
        ;;
    esac

    build_variant "$build_type" "$thread_mode"
  done
done
