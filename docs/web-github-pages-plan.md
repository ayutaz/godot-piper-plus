# Web GitHub Pages 公開メモ

更新日: 2026-04-12

関連文書:

- [README.md](../README.md)
- [マイルストーン管理](./milestones.md)
- [チケット一覧](./tickets/README.md)
- [M9 個別チケット一覧](./tickets/m9-github-pages/README.md)
- [Web Platform 技術調査](./web-platform-research.md)
- [TKT-012 Web GitHub Pages deploy / public demo](./tickets/TKT-012-web-github-pages-deploy.md)

## 現状

- release gate 上の Web preview support は完了しています。`M7 Web Support` は `preview support`、`CPU-only`、custom Web export template、browser smoke、README 反映までを受け入れ条件として閉じています。
- GitHub Pages 対応は release gate 外の follow-up として、[`M9 GitHub Pages Public Demo / Deploy`](./milestones.md#m9) で追跡しています。
- 2026-04-10 の GitHub Actions run `24223195868` で `Build Web` と browser smoke が通っており、`threads` / `no-threads` の両方で `WEB_SMOKE status=pass` を確認済みです。
- 2026-04-11 の GitHub Actions run `24282051911` では Pages demo の build、deploy、public URL smoke が成功しています。
- 公開デモは [https://ayutaz.github.io/godot-piper-plus/](https://ayutaz.github.io/godot-piper-plus/) で公開中です。
- 現在の runtime contract は Web 向け `EP_CPU` 固定で、Pages demo は英語最小構成に絞っています。

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

## 既知の制約

- addon 自体の Web export は preview support のままです。
- `execution_provider` は Web では `EP_CPU` 固定です。
- `openjtalk-native` shared library は Web では使えません。
- PWA / service worker ベースの cross-origin isolation workaround は cache の影響を受けやすく、更新反映や stale cache の切り分けが必要です。

## 今後の拡張候補

- Japanese text input の dictionary bootstrap
- `naist-jdic` を使う公開デモ
- multilingual parity 拡張
- Web 向け binary size 最適化
- thread build を使った Pages 公開
