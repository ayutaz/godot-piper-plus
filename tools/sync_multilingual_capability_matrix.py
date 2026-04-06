#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from pathlib import Path
from textwrap import dedent


ROOT = Path(__file__).resolve().parents[1]
MATRIX_PATH = ROOT / "tests" / "fixtures" / "multilingual_capability_matrix.json"
OUTPUT_PATH = ROOT / "docs" / "generated" / "multilingual_capability_matrix.md"


def load_rows() -> list[dict]:
    with MATRIX_PATH.open("r", encoding="utf-8") as handle:
        rows = json.load(handle)
    if not isinstance(rows, list):
        raise SystemExit("capability matrix must be a JSON array")
    return rows


def tier_label(row: dict) -> str:
    tier = row.get("support_tier", "experimental")
    routing_mode = row.get("routing_mode", "explicit_only")
    if tier == "preview":
        return "preview"
    if tier == "experimental" and routing_mode == "explicit_only":
        return "experimental explicit-only"
    if tier == "phoneme_only":
        return "phoneme-only"
    return str(tier)


def support_label(enabled: bool) -> str:
    return "yes" if enabled else "no"


def render_markdown(rows: list[dict]) -> str:
    lines = [
        "# Multilingual Capability Matrix",
        "",
        "Generated from `tests/fixtures/multilingual_capability_matrix.json`.",
        "",
        "This file is the doc-readable projection of the multilingual contract. Edit the JSON fixture first, then regenerate this file with `tools/sync_multilingual_capability_matrix.py`.",
        "",
        "| Language | Tier | Backend | Routing | Text | Auto | Language ID | Notes |",
        "|---|---|---|---|---|---|---|---|",
    ]

    for row in rows:
        language = row["language_code"]
        tier = tier_label(row)
        backend = str(row.get("frontend_backend", "raw_phoneme_only"))
        routing = row["routing_mode"]
        text = support_label(bool(row.get("text_supported", False)))
        auto = support_label(bool(row.get("auto_supported", False)))
        language_id = row.get("expected_language_id", -1)

        notes = []
        if tier == "preview":
            notes.append("auto/explicit")
        elif tier == "experimental explicit-only":
            notes.append("experimental adapter")
        elif tier == "phoneme-only":
            notes.append("raw phoneme only")
        if row.get("expected_error_contains"):
            notes.append(str(row["expected_error_contains"]))

        lines.append(
            f"| `{language}` | `{tier}` | `{backend}` | `{routing}` | `{text}` | `{auto}` | `{language_id}` | "
            f"{', '.join(notes) if notes else '-'} |"
        )

    lines.extend(
        [
            "",
            "## Contract Notes",
            "",
            "- `preview`: current model supports auto and explicit routing.",
            "- `experimental explicit-only`: supported as a text adapter, but not parity-grade and not auto-routed.",
            "- `phoneme-only`: model entry exists, but text input is rejected; use raw phoneme input.",
        ]
    )
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="fail if the generated file is out of date")
    args = parser.parse_args()

    rows = load_rows()
    rendered = render_markdown(rows)

    if args.check:
        if not OUTPUT_PATH.exists():
            raise SystemExit(f"missing generated file: {OUTPUT_PATH}")
        current = OUTPUT_PATH.read_text(encoding="utf-8")
        if current != rendered:
            raise SystemExit(f"generated file is out of date: {OUTPUT_PATH}")
        return 0

    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT_PATH.write_text(rendered, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
