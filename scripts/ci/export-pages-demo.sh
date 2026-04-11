#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
EXPORT_ROOT="${1:-${PIPER_PAGES_EXPORT_ROOT:-$REPO_ROOT/build/pages-site}}"
SITE_ROOT="$EXPORT_ROOT/site"
TEMPLATE_PROJECT_DIR="${PIPER_PAGES_TEMPLATE_PROJECT_DIR:-$REPO_ROOT/pages_demo}"
PROJECT_DIR="${PIPER_PAGES_PROJECT_DIR:-${PIPER_PAGES_DEMO_PROJECT_DIR:-$REPO_ROOT/build/pages-demo-project}}"
ENTRY_NAME="${PIPER_PAGES_ENTRY_NAME:-index.html}"
PRESET_NAME="${PIPER_PAGES_PRESET_NAME:-Web Pages}"
MODEL_KEY="${PIPER_PAGES_MODEL_KEY:-en_US-ljspeech-medium}"
MODEL_RELATIVE_DIR="piper_plus_assets/models/$MODEL_KEY"
MODEL_RELATIVE_PATH="$MODEL_RELATIVE_DIR/$MODEL_KEY.onnx"
CONFIG_RELATIVE_PATH="$MODEL_RELATIVE_DIR/$MODEL_KEY.onnx.json"
CMUDICT_RELATIVE_PATH="addons/piper_plus/dictionaries/cmudict_data.json"
ADDON_GDEXTENSION_RELATIVE_PATH="addons/piper_plus/piper_plus.gdextension"
ADDON_BIN_RELATIVE_DIR="addons/piper_plus/bin"
BUILD_SHA="${GITHUB_SHA:-$(git -C "$REPO_ROOT" rev-parse HEAD 2>/dev/null || printf 'local')}"

if [[ -z "${GODOT:-}" ]]; then
	echo "ERROR: GODOT is not set" >&2
	exit 1
fi

if ! command -v node >/dev/null 2>&1; then
	echo "ERROR: node is required to validate the Pages demo export." >&2
	exit 1
fi

if [[ ! -d "$TEMPLATE_PROJECT_DIR" ]]; then
	echo "ERROR: template project directory does not exist: $TEMPLATE_PROJECT_DIR" >&2
	exit 1
fi

rm -rf "$EXPORT_ROOT" "$PROJECT_DIR"
mkdir -p "$SITE_ROOT" "$(dirname "$PROJECT_DIR")"
cp -a "$TEMPLATE_PROJECT_DIR/." "$PROJECT_DIR/"

PIPER_PAGES_PROJECT_DIR="$PROJECT_DIR" bash "$SCRIPT_DIR/prepare-pages-demo-assets.sh"
node "$SCRIPT_DIR/validate-pages-preset.mjs" --project "$PROJECT_DIR" --preset "$PRESET_NAME"

"$GODOT" --headless --path "$PROJECT_DIR" --export-release "$PRESET_NAME" "$SITE_ROOT/$ENTRY_NAME"

if [[ ! -f "$SITE_ROOT/$ENTRY_NAME" ]]; then
	echo "ERROR: Pages demo export did not produce $SITE_ROOT/$ENTRY_NAME" >&2
	exit 1
fi

if [[ ! -f "$PROJECT_DIR/$ADDON_GDEXTENSION_RELATIVE_PATH" ]]; then
	echo "ERROR: staged addon manifest is missing: $PROJECT_DIR/$ADDON_GDEXTENSION_RELATIVE_PATH" >&2
	exit 1
fi

if [[ ! -d "$PROJECT_DIR/$ADDON_BIN_RELATIVE_DIR" ]]; then
	echo "ERROR: staged addon bin directory is missing: $PROJECT_DIR/$ADDON_BIN_RELATIVE_DIR" >&2
	exit 1
fi

