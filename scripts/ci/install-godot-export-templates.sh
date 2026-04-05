#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

GODOT_VERSION="${GODOT_VERSION:-4.4.1-stable}"
GODOT_TEMPLATES_VERSION="${GODOT_TEMPLATES_VERSION:-4.4.1.stable}"
TEMPLATES_ARCHIVE="${GODOT_EXPORT_TEMPLATES_ARCHIVE:-Godot_v${GODOT_VERSION}_export_templates.tpz}"
RELEASE_BASE_URL="${GODOT_RELEASE_BASE_URL:-https://github.com/godotengine/godot-builds/releases/download/${GODOT_VERSION}}"

case "$(uname -s)" in
  Darwin)
    TEMPLATES_DIR="${HOME}/Library/Application Support/Godot/export_templates/${GODOT_TEMPLATES_VERSION}"
    ;;
  *)
    TEMPLATES_DIR="${HOME}/.local/share/godot/export_templates/${GODOT_TEMPLATES_VERSION}"
    ;;
esac

if [[ -f "${TEMPLATES_DIR}/version.txt" ]]; then
  exit 0
fi

TMP_DIR="${REPO_ROOT}/.ci/godot-export-templates"
mkdir -p "${TMP_DIR}" "${TEMPLATES_DIR}"

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
  cp -R "${EXTRACT_DIR}/templates"/. "${TEMPLATES_DIR}/"
  if [[ -f "${EXTRACT_DIR}/version.txt" ]]; then
    cp -f "${EXTRACT_DIR}/version.txt" "${TEMPLATES_DIR}/version.txt"
  fi
else
  cp -R "${EXTRACT_DIR}"/. "${TEMPLATES_DIR}/"
fi
