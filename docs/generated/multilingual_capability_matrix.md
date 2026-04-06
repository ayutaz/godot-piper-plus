# Multilingual Capability Matrix

Generated from `tests/fixtures/multilingual_capability_matrix.json`.

This file is the doc-readable projection of the multilingual contract. Edit the JSON fixture first, then regenerate this file with `tools/sync_multilingual_capability_matrix.py`.

| Language | Tier | Backend | Routing | Text | Auto | Language ID | Notes |
|---|---|---|---|---|---|---|---|
| `ja` | `preview` | `openjtalk` | `auto` | `yes` | `yes` | `0` | auto/explicit |
| `en` | `preview` | `cmu_dict` | `auto` | `yes` | `yes` | `1` | auto/explicit |
| `es` | `experimental explicit-only` | `rule_based` | `explicit_only` | `yes` | `no` | `2` | experimental adapter |
| `fr` | `experimental explicit-only` | `rule_based` | `explicit_only` | `yes` | `no` | `4` | experimental adapter |
| `pt` | `experimental explicit-only` | `rule_based` | `explicit_only` | `yes` | `no` | `5` | experimental adapter |
| `zh` | `phoneme-only` | `raw_phoneme_only` | `phoneme_only` | `no` | `no` | `3` | raw phoneme only, text phonemization |

## Contract Notes

- `preview`: current model supports auto and explicit routing.
- `experimental explicit-only`: supported as a text adapter, but not parity-grade and not auto-routed.
- `phoneme-only`: model entry exists, but text input is rejected; use raw phoneme input.