if [[ ! -f "$PROJECT_DIR/$MODEL_RELATIVE_PATH" || ! -f "$PROJECT_DIR/$CONFIG_RELATIVE_PATH" ]]; then
	echo "ERROR: staged model bundle is incomplete under $PROJECT_DIR/$MODEL_RELATIVE_DIR" >&2
	exit 1
fi

if [[ ! -f "$PROJECT_DIR/$CMUDICT_RELATIVE_PATH" ]]; then
	echo "ERROR: staged cmudict is missing: $PROJECT_DIR/$CMUDICT_RELATIVE_PATH" >&2
	exit 1
fi

mkdir -p \
	"$SITE_ROOT/$ADDON_BIN_RELATIVE_DIR" \
	"$SITE_ROOT/$(dirname "$MODEL_RELATIVE_PATH")" \
	"$SITE_ROOT/$(dirname "$CMUDICT_RELATIVE_PATH")"

cp -f "$PROJECT_DIR/$ADDON_GDEXTENSION_RELATIVE_PATH" "$SITE_ROOT/$ADDON_GDEXTENSION_RELATIVE_PATH"
find "$PROJECT_DIR/$ADDON_BIN_RELATIVE_DIR" -mindepth 1 -maxdepth 1 ! -name '.gitignore' -exec cp -a {} "$SITE_ROOT/$ADDON_BIN_RELATIVE_DIR"/ \;
cp -f "$PROJECT_DIR/$MODEL_RELATIVE_PATH" "$SITE_ROOT/$MODEL_RELATIVE_PATH"
cp -f "$PROJECT_DIR/$CONFIG_RELATIVE_PATH" "$SITE_ROOT/$CONFIG_RELATIVE_PATH"
cp -f "$PROJECT_DIR/$CMUDICT_RELATIVE_PATH" "$SITE_ROOT/$CMUDICT_RELATIVE_PATH"
cp -f "$PROJECT_DIR/addons/piper_plus/LICENSE" "$SITE_ROOT/LICENSE.txt"
cp -f "$PROJECT_DIR/addons/piper_plus/THIRD_PARTY_LICENSES.txt" "$SITE_ROOT/THIRD_PARTY_LICENSES.txt"
printf '' > "$SITE_ROOT/.nojekyll"

cat > "$SITE_ROOT/public-demo-manifest.json" <<EOF
{
  "entry": "$ENTRY_NAME",
  "addon": {
    "gdextension_path": "$ADDON_GDEXTENSION_RELATIVE_PATH"
  },
  "runtime": {
    "thread_support": false,
    "execution_provider_policy": "cpu_only",
    "pwa_enabled": true
  },
  "model": {
    "key": "$MODEL_KEY",
    "path": "$MODEL_RELATIVE_PATH",
    "config_path": "$CONFIG_RELATIVE_PATH"
  },
  "dictionary": {
    "cmudict_path": "$CMUDICT_RELATIVE_PATH"
  },
  "notices": [
    "LICENSE.txt",
    "THIRD_PARTY_LICENSES.txt"
  ],
  "smoke": {
    "statusPrefix": "PAGES_DEMO status=",
    "summaryPrefix": "PAGES_DEMO summary=",
    "successStatus": "pass",
    "failureStatus": "fail",
    "timeoutMs": 240000
  }
}
EOF

cat > "$SITE_ROOT/build-meta.json" <<EOF
{
  "git_sha": "$BUILD_SHA",
  "generated_by": "scripts/ci/export-pages-demo.sh",
  "export_preset": "$PRESET_NAME",
  "entry": "$ENTRY_NAME",
  "model_key": "$MODEL_KEY"
}
EOF

node "$SCRIPT_DIR/patch-web-asm-const.mjs" "$SITE_ROOT"
node "$SCRIPT_DIR/validate-pages-artifact.mjs" "$SITE_ROOT" --expected-build "$BUILD_SHA"

printf 'PAGES_DEMO_EXPORT_READY %s\n' "$SITE_ROOT/$ENTRY_NAME"
