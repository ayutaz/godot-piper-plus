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
MODEL_KEY="${PIPER_PAGES_MODEL_KEY:-multilingual-test-medium}"
GODOT_TEMPLATES_VERSION="${GODOT_TEMPLATES_VERSION:-4.4.1.stable}"
MODEL_RELATIVE_DIR="piper_plus_assets/models/$MODEL_KEY"
MODEL_RELATIVE_PATH="$MODEL_RELATIVE_DIR/$MODEL_KEY.onnx"
CONFIG_RELATIVE_PATH="$MODEL_RELATIVE_DIR/$MODEL_KEY.onnx.json"
CMUDICT_RELATIVE_PATH="addons/piper_plus/dictionaries/cmudict_data.json"
ADDON_GDEXTENSION_RELATIVE_PATH="addons/piper_plus/piper_plus.gdextension"
ADDON_BIN_RELATIVE_DIR="addons/piper_plus/bin"
ADDON_ICON_RELATIVE_PATH="addons/piper_plus/icon.svg"
WEB_RELEASE_BINARY_RELATIVE_PATH="$ADDON_BIN_RELATIVE_DIR/libpiper_plus.web.template_release.wasm32.nothreads.wasm"
WEB_DEBUG_BINARY_RELATIVE_PATH="$ADDON_BIN_RELATIVE_DIR/libpiper_plus.web.template_debug.wasm32.nothreads.wasm"
LINUX_RELEASE_BINARY_RELATIVE_PATH="$ADDON_BIN_RELATIVE_DIR/libpiper_plus.linux.template_release.x86_64.so"
LINUX_DEBUG_BINARY_RELATIVE_PATH="$ADDON_BIN_RELATIVE_DIR/libpiper_plus.linux.template_debug.x86_64.so"
LINUX_ORT_RELATIVE_PATH="$ADDON_BIN_RELATIVE_DIR/libonnxruntime.linux.x86_64.so"
BUILD_SHA="${GITHUB_SHA:-$(git -C "$REPO_ROOT" rev-parse HEAD 2>/dev/null || printf 'local')}"
STAGED_TEMPLATE_ROOT="$(dirname "$PROJECT_DIR")/.ci/godot-web-templates/$GODOT_TEMPLATES_VERSION"
DEFAULT_MODEL_OVERRIDE="$REPO_ROOT/test/project/models/$MODEL_KEY.onnx"
DEFAULT_CONFIG_OVERRIDE="$REPO_ROOT/test/project/models/$MODEL_KEY.onnx.json"

resolve_custom_template_source_dir() {
	local candidate=""
	local official_templates_dir=""

	if [[ -n "${PIPER_PAGES_WEB_TEMPLATES_DIR:-}" ]]; then
		candidate="${PIPER_PAGES_WEB_TEMPLATES_DIR}"
		if [[ -f "$candidate/web_dlink_nothreads_debug.zip" && -f "$candidate/web_dlink_nothreads_release.zip" ]]; then
			printf '%s\n' "$candidate"
			return
		fi
	fi

	if [[ -n "${GODOT_WEB_TEMPLATES_DIR:-}" ]]; then
		candidate="${GODOT_WEB_TEMPLATES_DIR}"
		if [[ -f "$candidate/web_dlink_nothreads_debug.zip" && -f "$candidate/web_dlink_nothreads_release.zip" ]]; then
			printf '%s\n' "$candidate"
			return
		fi
	fi

	candidate="$REPO_ROOT/.ci/godot-web-templates/$GODOT_TEMPLATES_VERSION"
	if [[ -f "$candidate/web_dlink_nothreads_debug.zip" && -f "$candidate/web_dlink_nothreads_release.zip" ]]; then
		printf '%s\n' "$candidate"
		return
	fi

	case "$(uname -s)" in
		Darwin)
			official_templates_dir="${HOME}/Library/Application Support/Godot/export_templates/${GODOT_TEMPLATES_VERSION}"
			;;
		*)
			official_templates_dir="${HOME}/.local/share/godot/export_templates/${GODOT_TEMPLATES_VERSION}"
			;;
	esac

	if [[ -f "$official_templates_dir/web_dlink_nothreads_debug.zip" && -f "$official_templates_dir/web_dlink_nothreads_release.zip" ]]; then
		printf '%s\n' "$official_templates_dir"
		return
	fi

	echo "ERROR: custom Pages Web templates were not found. Set PIPER_PAGES_WEB_TEMPLATES_DIR or install templates with scripts/ci/install-godot-export-templates.sh." >&2
	exit 1
}

