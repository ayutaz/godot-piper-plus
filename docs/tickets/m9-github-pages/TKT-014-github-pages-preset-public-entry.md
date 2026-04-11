# TKT-014 GitHub Pages preset / public entry 整備

- 状態: `未着手`
- フェーズ: `GP1`
- 主マイルストーン: [M9 GitHub Pages Public Demo / Deploy](../../milestones.md#m9)
- 関連マイルストーン: [M7 Web Support 完成](../../milestones.md#m7)
- 親チケット: [TKT-012 Web GitHub Pages deploy / public demo](../TKT-012-web-github-pages-deploy.md)
- 依存チケット: [`TKT-013`](./TKT-013-github-pages-scope-asset-policy.md)
- 後続チケット: [TKT-015](./TKT-015-github-pages-deploy-workflow.md) [TKT-016](./TKT-016-github-pages-public-url-smoke.md) [TKT-017](./TKT-017-github-pages-docs-operational-notes.md)
- 関連メモ: [docs/web-github-pages-plan.md](../../web-github-pages-plan.md) [M9 GitHub Pages 個別チケット](./README.md)

## 進捗

- [ ] Pages 向けの `no-threads` / `CPU-only` / English minimal demo preset を追加する
- [ ] `index.html` を入口にした公開用 export target を固定する
- [ ] public demo 用 scene / asset / UI 導線を固定する
- [ ] `GP2` と `GP3` が利用する export artifact 契約を固定する

## タスク目的とゴール

- GitHub Pages 向け公開物の入口を、preset、export target、minimal demo、artifact 構成の 4 点で固定する。
- 後続の deploy workflow と public URL smoke が、どの出力を publish / verify すべきか迷わない状態を作る。

## 実装する内容の詳細

- `test/project/export_presets.cfg` を拡張するか、公開用 project / preset を別に切り出して Pages 向け export target を定義する。
- `variant/thread_support=false`、`progressive_web_app/enabled=true`、`progressive_web_app/ensure_cross_origin_isolation_headers=true`、`index.html` を満たす preset を用意する。
- 公開入口として使う minimal demo の scene、asset、UI テキスト、説明導線を固定する。
- export 後に GitHub Pages artifact として publish するファイル構成を固定し、`GP2` の workflow と `GP3` の smoke が同じ契約を参照できるようにする。
- model / dictionary / config の読み込み前提を `GP0` の scope と整合する形で整理する。

## 実装するために必要なエージェントチームの役割と人数

- `godot export engineer` x1: Web preset と export target を設計する。
- `web delivery engineer` x1: `index.html` を入口にした公開 artifact 構成を整える。
- `demo UX engineer` x1: minimal demo の scene / UI / copy を調整する。
- `QA engineer` x1: 後続の smoke が参照できる export 契約をレビューする。

## 提供範囲

- Pages 向け Web preset。
- `index.html` を入口にした公開用 export target。
- minimal public demo の scene / asset / UI 導線。
- `GP2` と `GP3` が参照する export artifact 契約。

## テスト項目

- preset が `no-threads`、`CPU-only`、PWA 有効、`index.html` を満たしている。
- export 結果が GitHub Pages artifact として publish 可能な構成になっている。
- public demo の scene / asset / UI が `GP0` で固定した scope と矛盾していない。
- `GP2` と `GP3` が参照する出力契約が文書と一致している。

## 実装する unit テスト

- preset の key/value が `GP0` の前提と一致していることを確認する script-level check を追加する。
- export artifact に `index.html`、必要な `.js` / `.wasm` / resource 群が揃っていることを確認する軽量チェックを追加する。

## 実装する e2e テスト

- local export を生成し、Pages 相当の静的ホスト前提で minimal demo が起動できることを確認する。
- addon load と最小 synthesize に必要な resource 解決が export 物のままで成立することを確認する。

## 実装に関する懸念事項

- `test/project` をそのまま使うか、公開用 project を切り出すかで保守コストが変わる。
- PWA と `index.html` の入口調整を先に誤ると、workflow と smoke の前提が後から崩れる。
- public demo 用 asset を足しすぎると、初回 Pages 対応の binary / download size が膨らむ。

## レビューする項目

- preset が `GP0` で固定した `no-threads` / `CPU-only` / English minimal demo に一致しているか。
- export artifact 契約が `GP2` と `GP3` から再利用しやすい形になっているか。
- minimal demo の scene / UI が公開デモとして十分だが、`M7` の preview scopeを超えていないか。
- `test/project` と公開用入口の責務が混線していないか。

## 一から作り直すとしたらどうするか

- Web smoke 用 fixture と public demo 用 project を最初から分離し、公開 artifact だけを生成する専用 export pipeline を作る。
- preset を手編集ではなく、1 つの deployment contract から生成する形にして environment ごとの差分を減らす。
- demo UI 文言と asset manifest を外出しし、Pages 用公開物の差し替えをノーコードでできるようにする。

## 後続のタスクに連絡する内容

- [TKT-015](./TKT-015-github-pages-deploy-workflow.md) には、workflow がこのチケットで固定した artifact 構成とファイル名をそのまま publish するよう引き継ぐ。
- [TKT-016](./TKT-016-github-pages-public-url-smoke.md) には、public URL smoke がこのチケットで固定した `index.html` と minimal demo 導線を前提に設計されるよう引き継ぐ。
- [TKT-017](./TKT-017-github-pages-docs-operational-notes.md) には、公開手順と known limitations がこのチケットの preset / artifact 契約と一致するよう引き継ぐ。
