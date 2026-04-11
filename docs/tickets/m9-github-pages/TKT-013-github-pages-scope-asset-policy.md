# TKT-013 GitHub Pages scope / asset policy 固定

- 状態: `進行中`
- フェーズ: `GP0`
- 主マイルストーン: [M9 GitHub Pages Public Demo / Deploy](../../milestones.md#m9)
- 関連マイルストーン: [M7 Web Support 完成](../../milestones.md#m7)
- 親チケット: [TKT-012 Web GitHub Pages deploy / public demo](../TKT-012-web-github-pages-deploy.md)
- 依存チケット: [`TKT-011`](../TKT-011-web-browser-smoke-ci-docs.md)
- 後続チケット: [TKT-014](./TKT-014-github-pages-preset-public-entry.md) [TKT-015](./TKT-015-github-pages-deploy-workflow.md) [TKT-016](./TKT-016-github-pages-public-url-smoke.md) [TKT-017](./TKT-017-github-pages-docs-operational-notes.md)
- 関連メモ: [docs/web-github-pages-plan.md](../../web-github-pages-plan.md) [M9 GitHub Pages 個別チケット](./README.md)

## 進捗

- [x] `M9` の `GP0` を個別チケットとして切り出す
- [ ] public demo の scope を `no-threads` / `CPU-only` / English minimal demo に固定する
- [ ] public demo 用 asset policy を `en_US-ljspeech-medium` 1 モデル同梱、runtime download なしで固定する
- [ ] license / notice / cache versioning / hosting 前提を固定する
- [ ] `GP1` から `GP4` が参照する acceptance criteria を整理する

## タスク目的とゴール

- GitHub Pages 対応で先に固定すべき公開 scope、asset policy、hosting 前提を文書の正本として確定する。
- 後続フェーズが「何を作るか」で揺れない状態を作り、preset、workflow、smoke、README の設計判断を一つの前提へ揃える。
- このフェーズの正本は本チケットとし、[docs/web-github-pages-plan.md](../../web-github-pages-plan.md) は overview を保つ。

## 実装メモ

- 現行の browser smoke は `scripts/ci/web-smoke-server.mjs` による `COOP` / `COEP` / `CORP` header 付与前提で成立しており、GitHub Pages は同じ hosting 条件をそのまま持ち込めません。
- 現行の `test/project/export_presets.cfg` では `progressive_web_app/enabled=false`、`progressive_web_app/ensure_cross_origin_isolation_headers=false` です。
- 初回 public demo で使う英語モデル候補は既存 catalog にある `en_US-ljspeech-medium` で、README 上も最短導入モデルとして扱っています。
- addon package 方針では model file を同梱しないため、public demo 用 asset policy を package policy と分離して定義する必要があります。

## 実装する内容の詳細

- GitHub Pages 公開の初回スコープを `no-threads`、`CPU-only`、English minimal demo、`index.html` export に固定する。
- public demo 用 model は `en_US-ljspeech-medium` 1 種に固定し、Pages artifact へ `.onnx` と `.onnx.json` を同梱する。初回スコープでは runtime download を行わない。
- public demo 用 asset は addon package の配布対象から切り離し、Pages 公開物専用の delivery contract として扱う。
- license / notice は public demo artifact から参照できる位置へ同梱し、README と Pages 用文書から同じ説明へリンクする。
- cache versioning は deploy 単位で更新される version key を前提にし、`GP2` と `GP3` は service worker の stale cache を切り分けられる構成を採用する。
- `PWA` と `ensure_cross_origin_isolation_headers` を使う前提、service worker cache の注意点、hosting 前提を文書化する。
- `TKT-014` から `TKT-017` が参照する受け入れ条件、非対象範囲、依存関係を文書に落とす。
- `M9`、`TKT-012`、個別チケット群の相互リンクと責務分担を確定する。

## 実装するために必要なエージェントチームの役割と人数

- `technical lead` x1: scope と完了条件の正本を決める。
- `web runtime architect` x1: Pages 上の hosting 制約、PWA workaround、runtime contract の妥当性を確認する。
- `release / docs engineer` x1: package 方針と公開方針の境界を整理し、文書へ反映する。
- `QA representative` x1: 後続フェーズで必要な検証観点を先に定義する。

## 提供範囲

- GitHub Pages 対応の scope、非対象範囲、model 配布方針、hosting 前提を定義した文書。
- `M9` と `TKT-012` を基点にした個別チケット構造。
- `GP1` から `GP4` が参照する受け入れ条件の正本。
- Pages artifact へ含める model / config / notice / cache versioning 方針。

## テスト項目

- `M9` と `TKT-012` の scope 記述が `M7` の完了条件と衝突していない。
- GitHub Pages 公開物と addon package の配布方針が混線していない。
- `en_US-ljspeech-medium` 1 モデル同梱、runtime download なし、notice 同梱、cache versioning ありの方針が docs 間で一致している。
- `GP1` 以降が参照する前提が、preset / workflow / smoke / docs の各フェーズで矛盾していない。

## 実装する unit テスト

- 新しい runtime unit test は追加しない。
- 代わりに、後続で必要になる preset / workflow / smoke の検証観点を文書チェックリストとして固定する。

## 実装する e2e テスト

- このフェーズ単体では e2e は追加しない。
- ただし、`TKT-016` で実施する public URL smoke の観点と pass 条件をここで固定する。

## 実装に関する懸念事項

- 公開用 model を artifact 同梱に固定するため、初回 Pages 公開物のサイズが大きくなりやすい。
- `en_US-ljspeech-medium` を採用する以上、将来の軽量モデル差し替え余地を後続で意識する必要がある。
- 公開用 model のサイズ、ライセンス、配布場所を誤ると package 方針と衝突する。
- `threads` を外す理由、Japanese text input を初回スコープから外す理由が曖昧だと、後続チケットで scope がぶれる。
- hosting 前提を曖昧にしたまま `GP1` へ進むと、preset と workflow の設計が何度も揺り戻される。

## レビューする項目

- `M7 Web Support` の release gate を reopen していないか。
- `CPU-only`、`no-threads`、English minimal demo の 3 条件が文書の正本で一致しているか。
- `en_US-ljspeech-medium` 1 モデル同梱、runtime download なし、notice 同梱、cache versioning ありの asset policy が明示されているか。
- model 配布方針が package と GitHub Pages 公開物で明確に分離されているか。
- `GP1` から `GP4` の責務分割が妥当で、依存関係が循環していないか。

## 一から作り直すとしたらどうするか

- release gate 用の Web fixture と public demo 用の deploy target を最初から別マイルストーン・別チケット体系で管理する。
- 公開 scope、asset policy、hosting 制約を 1 つの machine-readable な manifest に寄せ、preset / workflow / docs をそこから生成する。
- model 配布方針とライセンス情報を deployment manifest に含め、README への転記を後段で自動化する。

## 後続のタスクに連絡する内容

- [TKT-014](./TKT-014-github-pages-preset-public-entry.md) には、Pages preset と public entry を `no-threads` / `CPU-only` / English minimal demo 前提で実装するよう引き継ぐ。
- [TKT-015](./TKT-015-github-pages-deploy-workflow.md) には、deploy workflow が `en_US-ljspeech-medium` 同梱、runtime download なし、notice 同梱、cache versioning の契約を壊さないよう引き継ぐ。
- [TKT-016](./TKT-016-github-pages-public-url-smoke.md) には、public URL smoke が fresh browser context と service worker cache 切り分けを前提にするよう引き継ぐ。
- [TKT-017](./TKT-017-github-pages-docs-operational-notes.md) には、最終文書化で `GP0` の scope を過大表現しないことを引き継ぐ。
