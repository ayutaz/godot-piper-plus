# チケット一覧

更新日: 2026-04-11

このディレクトリは [docs/milestones.md](../milestones.md) から分解した実行チケットを管理します。各チケットは関連マイルストーンへリンクし、[docs/milestones.md](../milestones.md) 側も対応するチケットへリンクします。進捗を更新するときは、対象チケットと関連マイルストーンの両方を同じコミットで更新してください。

## 状態一覧

| 状態 | Ticket | タイトル | 親チケット | 関連マイルストーン | 依存 | 後続 |
|---|---|---|---|---|---|---|
| 完了 | [TKT-002](TKT-002-web-platform.md) | Web platform 対応 | - | `M7` `M4` `M5` `M8` | - | `TKT-007` |
| 完了 | [TKT-003](TKT-003-macos-packaged-smoke.md) | macOS arm64 packaged addon smoke 確認 | - | `M6` `M5` `M8` | - | `TKT-007` |
| 進行中 | [TKT-004](TKT-004-android-export-runtime.md) | Android arm64 export / runtime 確認 | - | `M6` `M5` `M8` | - | `TKT-007` |
| 進行中 | [TKT-005](TKT-005-windows-android-export-error.md) | Windows local Android export error 切り分け | - | `M6` `M5` | - | `TKT-004` `TKT-007` |
| 完了 | [TKT-006](TKT-006-ios-export-link-smoke.md) | iOS arm64 export / link smoke 確認 | - | `M6` `M5` `M8` | - | `TKT-007` |
| 進行中 | [TKT-007](TKT-007-release-finalization.md) | package / 文書 / Asset Library 最終化 | - | `M4` `M8` | `TKT-004` `TKT-005` | - |
| 完了 | [TKT-008](TKT-008-web-template-toolchain-bootstrap.md) | Web custom template / toolchain bootstrap | `TKT-002` | `M7` `M5` | - | `TKT-009` `TKT-010` |
| 完了 | [TKT-009](TKT-009-web-manifest-package-export-preset.md) | Web manifest / package / export preset 整備 | `TKT-002` | `M7` `M4` `M5` | `TKT-008` | `TKT-010` `TKT-011` |
| 完了 | [TKT-010](TKT-010-web-runtime-ort-adaptation.md) | Web runtime adaptation / ORT Web 対応 | `TKT-002` | `M7` `M5` | `TKT-008` `TKT-009` | `TKT-011` |
| 完了 | [TKT-011](TKT-011-web-browser-smoke-ci-docs.md) | Web browser smoke / CI / 文書反映 | `TKT-002` | `M7` `M4` `M5` `M8` | `TKT-009` `TKT-010` | `TKT-007` |
| 進行中 | [TKT-012](TKT-012-web-github-pages-deploy.md) | Web GitHub Pages deploy / public demo | - | `M9` `M7` | `TKT-011` | `TKT-013` `TKT-014` `TKT-015` `TKT-016` `TKT-017` |
| 完了 | [TKT-013](m9-github-pages/TKT-013-github-pages-scope-asset-policy.md) | GitHub Pages scope / asset policy 固定 | `TKT-012` | `M9` `M7` | `TKT-011` | `TKT-014` `TKT-015` `TKT-016` `TKT-017` |
| 進行中 | [TKT-014](m9-github-pages/TKT-014-github-pages-preset-public-entry.md) | GitHub Pages preset / public entry 整備 | `TKT-012` | `M9` `M7` | `TKT-013` | `TKT-015` `TKT-016` `TKT-017` |
| 進行中 | [TKT-015](m9-github-pages/TKT-015-github-pages-deploy-workflow.md) | GitHub Pages deploy workflow 整備 | `TKT-012` | `M9` `M7` | `TKT-013` `TKT-014` | `TKT-016` `TKT-017` |
| 進行中 | [TKT-016](m9-github-pages/TKT-016-github-pages-public-url-smoke.md) | GitHub Pages public URL smoke 整備 | `TKT-012` | `M9` `M7` | `TKT-014` `TKT-015` | `TKT-017` |
| 進行中 | [TKT-017](m9-github-pages/TKT-017-github-pages-docs-operational-notes.md) | GitHub Pages docs / operational notes 最終化 | `TKT-012` | `M9` `M7` | `TKT-013` `TKT-014` `TKT-015` `TKT-016` | - |

## 運用メモ

- 完了したチケットは、依存する umbrella ticket や release ticket が結果を取り込むまではこのディレクトリに残し、進捗可視化を優先します。整理タイミングで成果だけを `docs/milestones.md` と残存チケットへ反映して削除します。
- `TKT-002` は Web 要求全体を束ねる umbrella ticket です。`W0` feasibility / scope 固定を持ち、実装は `TKT-008` から `TKT-011` の `W1` から `W4` に分割しています。技術調査は `docs/web-platform-research.md` に集約しています。`親チケット` は進捗の集約先を示し、`依存` と `後続` は実行順だけを表します。
- Web の `TKT-008` から `TKT-011` は、2026-04-10 の GitHub Actions run `24223195868` で `Build Web` と browser smoke が通り、`threads` / `no-threads` の両方で `WEB_SMOKE status=pass` を確認済みです。
- multilingual contract は `tests/fixtures/multilingual_capability_matrix.json` を正本、`docs/generated/multilingual_capability_matrix.md` を投影として扱います。
- `TKT-003` から `TKT-006` は platform verification の結果確定チケットです。2026-04-10 時点で macOS packaged smoke と iOS export smoke は完了、Android は export smoke 成功後の runtime / local 再現性だけが残っています。
- `TKT-008` から `TKT-011` は `M7 Web Support` を `W1` から `W4` に分解した実装チケットです。進捗更新時は `TKT-002` と `docs/milestones.md` の `M7` セクションも同時に更新します。
- `TKT-007` は release 完了条件の最終集約チケットで、他チケットの結果を取り込みます。Web については `TKT-002` の umbrella 状態だけでなく、`TKT-011` の smoke / README 反映完了も依存に含めます。
- `TKT-012` は独立マイルストーン `M9 GitHub Pages Public Demo / Deploy` の実行チケットです。release gate 上の `M7` を reopen せずに、GitHub Pages 向けの public demo / deploy を追跡します。前提整理は `docs/web-github-pages-plan.md` にまとめます。
- `M9` の個別実行チケットは `docs/tickets/m9-github-pages/` に分けて置き、umbrella ticket の `TKT-012` と [`docs/milestones.md`](../milestones.md) 側の `GP0` から `GP4` と相互にリンクさせます。
