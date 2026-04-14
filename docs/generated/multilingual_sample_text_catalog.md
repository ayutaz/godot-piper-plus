# Multilingual Sample Text Catalog

Generated from `tests/fixtures/multilingual_sample_text_catalog.json`.

This file is the doc-readable projection of the canonical 6-language template text catalog. Edit the JSON fixture first, then regenerate this file with `tools/sync_multilingual_sample_text_catalog.py`.

| Language | Code | Template text | Placeholder |
|---|---|---|---|
| Japanese (ja) | `ja` | `こんにちは、今日は良い天気ですね。` | `日本語テキストを入力してください` |
| English (en) | `en` | `Hello, how are you today?` | `Enter English text to synthesize` |
| Chinese (zh) | `zh` | `你好，今天天气很好。` | `请输入中文文本` |
| Spanish (es) | `es` | `Hola, ¿cómo estás hoy?` | `Introduce texto en español` |
| French (fr) | `fr` | `Bonjour, comment allez-vous ?` | `Entrez du texte en français` |
| Portuguese (pt) | `pt` | `Olá, como você está hoje?` | `Digite texto em português` |

## Contract Notes

- `language_code` is the canonical lookup key.
- UI and smoke scenarios should read the same catalog projection.
