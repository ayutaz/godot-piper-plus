# TKT-012 Web GitHub Pages deploy / public demo

- 状態: `進行中`
- 主マイルストーン: [M9 GitHub Pages Public Demo / Deploy](../milestones.md#m9)
- 関連マイルストーン: [M7 Web Support 完成](../milestones.md#m7)
- 関連要求: post-preview Web public demo / GitHub Pages deployment
- 子チケット: [`TKT-013`](./m9-github-pages/TKT-013-github-pages-scope-asset-policy.md)
- 依存チケット: [`TKT-011`](TKT-011-web-browser-smoke-ci-docs.md)
- 後続チケット: [`TKT-013`](./m9-github-pages/TKT-013-github-pages-scope-asset-policy.md) 以降の `GP1` から `GP4` split tickets
- 関連メモ: [docs/web-github-pages-plan.md](../web-github-pages-plan.md)

## 進捗

- [x] `M9` を個別チケット化するための分解方針を決める
- [x] `docs/tickets/m9-github-pages/` 配下に GitHub Pages 専用チケットフォルダを作る
- [ ] `GP0` から `GP4` を個別チケットへ分割する
- [ ] GitHub Pages 向けの前提、制約、公開範囲を固定する
- [ ] Pages 専用の `no-threads` / `CPU-only` / English minimal demo preset を追加する
- [ ] `index.html` を入口にした公開用 export を組み立てる
- [ ] GitHub Actions に Pages artifact upload / deploy job を追加する
- [ ] deploy 後の public URL に対する smoke を追加する
- [ ] 公開デモの scope と既知制約を文書へ反映する

## タスク目的とゴール

- release gate 上では完了している Web preview support を維持したまま、GitHub Pages 上で再現可能な public demo と deploy 導線を追加する。
- ゴールは、GitHub Pages で addon load と最小 synthesize が成立する公開 URL を持ち、同じ workflow で build・deploy・smoke を通せる状態にすること。
- 実装順は [M9 GitHub Pages Public Demo / Deploy](../milestones.md#m9) の `GP0` から `GP4` に従います。

## 分割した子チケット

- [TKT-013 GitHub Pages scope / asset policy 固定](./m9-github-pages/TKT-013-github-pages-scope-asset-policy.md): `GP0`
- [TKT-014 GitHub Pages preset / public entry 整備](./m9-github-pages/TKT-014-github-pages-preset-public-entry.md): `GP1`
- [TKT-015 GitHub Pages deploy workflow 整備](./m9-github-pages/TKT-015-github-pages-deploy-workflow.md): `GP2`
- [TKT-016 GitHub Pages public URL smoke 整備](./m9-github-pages/TKT-016-github-pages-public-url-smoke.md): `GP3`
- `GP4`: `TKT-017` で個別化予定

## 実装メモ

- `M7 Web Support` は 2026-04-10 の GitHub Actions run `24223195868` で完了済みであり、このチケットはその release gate を reopen しません。
- 現在の browser smoke は `COOP` / `COEP` header を付ける Node static server 前提で成立しています。GitHub Pages 化では hosting 条件を吸収する別構成が必要です。
- 初回スコープは `no-threads`、`CPU-only`、English minimal demo を優先し、Japanese text input と `naist-jdic` を使う公開デモは対象外にします。
- `TKT-012` は umbrella と handoff の管理が責務です。`GP0` の具体的な scope / asset policy は [`TKT-013`](./m9-github-pages/TKT-013-github-pages-scope-asset-policy.md) を正本にします。

## 実装する内容の詳細

- `GP0` として [`TKT-013`](./m9-github-pages/TKT-013-github-pages-scope-asset-policy.md) で scope / asset policy / hosting 前提を固定する。
- `GP1` 以降で、Pages preset / public entry、deploy workflow、public URL smoke、文書反映を child ticket へ分割して進める。
- この umbrella ticket では child ticket 間の依存、handoff、acceptance の整合だけを管理する。

## 実装するために必要なエージェントチームの役割と人数

- `web delivery engineer` x1: Pages 向け export preset と artifact 構成を担当する。
- `CI / deployment engineer` x1: GitHub Actions Pages deploy を担当する。
- `QA / browser automation engineer` x1: public URL smoke と cache 問題の切り分けを担当する。
- `docs engineer` x1: scope、制約、公開手順の文書化を担当する。

## 提供範囲

- GitHub Pages 向け公開用 Web export。
- GitHub Actions による artifact upload / deploy。
- 公開 URL に対する smoke。
- Pages 公開の前提と制約をまとめた文書。

## テスト項目

- GitHub Actions 上で Pages 向け Web export が成功する。
- 成功時 artifact が GitHub Pages へ deploy される。
- public URL で addon load と最小 synthesize が成立する。
- `no-threads` / `CPU-only` / English minimal demo の scope が文書と実装で一致する。
- stale service worker cache が原因の false positive / false negative を切り分けられる。

## 実装する unit テスト

- Pages 向け preset の `PWA`、`index.html`、thread support 無効化などを確認する script-level check を追加する。
- workflow helper に deploy artifact の必須構成を確認する軽量チェックを追加する。

## 実装する e2e テスト

- build から Pages deploy までを通した workflow を実行する。
- deploy 後の `page_url` に headless browser でアクセスし、addon load と最小 synthesize を確認する。

## 実装に関する懸念事項

- service worker ベースの workaround は cache 問題を起こしやすく、更新反映の切り分けが必要です。
- 公開用 model のサイズとライセンスが package 方針や公開方針と衝突する可能性があります。
- thread build は hosting 条件の影響を受けやすく、初回 Pages 対応の難度を上げます。

## レビューする項目

- `M7 Web Support` の release gate を誤って reopen していないか。
- Pages 向け preset と CI smoke が同じ前提を見ているか。
- 公開デモの model 配布方針が package 方針と混線していないか。
- `preview support` の表現と public demo の表現が過大になっていないか。

## 一から作り直すとしたらどうするか

- release gate 用の Web smoke と public demo 用の deploy target を最初から別 project / preset として分離する。
- PWA、artifact 名、deploy metadata を 1 つの正本から生成する形に寄せる。

## 後続のタスクに連絡する内容

- public demo が成立したら、README へ公開 URL と scope を反映するか判断材料を返す。
- thread build や Japanese text input を Pages へ広げる場合は、別チケットとして Phase 2 拡張へ切り出す。
