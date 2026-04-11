# Web GitHub Pages 公開メモ

更新日: 2026-04-11

関連文書:

- [README.md](../README.md)
- [マイルストーン管理](./milestones.md)
- [チケット一覧](./tickets/README.md)
- [Web Platform 技術調査](./web-platform-research.md)
- [TKT-011 Web browser smoke / CI / 文書反映](./tickets/TKT-011-web-browser-smoke-ci-docs.md)
- [TKT-012 Web GitHub Pages deploy / public demo](./tickets/TKT-012-web-github-pages-deploy.md)

## 現状

- release gate 上の Web Phase 1 は完了です。`M7 Web Support` は `preview support`、`CPU-only`、custom Web export template、browser smoke、README 反映までを受け入れ条件として閉じています。
- 2026-04-10 の GitHub Actions run `24223195868` で `Build Web` と browser smoke が通っており、`threads` / `no-threads` の両方で `WEB_SMOKE status=pass` を確認済みです。
- 現在の runtime contract は Web 向け `EP_CPU` 固定で、最小 synthesize は英語 text input または phoneme string を中心に成立しています。

## GitHub Pages 化で追加で必要なこと

- 現在の workflow は Web export と browser smoke までで、成功時 artifact を GitHub Pages 用に upload / deploy していません。
- 現在の Web preset は `progressive_web_app/enabled=false`、`progressive_web_app/ensure_cross_origin_isolation_headers=false` です。
- CI の browser smoke は `scripts/ci/web-smoke-server.mjs` が `COOP` / `COEP` / `CORP` header を付ける前提で成立しており、その tested environment を GitHub Pages にそのまま持ち込めません。
- 現在の export target は `test/project` の smoke fixture で、出力も `piper-plus-tests.html` 前提です。公開ページ向けの `index.html`、導線、見た目、説明は未整備です。
- `demo/` は `CSS10` モデルと `naist-jdic` を前提にしており、preview support の最小公開デモとしては重すぎます。
- addon package 方針では model file を同梱しないため、GitHub Pages 公開物だけ別扱いでどのモデルをどう載せるかを決める必要があります。

## 推奨スコープ

初回の GitHub Pages 対応は、release gate を広げずに次の範囲へ限定します。

- `no-threads`
- `CPU-only`
- English minimal demo
- `index.html` export
- PWA と cross-origin isolation workaround を有効化した Pages 向け preset
- GitHub Actions からの Pages deploy
- deploy 後の公開 URL に対する smoke

## 初回スコープに含めないもの

- Japanese text input の dictionary bootstrap
- `naist-jdic` を使う公開デモ
- multilingual parity 拡張
- Web 向け binary size 最適化
- thread build を使った Pages 公開

これらは `preview support` の release gate を再オープンせず、別の拡張要求として扱います。

## 実装方針

- Pages 専用の Web preset を追加し、まずは `variant/thread_support=false` の `no-threads` を正本にする
- `progressive_web_app/enabled=true` と `progressive_web_app/ensure_cross_origin_isolation_headers=true` を使い、header を直接制御できない hosting でも動く形を優先する
- 公開対象は `test/project` ベースの最小 English demo か、新規の minimal public demo のどちらかに絞る
- export 生成物は `index.html` を入口に揃える
- workflow には `actions/configure-pages`、`actions/upload-pages-artifact`、`actions/deploy-pages` を追加する
- deploy 後に `page_url` へアクセスして、addon load と最小 synthesize を確認する smoke を追加する

## 受け入れ条件

- GitHub Actions で Pages 用 Web export が成功する
- 成功時 artifact が GitHub Pages へ deploy される
- 公開 URL で addon load と最小 synthesize が成立する
- 公開デモの scope が `preview support` と矛盾せず、`CPU-only` / English minimal demo であることが文書化される
- `M7 Web Support` の release gate を reopen せず、follow-up task として追跡できる

## リスク

- PWA / service worker ベースの cross-origin isolation workaround は cache の影響を受けやすく、更新反映や stale cache の切り分けが必要です
- 公開用 model のサイズ、ライセンス、配布方法を誤ると package 方針や公開方針と衝突します
- thread build は header 条件の影響を受けやすいため、初回 Pages 対応では避けた方が安全です

