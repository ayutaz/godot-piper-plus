# チケット一覧

更新日: 2026-04-12

このディレクトリは [docs/milestones.md](../milestones.md) から分解した実行チケットを管理します。各チケットは関連マイルストーンへリンクし、[docs/milestones.md](../milestones.md) 側も対応するチケットへリンクします。進捗を更新するときは、対象チケットと関連マイルストーンの両方を同じコミットで更新してください。

## 状態一覧

| 状態 | Ticket | タイトル | 親チケット | 関連マイルストーン | 依存 | 後続 |
|---|---|---|---|---|---|---|
| 進行中 | [TKT-004](TKT-004-android-export-runtime.md) | Android arm64 export / runtime 確認 | - | `M6` `M5` `M8` | - | `TKT-007` |
| 進行中 | [TKT-005](TKT-005-windows-android-export-error.md) | Windows local Android export error 切り分け | - | `M6` `M5` | - | `TKT-004` `TKT-007` |
| 進行中 | [TKT-007](TKT-007-release-finalization.md) | package / 文書 / Asset Library 最終化 | - | `M4` `M8` | `TKT-004` `TKT-005` `TKT-018` | - |
| 進行中 | [TKT-018](TKT-018-web-japanese-support.md) | Web 日本語対応 | - | `M10` `M4` `M5` `M8` | - | `TKT-007` |
| 未着手 | [TKT-019](TKT-019-web-japanese-dictionary-bootstrap.md) | Web 日本語 dictionary bootstrap / runtime | `TKT-018` | `M10` `M5` | `TKT-018` | `TKT-020` `TKT-021` |
| 未着手 | [TKT-020](TKT-020-web-japanese-browser-smoke-ci.md) | Web 日本語 browser smoke / CI gate | `TKT-018` | `M10` `M5` | `TKT-019` | `TKT-021` `TKT-007` |
| 未着手 | [TKT-021](TKT-021-pages-japanese-demo-public-smoke.md) | GitHub Pages 日本語 demo / public smoke | `TKT-018` | `M10` `M4` `M8` | `TKT-019` `TKT-020` | `TKT-007` |

## 運用メモ

- 完了したチケットは、成果を `docs/milestones.md` と残存チケットへ吸収したうえで、このディレクトリから削除します。履歴の正本は milestone と関連計画メモです。
- `M7 Web Support` の `W0` から `W4` は完了済みで、結果は `docs/milestones.md` に集約しています。個別 ticket は削除済みです。
- macOS / iOS の完了済み platform 確認も `docs/milestones.md` の `M6` へ吸収しています。
- `TKT-018` から `TKT-021` は Web 日本語対応の follow-up track です。Phase 1 preview と English-only Pages demo は完了扱いのまま残し、`naist-jdic` bootstrap、日本語 text input / synthesize、browser smoke、Pages public smoke を `M10` として追跡します。
- multilingual contract は `tests/fixtures/multilingual_capability_matrix.json` を正本、`docs/generated/multilingual_capability_matrix.md` を投影として扱います。
- `TKT-007` は release 完了条件の最終集約チケットで、他チケットの結果を取り込みます。Web については `M7` / `M9` の完了結果と `TKT-018` 系の日本語対応完了を依存に含めます。
- `M9 GitHub Pages Public Demo / Deploy` は English-only scope として完了済みです。branch 上で一時的に作成していた `TKT-012` から `TKT-017` は、成果を [`docs/milestones.md`](../milestones.md) と [`docs/web-github-pages-plan.md`](../web-github-pages-plan.md) へ吸収したため、この一覧から削除しています。
