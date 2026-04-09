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

ORT_SOURCE_DIR="${1:-${ORT_SOURCE_DIR:-}}"
STAGING_ROOT="${2:-${PIPER_ONNXRUNTIME_WEB_STAGING_ROOT:-$REPO_ROOT/artifacts/onnxruntime-web}}"
ORT_BUILD_FLAGS="${ORT_BUILD_FLAGS:---build_wasm_static_lib --enable_wasm_simd --enable_wasm_threads --skip_tests --config Release}"
ORT_BUILD_PARALLEL="${ORT_BUILD_PARALLEL:-$(detect_parallel_level)}"

if [[ -z "$ORT_SOURCE_DIR" ]]; then
  echo "ERROR: ORT_SOURCE_DIR is required" >&2
  exit 1
fi

if [[ ! -f "$ORT_SOURCE_DIR/build.sh" ]]; then
  echo "ERROR: ORT_SOURCE_DIR does not look like an ONNX Runtime source checkout: $ORT_SOURCE_DIR" >&2
  exit 1
fi

if ! command -v emcmake >/dev/null 2>&1; then
  echo "ERROR: emcmake is not available. Activate emsdk before building ONNX Runtime Web." >&2
  exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
  echo "ERROR: cmake is not available." >&2
  exit 1
fi

mkdir -p "$STAGING_ROOT/lib" "$STAGING_ROOT/include"

(
  cd "$ORT_SOURCE_DIR"
  read -r -a ort_build_args <<< "$ORT_BUILD_FLAGS"
  if [[ " ${ort_build_args[*]} " != *" --parallel "* ]]; then
    ort_build_args+=(--parallel "$ORT_BUILD_PARALLEL")
  fi

  echo "Building ONNX Runtime Web static library with parallel=$ORT_BUILD_PARALLEL"
  ./build.sh "${ort_build_args[@]}"
)

ort_static_lib="$(find "$ORT_SOURCE_DIR/build" -type f -name 'libonnxruntime_webassembly.a' | sort | tail -n 1)"
if [[ -z "$ort_static_lib" ]]; then
  echo "ERROR: libonnxruntime_webassembly.a was not produced under $ORT_SOURCE_DIR/build" >&2
  exit 1
fi

cp -f "$ort_static_lib" "$STAGING_ROOT/lib/libonnxruntime_webassembly.a"
cp -R "$ORT_SOURCE_DIR/include"/. "$STAGING_ROOT/include"/

echo "Staged ONNX Runtime Web package at: $STAGING_ROOT"
find "$STAGING_ROOT" -maxdepth 3 -type f | sort
