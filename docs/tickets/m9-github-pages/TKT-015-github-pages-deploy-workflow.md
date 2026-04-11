# TKT-015 GitHub Pages deploy workflow 整備

- 状態: `進行中`
- フェーズ: `GP2`
- 主マイルストーン: [M9 GitHub Pages Public Demo / Deploy](../../milestones.md#m9)
- 関連マイルストーン: [M7 Web Support 完成](../../milestones.md#m7)
- 親チケット: [TKT-012 Web GitHub Pages deploy / public demo](../TKT-012-web-github-pages-deploy.md)
- 依存チケット: [`TKT-013`](./TKT-013-github-pages-scope-asset-policy.md) [`TKT-014`](./TKT-014-github-pages-preset-public-entry.md)
- 後続チケット: [TKT-016](./TKT-016-github-pages-public-url-smoke.md) [TKT-017](./TKT-017-github-pages-docs-operational-notes.md)
- 関連メモ: [docs/web-github-pages-plan.md](../../web-github-pages-plan.md) [M9 GitHub Pages 個別チケット](./README.md)

## 進捗

- [x] Pages 用 artifact upload / deploy の workflow 構成を決める
- [x] `configure-pages`、`upload-pages-artifact`、`deploy-pages` の利用方針を固定する
- [x] Pages deploy の permissions、environment、concurrency、artifact 名を固定する
- [x] `GP3` が利用する `page_url` / metadata の受け渡しを固定する

## タスク目的とゴール

- GitHub Actions から GitHub Pages へ publish する導線を、workflow、permissions、artifact 契約、deploy 出力の観点で固定する。
- preset 側が作った export 物を、手作業なしで publish し、後続の public URL smoke に必要な metadata を確実に渡せるようにする。

## 実装する内容の詳細

- Pages 専用 workflow [`.github/workflows/pages.yml`](../../../.github/workflows/pages.yml) を分離し、`build-pages-demo`、`deploy-pages-demo`、`smoke-pages-demo` に責務を分ける。
- `actions/configure-pages`、`actions/upload-pages-artifact`、`actions/deploy-pages` を用いた deploy job の責務を定義する。
- deploy 対象 artifact の入力ディレクトリ、必要ファイル、命名規則、成功条件を固定する。
- workflow の permissions、environment、concurrency、branch 条件、manual trigger の扱いを整理する。`main` push は自動 deploy、`workflow_dispatch` は current ref を手動 deploy できる前提に揃える。
- deploy 後に `page_url` を後続 job へ受け渡す方法を固定し、`GP3` が同じ metadata を参照できるようにする。

## 実装するために必要なエージェントチームの役割と人数

- `CI / deployment engineer` x1: workflow の構成と Pages deploy を設計する。
- `release engineer` x1: branch 条件、environment、permissions を整理する。
- `web delivery engineer` x1: artifact 契約と Pages 公開物の整合を確認する。
- `QA / observability engineer` x1: deploy 結果の可観測性と `page_url` の受け渡しをレビューする。

## 提供範囲

- GitHub Pages deploy workflow。
- deploy 用 artifact 契約と成功条件。
- permissions、environment、concurrency を含む運用方針。
- `GP3` が参照する `page_url` / deploy metadata の出力。

## テスト項目

- Pages 用 export artifact が成功時のみ upload / deploy される。
- workflow が必要な permissions と environment を持ち、不要な権限を増やしていない。
- deploy 後に `page_url` が後続 job から参照できる。
- deploy workflow の入力が `GP1` の export artifact 契約と一致している。

## 実装する unit テスト

- deploy helper script や workflow helper が artifact 構成と必須変数を正しく検証するチェックを追加する。
- Pages deploy job が必要なファイルを受け取っているかを確認する軽量チェックを追加する。

## 実装する e2e テスト

- CI 上で Pages deploy まで含む workflow を実行し、deploy job が成功することを確認する。
- deploy 結果として返る `page_url` を後続 job へ引き渡せることを確認する。

## 実装に関する懸念事項

- Pages deploy は branch 条件、repository settings、permissions に依存するため、CI 設定差分で失敗しやすい。
- GitHub Pages は repo ごとに 1 つの公開 site なので、branch ごとの concurrency にすると deploy が競合しやすい。
- `build.yml` に詰め込みすぎると Web preview の既存 job と責務が混線する。
- artifact 契約を曖昧にしたまま workflow を組むと、`GP1` と `GP3` の変更で簡単に壊れる。

## レビューする項目

- workflow が `GP1` の export artifact 契約をそのまま受け取れる構成になっているか。
- Pages deploy に必要な permissions / environment だけを持ち、過剰な権限を要求していないか。
- `main` 自動 deploy と branch 手動 deploy の条件が明確で、Pages site 競合を避ける concurrency になっているか。
- `page_url` の出力や failure 時の診断情報が `GP3` で再利用しやすいか。
- preview 用の既存 Web CI と public deploy 用 CI の責務が明確に分かれているか。

## 一から作り直すとしたらどうするか

- `build` と `deploy-pages` を最初から別 workflow に分け、artifact 契約だけで接続する。
- Pages deploy 用の reusable workflow か composite action を用意し、artifact path と metadata を 1 つの入力に揃える。
- deploy 後の metadata を JSON manifest として保存し、smoke と docs が同じ正本を参照するようにする。

## 後続のタスクに連絡する内容

- [TKT-016](./TKT-016-github-pages-public-url-smoke.md) には、public URL smoke がこのチケットで出力する `page_url`、deploy metadata、workflow / job 名を前提に設計されるよう引き継ぐ。
- [TKT-017](./TKT-017-github-pages-docs-operational-notes.md) には、README と運用メモがこのチケットの trigger 条件、permissions、workflow / job 名、environment 名、artifact 名、rerun / redeploy 手順と一致するよう引き継ぐ。
