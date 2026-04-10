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
ORT_BUILD_FLAGS="${ORT_BUILD_FLAGS:---build_wasm_static_lib --enable_wasm_simd --skip_tests --disable_rtti --config Release --cmake_extra_defines onnxruntime_BUILD_UNIT_TESTS=OFF}"
ORT_BUILD_PARALLEL="${ORT_BUILD_PARALLEL:-$(detect_parallel_level)}"
ORT_EMSDK_VERSION="${ORT_EMSDK_VERSION:-}"
ORT_BUILD_TARGET="${ORT_BUILD_TARGET:-}"

mkdir -p "$STAGING_ROOT/lib" "$STAGING_ROOT/include"

if [[ -f "$STAGING_ROOT/lib/libonnxruntime_webassembly.a" ]] && \
   [[ -f "$STAGING_ROOT/include/onnxruntime_c_api.h" ]]; then
  echo "Using existing staged ONNX Runtime Web package at: $STAGING_ROOT"
  find "$STAGING_ROOT" -maxdepth 3 -type f | sort
  exit 0
fi

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

read -r -a ort_build_args <<< "$ORT_BUILD_FLAGS"
if [[ " ${ort_build_args[*]} " != *" --parallel "* ]]; then
  ort_build_args+=(--parallel "$ORT_BUILD_PARALLEL")
fi
if [[ -n "$ORT_EMSDK_VERSION" && " ${ort_build_args[*]} " != *" --emsdk_version "* ]]; then
  ort_build_args+=(--emsdk_version "$ORT_EMSDK_VERSION")
fi
if [[ -n "$ORT_BUILD_TARGET" && " ${ort_build_args[*]} " != *" --target "* ]]; then
  ort_build_args+=(--target "$ORT_BUILD_TARGET")
fi

ort_build_dir=""
for ((i = 0; i < ${#ort_build_args[@]}; ++i)); do
  if [[ "${ort_build_args[$i]}" == "--build_dir" && $((i + 1)) -lt ${#ort_build_args[@]} ]]; then
    ort_build_dir="${ort_build_args[$((i + 1))]}"
    break
  fi
done

if [[ -z "$ort_build_dir" ]]; then
  ort_build_dir="$ORT_SOURCE_DIR/build"
elif [[ "$ort_build_dir" != /* ]]; then
  ort_build_dir="$ORT_SOURCE_DIR/$ort_build_dir"
fi

(
  cd "$ORT_SOURCE_DIR"
  echo "Building ONNX Runtime Web static library with target=${ORT_BUILD_TARGET:-default} parallel=$ORT_BUILD_PARALLEL emsdk=${ORT_EMSDK_VERSION:-default}"
  ./build.sh "${ort_build_args[@]}"
)

ort_static_lib=""
if [[ -d "$ort_build_dir" ]]; then
  ort_static_lib="$(find "$ort_build_dir" -type f -name 'libonnxruntime_webassembly.a' | sort | tail -n 1)"
fi

if [[ -z "$ort_static_lib" ]]; then
  echo "ERROR: libonnxruntime_webassembly.a was not produced under $ort_build_dir" >&2
  exit 1
fi

cp -f "$ort_static_lib" "$STAGING_ROOT/lib/libonnxruntime_webassembly.a"
cp -R "$ORT_SOURCE_DIR/include"/. "$STAGING_ROOT/include"/

# Mirror the flat header layout used by the official native packages so the
# existing CMake lookup works for Web builds too.
ort_session_include_dir="$ORT_SOURCE_DIR/include/onnxruntime/core/session"
if [[ -d "$ort_session_include_dir" ]]; then
  find "$ort_session_include_dir" -maxdepth 1 -type f -name '*.h' -exec cp -f {} "$STAGING_ROOT/include/" \;
fi

echo "Staged ONNX Runtime Web package at: $STAGING_ROOT"
find "$STAGING_ROOT" -maxdepth 3 -type f | sort
