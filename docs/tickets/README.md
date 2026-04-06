# チケット一覧

更新日: 2026-04-06

このディレクトリは [docs/milestones.md](../milestones.md) から分解した実行チケットを管理します。各チケットは関連マイルストーンへリンクし、[docs/milestones.md](../milestones.md) 側も対応するチケットへリンクします。進捗を更新するときは、対象チケットと関連マイルストーンの両方を同じコミットで更新してください。

## 状態一覧

| 状態 | Ticket | タイトル | 関連マイルストーン | 依存 | 後続 |
|---|---|---|---|---|---|
| 完了 | [TKT-001](TKT-001-multilingual-parity.md) | multilingual parity 拡張 | `M2` `M5` `M8` | - | `TKT-007` |
| 進行中 | [TKT-002](TKT-002-web-platform.md) | Web platform 対応 | `M7` `M4` `M5` `M8` | - | `TKT-007` |
| 要確認 | [TKT-003](TKT-003-macos-packaged-smoke.md) | macOS arm64 packaged addon smoke 確認 | `M6` `M5` `M8` | - | `TKT-007` |
| 進行中 | [TKT-004](TKT-004-android-export-runtime.md) | Android arm64 export / runtime 確認 | `M6` `M5` `M8` | - | `TKT-007` |
| 進行中 | [TKT-005](TKT-005-windows-android-export-error.md) | Windows local Android export error 切り分け | `M6` `M5` | - | `TKT-004` `TKT-007` |
| 要確認 | [TKT-006](TKT-006-ios-export-link-smoke.md) | iOS arm64 export / link smoke 確認 | `M6` `M5` `M8` | - | `TKT-007` |
| 進行中 | [TKT-007](TKT-007-release-finalization.md) | package / 文書 / Asset Library 最終化 | `M4` `M8` | `TKT-001` `TKT-002` `TKT-003` `TKT-004` `TKT-006` | - |

## 運用メモ

- `TKT-001` は multilingual contract の固定と matrix-first 検証まで完了しています。release 反映は `TKT-007` で継続管理します。
- `TKT-002` は Web 要求の設計と実装を閉じるための feature チケットです。技術調査は `docs/web-platform-research.md` に集約しています。
- `TKT-001` の multilingual contract は `tests/fixtures/multilingual_capability_matrix.json` を正本、`docs/generated/multilingual_capability_matrix.md` を投影として扱います。
- `TKT-003` から `TKT-006` は platform verification の結果確定チケットです。
- `TKT-007` は release 完了条件の最終集約チケットで、他チケットの結果を取り込みます。
