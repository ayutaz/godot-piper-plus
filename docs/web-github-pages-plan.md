# Web GitHub Pages 公開メモ

更新日: 2026-04-12

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
- 現在の runtime contract は Web 向け `EP_CPU` 固定で、Pages demo は英語最小構成に絞っています。
- 日本語 text input / synthesize と Pages 日本語 demo は must follow-up として [`M10 Web Japanese Support / Pages Japanese Demo 完成`](./milestones.md#m10) と [`TKT-018`](./tickets/TKT-018-web-japanese-support.md) から [`TKT-021`](./tickets/TKT-021-pages-japanese-demo-public-smoke.md) で管理します。
- M9 向けに一時的に作成した GitHub Pages ticket 群の内容は、完了時にこの文書と [`docs/milestones.md`](./milestones.md) へ吸収しています。

## 公開スコープ

GitHub Pages 公開デモは次の範囲に固定しています。

- `no-threads`
- `CPU-only`
- English minimal demo
- `index.html` export
- `multilingual-test-medium` 1 model 同梱
- runtime download なし
- PWA と cross-origin isolation workaround を有効化

## 現在の実装

- public demo project: [`pages_demo`](../pages_demo)
- project staging: [`scripts/ci/prepare-pages-demo-assets.sh`](../scripts/ci/prepare-pages-demo-assets.sh)
- export: [`scripts/ci/export-pages-demo.sh`](../scripts/ci/export-pages-demo.sh)
- local / public smoke: [`scripts/ci/run-pages-demo-smoke.mjs`](../scripts/ci/run-pages-demo-smoke.mjs)
- workflow: [`.github/workflows/pages.yml`](../.github/workflows/pages.yml)
- artifact contract: `public-demo-manifest.json` と `build-meta.json`

## 運用

- `pull_request` では `build-pages-demo` を実行し、Pages demo の build と local smoke を確認します。
- `main` への push では Pages deploy と public URL smoke を実行します。
- `workflow_dispatch` では current ref に対して手動実行でき、`deploy_pages=true` のときだけ deploy を試行します。
- `pull_request` では本番 Pages を更新せず、preview artifact と local smoke の確認だけを行います。

## 既知の制約

- addon 自体の Web export は preview support のままです。
- `execution_provider` は Web では `EP_CPU` 固定です。
- `openjtalk-native` shared library は Web では使えません。
- PWA / service worker ベースの cross-origin isolation workaround は cache の影響を受けやすく、更新反映や stale cache の切り分けが必要です。

## 必須の後続作業

- Japanese text input の dictionary bootstrap
- `naist-jdic` を使う公開デモ
- Japanese scenario を含む local / CI / public smoke
- README と release 文書の日本語 Web scope 同期

## 拡張候補

- multilingual parity 拡張
- Web 向け binary size 最適化
- thread build を使った Pages 公開