stage_custom_templates() {
	local source_dir=""

	source_dir="$(resolve_custom_template_source_dir)"
	rm -rf "$STAGED_TEMPLATE_ROOT"
	mkdir -p "$STAGED_TEMPLATE_ROOT"
	cp -f "$source_dir/web_dlink_nothreads_debug.zip" "$STAGED_TEMPLATE_ROOT/web_dlink_nothreads_debug.zip"
	cp -f "$source_dir/web_dlink_nothreads_release.zip" "$STAGED_TEMPLATE_ROOT/web_dlink_nothreads_release.zip"
	if [[ -f "$source_dir/version.txt" ]]; then
		cp -f "$source_dir/version.txt" "$STAGED_TEMPLATE_ROOT/version.txt"
	fi
}

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

MODEL_OVERRIDE="${PIPER_PAGES_MODEL_PATH:-}"
CONFIG_OVERRIDE="${PIPER_PAGES_CONFIG_PATH:-}"

if [[ -z "$MODEL_OVERRIDE" && -f "$DEFAULT_MODEL_OVERRIDE" ]]; then
	MODEL_OVERRIDE="$DEFAULT_MODEL_OVERRIDE"
fi

if [[ -z "$CONFIG_OVERRIDE" && -f "$DEFAULT_CONFIG_OVERRIDE" ]]; then
	CONFIG_OVERRIDE="$DEFAULT_CONFIG_OVERRIDE"
fi

PIPER_PAGES_PROJECT_DIR="$PROJECT_DIR" \
PIPER_PAGES_MODEL_KEY="$MODEL_KEY" \
PIPER_PAGES_MODEL_PATH="$MODEL_OVERRIDE" \
PIPER_PAGES_CONFIG_PATH="$CONFIG_OVERRIDE" \
bash "$SCRIPT_DIR/prepare-pages-demo-assets.sh"
stage_custom_templates
node "$SCRIPT_DIR/validate-pages-preset.mjs" --project "$PROJECT_DIR" --preset "$PRESET_NAME"

for required_path in \
	"$PROJECT_DIR/$ADDON_ICON_RELATIVE_PATH" \
	"$PROJECT_DIR/$WEB_RELEASE_BINARY_RELATIVE_PATH" \
	"$PROJECT_DIR/$WEB_DEBUG_BINARY_RELATIVE_PATH" \
	"$PROJECT_DIR/$LINUX_RELEASE_BINARY_RELATIVE_PATH" \
	"$PROJECT_DIR/$LINUX_DEBUG_BINARY_RELATIVE_PATH" \
	"$PROJECT_DIR/$LINUX_ORT_RELATIVE_PATH"
do
	if [[ ! -f "$required_path" ]]; then
		echo "ERROR: required Pages demo export input is missing: $required_path" >&2
		exit 1
	fi
done

"$GODOT" --headless --verbose --path "$PROJECT_DIR" --export-release "$PRESET_NAME" "$SITE_ROOT/$ENTRY_NAME"

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

if [[ ! -f "$PROJECT_DIR/$ADDON_ICON_RELATIVE_PATH" ]]; then
	echo "ERROR: staged addon icon is missing: $PROJECT_DIR/$ADDON_ICON_RELATIVE_PATH" >&2
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
