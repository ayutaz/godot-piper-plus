# M9 GitHub Pages 個別チケット

更新日: 2026-04-11

このディレクトリは [`M9 GitHub Pages Public Demo / Deploy`](../../milestones.md#m9) を `GP0` から `GP4` の個別チケットへ分解して管理します。umbrella ticket は [`TKT-012`](../TKT-012-web-github-pages-deploy.md) です。進捗を更新するときは、このディレクトリの該当チケットと [`docs/milestones.md`](../../milestones.md)、必要なら [`TKT-012`](../TKT-012-web-github-pages-deploy.md) を同じコミットで更新してください。

## 状態一覧

| 状態 | Ticket | フェーズ | 概要 | 依存 | 後続 |
|---|---|---|---|---|---|
| 進行中 | [TKT-013](./TKT-013-github-pages-scope-asset-policy.md) | `GP0` | public demo scope / asset policy / hosting 前提の固定 | `TKT-011` | `TKT-014` `TKT-015` `TKT-017` |
| 未着手 | [TKT-014](./TKT-014-github-pages-preset-public-entry.md) | `GP1` | Pages 向け preset / public entry 整備 | `TKT-013` | `TKT-015` `TKT-016` `TKT-017` |
| 未着手 | [TKT-015](./TKT-015-github-pages-deploy-workflow.md) | `GP2` | Pages deploy workflow 整備 | `TKT-013` `TKT-014` | `TKT-016` `TKT-017` |
| 未着手 | [TKT-016](./TKT-016-github-pages-public-url-smoke.md) | `GP3` | public URL smoke 整備 | `TKT-014` `TKT-015` | `TKT-017` |
| 未着手 | `TKT-017` | `GP4` | 文書 / 運用メモ最終化 | `TKT-013` `TKT-014` `TKT-015` `TKT-016` | - |

## 運用メモ

- `GP0` から `GP4` は `M9` の実装順に対応します。`GP0` が scope を固定し、`GP1` 以降が実装と検証を進めます。
- 各チケットは、目的、詳細、エージェントチーム構成、提供範囲、テスト、懸念事項、レビュー項目、作り直す場合の設計思想、後続への引き継ぎを必須セクションとして持ちます。
- `M7` は完了済みの preview support です。このディレクトリのチケットは `M7` を reopen せず、Pages 向け public demo / deploy だけを扱います。
