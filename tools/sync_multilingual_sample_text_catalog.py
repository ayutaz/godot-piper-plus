#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FIXTURE_PATH = ROOT / "tests" / "fixtures" / "multilingual_sample_text_catalog.json"
ADDON_PATH = ROOT / "addons" / "piper_plus" / "multilingual_sample_text_catalog.json"
OUTPUT_PATH = ROOT / "docs" / "generated" / "multilingual_sample_text_catalog.md"


def load_catalog() -> dict:
    catalog = json.loads(FIXTURE_PATH.read_text(encoding="utf-8"))
    if not isinstance(catalog, dict):
        raise SystemExit("sample text catalog must be a JSON object")
    languages = catalog.get("languages", [])
    if not isinstance(languages, list):
        raise SystemExit("sample text catalog.languages must be a JSON array")
    return catalog


def render_markdown(catalog: dict) -> str:
    lines = [
        "# Multilingual Sample Text Catalog",
        "",
        "Generated from `tests/fixtures/multilingual_sample_text_catalog.json`.",
        "",
        "This file is the doc-readable projection of the canonical 6-language template text catalog. Edit the JSON fixture first, then regenerate this file with `tools/sync_multilingual_sample_text_catalog.py`.",
        "",
        "| Language | Code | Template text | Placeholder |",
        "|---|---|---|---|",
    ]

    for language in catalog.get("languages", []):
        if not isinstance(language, dict):
            continue
        label = str(language.get("display_name", "")).replace("`", "'")
        code = str(language.get("language_code", ""))
        template_text = str(language.get("template_text", "")).replace("`", "'")
        placeholder = str(language.get("placeholder_text", "")).replace("`", "'")
        lines.append(f"| {label} | `{code}` | `{template_text}` | `{placeholder}` |")

    lines.extend(
        [
            "",
            "## Contract Notes",
            "",
            "- `language_code` is the canonical lookup key.",
            "- UI and smoke scenarios should read the same catalog projection.",
        ]
    )
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="fail if generated files are out of date")
    args = parser.parse_args()

    catalog = load_catalog()
    rendered = render_markdown(catalog)
    addon_content = json.dumps(catalog, ensure_ascii=False, indent=2) + "\n"

    if args.check:
        if not ADDON_PATH.exists():
            raise SystemExit(f"missing generated addon catalog: {ADDON_PATH}")
        if not OUTPUT_PATH.exists():
            raise SystemExit(f"missing generated doc catalog: {OUTPUT_PATH}")
        if ADDON_PATH.read_text(encoding="utf-8") != addon_content:
            raise SystemExit(f"generated addon catalog is out of date: {ADDON_PATH}")
        if OUTPUT_PATH.read_text(encoding="utf-8") != rendered:
            raise SystemExit(f"generated doc catalog is out of date: {OUTPUT_PATH}")
        return 0

    ADDON_PATH.write_text(addon_content, encoding="utf-8")
    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_PATH.write_text(rendered, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
