# Web GitHub Pages 公開メモ

更新日: 2026-04-14

関連文書:

- [README.md](../README.md)
- [マイルストーン管理](./milestones.md)
- [チケット一覧](./tickets/README.md)

## 現状

- release gate 上の Web preview support は完了しています。`M7 Web Support` は `preview support`、`CPU-only`、custom Web export template、browser smoke、README 反映までを受け入れ条件として閉じています。
- GitHub Pages 対応は release gate 外の follow-up として開始し、[`M9 GitHub Pages Public Demo / Deploy`](./milestones.md#m9) として完了しています。
- 2026-04-10 の GitHub Actions run `24223195868` で `Build Web` と browser smoke が通っており、`threads` / `no-threads` の両方で `WEB_SMOKE status=pass` を確認済みです。
- 2026-04-11 の GitHub Actions run `24282051911` では Pages demo の build、deploy、public URL smoke が成功しています。
- 公開デモは [https://ayutaz.github.io/godot-piper-plus/](https://ayutaz.github.io/godot-piper-plus/) で公開中です。
- 現在の runtime contract は Web 向け `EP_CPU` 固定です。
- current branch では Pages demo を `multilingual-test-medium`、staged `naist-jdic`、shared descriptor / template catalog を使う canonical 6-language demo へ拡張し、startup self-test、selector / template text UX、local / public smoke の scenario loop を実装済みです。
- branch の workflow 定義では `generate-pages-manifest.mjs --list-language-codes` で catalog の全言語を列挙し、local / public smoke を `ja/en/zh/es/fr/pt` で回す構成へ更新済みです。残りの CI 実証、`main` 反映、release 文書化は [`M10 Web Japanese Support / Pages Japanese Demo 完成`](./milestones.md#m10)、[`M11 Windows / Web 6-Language Text Input / Template UX 完成`](./milestones.md#m11)、[`TKT-007`](./tickets/TKT-007-release-finalization.md) で管理します。
- M9 向けに一時的に作成した GitHub Pages ticket 群の内容は、完了時にこの文書と [`docs/milestones.md`](./milestones.md) へ吸収しています。

## 公開スコープ

main で公開中の scope:

- `no-threads`
- `CPU-only`
- English minimal demo
- `index.html` export
- `multilingual-test-medium` 1 model 同梱
- runtime download なし
- PWA と cross-origin isolation workaround を有効化

current branch で実装済みの拡張 scope:

- `no-threads`
- `CPU-only`
- canonical 6-language selector / template text demo
- `index.html` export
- `multilingual-test-medium` 1 model 同梱
- staged `naist-jdic` 同梱
- runtime download なし
- PWA と cross-origin isolation workaround を有効化

## 現在の実装

- public demo project: [`pages_demo`](../pages_demo)
- project staging: [`scripts/ci/prepare-pages-demo-assets.sh`](../scripts/ci/prepare-pages-demo-assets.sh)
- export: [`scripts/ci/export-pages-demo.sh`](../scripts/ci/export-pages-demo.sh)
- local / public smoke: [`scripts/ci/run-pages-demo-smoke.mjs`](../scripts/ci/run-pages-demo-smoke.mjs)
- workflow: [`.github/workflows/pages.yml`](../.github/workflows/pages.yml)
- artifact contract: `public-demo-manifest.json` と `build-meta.json`
- metadata source: `addons/piper_plus/model_descriptors/multilingual-test-medium.json` と `addons/piper_plus/multilingual_sample_text_catalog.json`

## 運用

- `pull_request` では `build-pages-demo` を実行し、Pages demo の build と local smoke を catalog の全言語で確認する定義です。
- `main` への push では Pages deploy と public URL smoke を catalog の全言語で実行する定義です。
- `workflow_dispatch` では current ref に対して手動実行でき、`deploy_pages=true` のときだけ deploy を試行します。
- `pull_request` では本番 Pages を更新せず、preview artifact と local smoke の確認だけを行います。
- scenario 一覧は `node scripts/ci/generate-pages-manifest.mjs --catalog tests/fixtures/multilingual_sample_text_catalog.json --list-language-codes` で確認できます。
- catalog の正本は `tests/fixtures/multilingual_sample_text_catalog.json`、runtime descriptor の正本は `addons/piper_plus/model_descriptors/multilingual-test-medium.json` です。

## 既知の制約

- addon 自体の Web export は preview support のままです。
- `execution_provider` は Web では `EP_CPU` 固定です。
- `openjtalk-native` shared library は Web では使えません。
- PWA / service worker ベースの cross-origin isolation workaround は cache の影響を受けやすく、更新反映や stale cache の切り分けが必要です。
- current branch の 6-language scope は実装済みですが、`workflow_dispatch` / `main` deploy での実証が残っています。

## 残作業

- `TKT-020` の `workflow_dispatch` / PR `Build Web` で Japanese browser smoke の実証を残す
- `TKT-021` と `TKT-025` の `workflow_dispatch` / `main` deploy で public URL smoke の実証を残す
- `TKT-024` の Windows packaged addon smoke と合わせて、`TKT-007` で release 文書へ最終 scope と既知制約を吸収する

## 拡張候補

- multilingual parity 拡張
- Web 向け binary size 最適化
- thread build を使った Pages 公開
