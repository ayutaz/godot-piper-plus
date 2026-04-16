# チケット一覧

更新日: 2026-04-14

このディレクトリは [docs/milestones.md](../milestones.md) から分解した実行チケットを管理します。各チケットは関連マイルストーンへリンクし、[docs/milestones.md](../milestones.md) 側も対応するチケットへリンクします。進捗を更新するときは、対象チケットと関連マイルストーンの両方を同じコミットで更新してください。

## 状態一覧

| 状態 | Ticket | タイトル | 親チケット | 関連マイルストーン | 依存 | 後続 |
|---|---|---|---|---|---|---|
| 進行中 | [TKT-004](TKT-004-android-export-runtime.md) | Android arm64 export / runtime 確認 | - | `M6` `M5` `M8` | - | `TKT-007` |
| 進行中 | [TKT-005](TKT-005-windows-android-export-error.md) | Windows local Android export error 切り分け | - | `M6` `M5` | - | `TKT-004` `TKT-007` |
| 進行中 | [TKT-007](TKT-007-release-finalization.md) | package / 文書 / Asset Library 最終化 | - | `M4` `M8` `M10` `M11` | `TKT-004` `TKT-005` `TKT-020` `TKT-021` `TKT-024` `TKT-025` | - |
| 要確認 | [TKT-020](TKT-020-web-japanese-browser-smoke-ci.md) | Web 日本語 browser smoke / CI gate | `統合済み (旧 TKT-018)` | `M10` `M5` | - | `TKT-021` `TKT-007` |
| 要確認 | [TKT-021](TKT-021-pages-japanese-demo-public-smoke.md) | GitHub Pages 日本語 demo / public smoke | `統合済み (旧 TKT-018)` | `M10` `M4` `M8` | `TKT-020` | `TKT-007` |
| 要確認 | [TKT-024](TKT-024-windows-six-language-runtime-smoke.md) | Windows 6-language runtime / smoke / template UI | `統合済み (旧 TKT-022)` | `M11` `M5` `M6` | - | `TKT-007` |
| 要確認 | [TKT-025](TKT-025-web-six-language-runtime-demo-smoke.md) | Web 6-language runtime / Pages demo / smoke | `統合済み (旧 TKT-022)` | `M11` `M5` `M8` | `TKT-021` | `TKT-007` |

## 運用メモ

- 完了したチケットは、成果を `docs/milestones.md` と残存チケットへ吸収したうえで、このディレクトリから削除します。履歴の正本は milestone と関連計画メモです。
- `M7 Web Support` の `W0` から `W4` は完了済みで、結果は `docs/milestones.md` に集約しています。個別 ticket は削除済みです。
- macOS / iOS の完了済み platform 確認も `docs/milestones.md` の `M6` へ吸収しています。
- `TKT-018` は完了済みです。Web 日本語対応の umbrella 内容は `M10`、`TKT-020`、`TKT-021`、[`docs/web-github-pages-plan.md`](../web-github-pages-plan.md) へ吸収したため削除しています。
- `TKT-019` は完了済みです。Web dictionary bootstrap / runtime の結果は `M10`、[`docs/web-github-pages-plan.md`](../web-github-pages-plan.md)、`TKT-020`、`TKT-021` へ吸収したため削除しています。
- `TKT-020` と `TKT-021` は Web 日本語対応の残件です。Phase 1 preview と M9 の English minimal Pages demo は完了扱いのまま残し、日本語 browser smoke と Pages public smoke の実証を `M10` として追跡します。
- `TKT-022` は完了済みです。Windows / Web 6-language umbrella 内容は `M11`、`TKT-024`、`TKT-025`、README 類へ吸収したため削除しています。
- `TKT-023` と `TKT-026` は完了済みです。6 言語 capability / template contract と docs sync の結果は `M11`、README 類、`TKT-007` へ吸収したため削除しています。
- `TKT-024` と `TKT-025` は Windows / Web の minimum 6-language text input / synthesize と template text UX の残件です。`M10` の `ja/en` baseline を前提に、`ja/en/zh/es/fr/pt` の explicit selection、sample text catalog、Windows packaged addon smoke、Web / Pages smoke の実証を `M11` として追跡します。
- multilingual contract は `tests/fixtures/multilingual_capability_matrix.json` を正本、`docs/generated/multilingual_capability_matrix.md` を投影として扱います。
- template text catalog は `tests/fixtures/multilingual_sample_text_catalog.json` を contract fixture、`addons/piper_plus/multilingual_sample_text_catalog.json` を addon compatibility copy、`addons/piper_plus/model_descriptors/multilingual-test-medium.json` を runtime metadata として扱います。
- `TKT-007` は release 完了条件の最終集約チケットで、他チケットの結果を取り込みます。Web については `M7` / `M9` の完了結果と `TKT-020` / `TKT-021`、Windows / Web 6-language については `TKT-024` / `TKT-025` の実証完了を依存に含めます。
- `M9 GitHub Pages Public Demo / Deploy` は English-only scope として完了済みです。branch 上で一時的に作成していた `TKT-012` から `TKT-017` は、成果を [`docs/milestones.md`](../milestones.md) と [`docs/web-github-pages-plan.md`](../web-github-pages-plan.md) へ吸収したため、この一覧から削除しています。
