#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

GODOT_VERSION="${GODOT_VERSION:-4.4.1-stable}"
GODOT_TEMPLATES_VERSION="${GODOT_TEMPLATES_VERSION:-4.4.1.stable}"
TEMPLATES_ARCHIVE="${GODOT_EXPORT_TEMPLATES_ARCHIVE:-Godot_v${GODOT_VERSION}_export_templates.tpz}"
RELEASE_BASE_URL="${GODOT_RELEASE_BASE_URL:-https://github.com/godotengine/godot-builds/releases/download/${GODOT_VERSION}}"
GODOT_SKIP_OFFICIAL_TEMPLATES="${GODOT_SKIP_OFFICIAL_TEMPLATES:-0}"
GODOT_WEB_TEMPLATES_DIR="${GODOT_WEB_TEMPLATES_DIR:-}"
CUSTOM_WEB_TEMPLATE_INSTALL_DIR="${GODOT_CUSTOM_WEB_TEMPLATE_INSTALL_DIR:-$REPO_ROOT/.ci/godot-web-templates/${GODOT_TEMPLATES_VERSION}}"

case "$(uname -s)" in
  Darwin)
    TEMPLATES_DIR="${HOME}/Library/Application Support/Godot/export_templates/${GODOT_TEMPLATES_VERSION}"
    ;;
  *)
    TEMPLATES_DIR="${HOME}/.local/share/godot/export_templates/${GODOT_TEMPLATES_VERSION}"
    ;;
esac

if [[ -z "${GODOT_WEB_TEMPLATES_DIR}" ]]; then
  DEFAULT_WEB_TEMPLATES_DIR="${REPO_ROOT}/.ci/godot-web-templates/${GODOT_TEMPLATES_VERSION}"
  if [[ -d "${DEFAULT_WEB_TEMPLATES_DIR}" ]]; then
    GODOT_WEB_TEMPLATES_DIR="${DEFAULT_WEB_TEMPLATES_DIR}"
  fi
fi

copy_tree_contents() {
  local src_dir="$1"
  local dst_dir="$2"

  mkdir -p "${dst_dir}"
  cp -R "${src_dir}"/. "${dst_dir}/"
}

install_custom_web_templates() {
  local src_dir="$1"
  local install_dir="${CUSTOM_WEB_TEMPLATE_INSTALL_DIR}"
  local -a required_archives=(
    "web_dlink_debug.zip"
    "web_dlink_release.zip"
    "web_dlink_nothreads_debug.zip"
    "web_dlink_nothreads_release.zip"
  )
  local archive_name=""

  if [[ ! -d "${src_dir}" ]]; then
    echo "ERROR: custom Web template directory not found: ${src_dir}" >&2
    exit 1
  fi

  mkdir -p "${TEMPLATES_DIR}"
  mkdir -p "${install_dir}"
  for archive_name in "${required_archives[@]}"; do
    if [[ ! -f "${src_dir}/${archive_name}" ]]; then
      echo "ERROR: missing custom Web template archive: ${src_dir}/${archive_name}" >&2
      exit 1
    fi
    cp -f "${src_dir}/${archive_name}" "${TEMPLATES_DIR}/${archive_name}"
    if [[ "${src_dir}/${archive_name}" != "${install_dir}/${archive_name}" ]]; then
      cp -f "${src_dir}/${archive_name}" "${install_dir}/${archive_name}"
    fi
  done
}

TMP_DIR="${REPO_ROOT}/.ci/godot-export-templates"
mkdir -p "${TMP_DIR}" "${TEMPLATES_DIR}"

if [[ ! -f "${TEMPLATES_DIR}/version.txt" && "${GODOT_SKIP_OFFICIAL_TEMPLATES}" != "1" ]]; then
  ARCHIVE_PATH="${TMP_DIR}/${TEMPLATES_ARCHIVE}"
  SUMS_PATH="${TMP_DIR}/SHA512-SUMS.txt"
  EXTRACT_DIR="${TMP_DIR}/extract"

  curl -L -o "${ARCHIVE_PATH}" "${RELEASE_BASE_URL}/${TEMPLATES_ARCHIVE}"
  curl -L -o "${SUMS_PATH}" "${RELEASE_BASE_URL}/SHA512-SUMS.txt"
  grep " ${TEMPLATES_ARCHIVE}\$" "${SUMS_PATH}" > "${TMP_DIR}/godot-export-templates.sha512"

  (
    cd "${TMP_DIR}"
    if [[ "$(uname -s)" == "Darwin" ]]; then
      shasum -a 512 -c "godot-export-templates.sha512"
    else
      sha512sum -c "godot-export-templates.sha512"
    fi
  )

  rm -rf "${EXTRACT_DIR}"
  mkdir -p "${EXTRACT_DIR}"
  unzip -qo "${ARCHIVE_PATH}" -d "${EXTRACT_DIR}"

  if [[ -d "${EXTRACT_DIR}/templates" ]]; then
    copy_tree_contents "${EXTRACT_DIR}/templates" "${TEMPLATES_DIR}"
    if [[ -f "${EXTRACT_DIR}/version.txt" ]]; then
      cp -f "${EXTRACT_DIR}/version.txt" "${TEMPLATES_DIR}/version.txt"
    fi
  else
    copy_tree_contents "${EXTRACT_DIR}" "${TEMPLATES_DIR}"
  fi
elif [[ ! -f "${TEMPLATES_DIR}/version.txt" ]]; then
  printf '%s\n' "${GODOT_TEMPLATES_VERSION}" > "${TEMPLATES_DIR}/version.txt"
fi

if [[ -n "${GODOT_WEB_TEMPLATES_DIR}" ]]; then
  install_custom_web_templates "${GODOT_WEB_TEMPLATES_DIR}"
fi
