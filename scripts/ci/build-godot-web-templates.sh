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

GODOT_SOURCE_DIR="${GODOT_SOURCE_DIR:-}"
GODOT_TEMPLATES_VERSION="${GODOT_TEMPLATES_VERSION:-4.4.1.stable}"
SCONS_BIN="${SCONS_BIN:-scons}"
GODOT_WEB_TEMPLATE_VARIANTS="${GODOT_WEB_TEMPLATE_VARIANTS:-threads,nothreads}"
GODOT_WEB_TEMPLATE_OUTPUT_DIR="${1:-${GODOT_WEB_TEMPLATE_OUTPUT_DIR:-$REPO_ROOT/.ci/godot-export-templates/custom}}"
INSTALL_TO_EXPORT_TEMPLATES="${INSTALL_TO_EXPORT_TEMPLATES:-0}"
GODOT_WEB_SCONS_JOBS="${GODOT_WEB_SCONS_JOBS:-$(detect_parallel_level)}"

if [[ -z "${GODOT_SOURCE_DIR}" ]]; then
  echo "ERROR: GODOT_SOURCE_DIR is not set" >&2
  exit 1
fi

if [[ ! -f "${GODOT_SOURCE_DIR}/SConstruct" ]]; then
  echo "ERROR: GODOT_SOURCE_DIR does not look like a Godot source checkout: ${GODOT_SOURCE_DIR}" >&2
  exit 1
fi

if ! command -v emcc >/dev/null 2>&1; then
  echo "ERROR: emcc was not found. Activate emsdk before building Web templates." >&2
  exit 1
fi

mkdir -p "${GODOT_WEB_TEMPLATE_OUTPUT_DIR}"
printf '%s\n' "${GODOT_TEMPLATES_VERSION}" > "${GODOT_WEB_TEMPLATE_OUTPUT_DIR}/version.txt"

archive_name_for() {
  local target="$1"
  local variant="$2"
  local flavor=""

  if [[ "${target}" == "template_debug" ]]; then
    flavor="debug"
  else
    flavor="release"
  fi

  if [[ "${variant}" == "threads" ]]; then
    printf 'web_dlink_%s.zip\n' "${flavor}"
  else
    printf 'web_dlink_nothreads_%s.zip\n' "${flavor}"
  fi
}

find_built_archive() {
  local target="$1"
  local variant="$2"
  local candidate=""
  local selected=""

  while IFS= read -r candidate; do
    [[ "${candidate}" == *".dlink."* ]] || continue
    if [[ "${variant}" == "threads" && "${candidate}" == *".nothreads."* ]]; then
      continue
    fi
    if [[ "${variant}" == "nothreads" && "${candidate}" != *".nothreads."* ]]; then
      continue
    fi
    selected="${candidate}"
  done < <(find "${GODOT_SOURCE_DIR}/bin" -maxdepth 1 -type f -name "godot.web.${target}.wasm32*.zip" | sort)

  if [[ -z "${selected}" ]]; then
    echo "ERROR: built archive not found for ${target} (${variant})" >&2
    exit 1
  fi

  printf '%s\n' "${selected}"
}

build_variant() {
  local target="$1"
  local variant="$2"
  local threads_flag="yes"
  local built_archive=""
  local output_name=""

  if [[ "${variant}" == "nothreads" ]]; then
    threads_flag="no"
  fi

  "${SCONS_BIN}" -j "${GODOT_WEB_SCONS_JOBS}" -C "${GODOT_SOURCE_DIR}" \
    platform=web \
    target="${target}" \
    dlink_enabled=yes \
    "threads=${threads_flag}"

  built_archive="$(find_built_archive "${target}" "${variant}")"
  output_name="$(archive_name_for "${target}" "${variant}")"
  cp -f "${built_archive}" "${GODOT_WEB_TEMPLATE_OUTPUT_DIR}/${output_name}"
}

IFS=',' read -r -a variants <<< "${GODOT_WEB_TEMPLATE_VARIANTS}"
for variant in "${variants[@]}"; do
  case "${variant}" in
    threads|nothreads)
      build_variant "template_debug" "${variant}"
      build_variant "template_release" "${variant}"
      ;;
    *)
      echo "ERROR: unsupported variant '${variant}'. Use threads or nothreads." >&2
      exit 1
      ;;
  esac
done

node "${SCRIPT_DIR}/patch-web-asm-const.mjs" "${GODOT_WEB_TEMPLATE_OUTPUT_DIR}"

if [[ "${INSTALL_TO_EXPORT_TEMPLATES}" == "1" ]]; then
  GODOT_SKIP_OFFICIAL_TEMPLATES=1 \
  GODOT_WEB_TEMPLATES_DIR="${GODOT_WEB_TEMPLATE_OUTPUT_DIR}" \
  bash "${SCRIPT_DIR}/install-godot-export-templates.sh"
fi
