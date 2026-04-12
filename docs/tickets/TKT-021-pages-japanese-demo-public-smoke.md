# TKT-021 GitHub Pages 日本語 demo / public smoke

- 状態: `未着手`
- 主マイルストーン: [M10 Web Japanese Support / Pages Japanese Demo 完成](../milestones.md#m10)
- 関連マイルストーン: [M4 Packaging / Documentation 完成](../milestones.md#m4) [M8 Release / Asset Library 準備](../milestones.md#m8)
- 関連要求: `FR-3` `FR-4` `FR-10` `NFR-2` `NFR-6`
- 親チケット: [`TKT-018`](./TKT-018-web-japanese-support.md)
- 依存チケット: [`TKT-019`](./TKT-019-web-japanese-dictionary-bootstrap.md) [`TKT-020`](./TKT-020-web-japanese-browser-smoke-ci.md)
- 後続チケット: [`TKT-007`](./TKT-007-release-finalization.md)

## 進捗

- [ ] Pages demo の UI に日本語入力導線を追加する
- [ ] Pages artifact に日本語用 dictionary / model staging を追加する
- [ ] deploy 後 public URL smoke を日本語シナリオまで広げる
- [ ] README と運用メモを English-only 表記から更新する
- [ ] `TKT-007` へ public scope と既知制約を handoff する

## タスク目的とゴール

- GitHub Pages 公開デモで、日本語入力から実際に合成できる状態にする。
- ゴールは、公開 URL で日本語 text input と synthesize を確認でき、PR build、main deploy、public smoke の一連がそれを保証すること。

## 実装する内容の詳細

- `pages_demo` を English-only UI から、日本語入力を含む公開 demo UI へ拡張する。必要なら language selector、sample text、status 表示を追加する。
- `TKT-019` の handoff を受け、初期の正本は `multilingual-test-medium` + staged `naist-jdic` + sample text `こんにちは` とする。別の日本語 model へ切り替える場合は catalog hash と staging contract を先に固定する。
- `scripts/ci/prepare-pages-demo-assets.sh` と `scripts/ci/export-pages-demo.sh` を更新し、日本語用 dictionary と必要 model を Pages artifact へ stage する。
- `.github/workflows/pages.yml` で PR build、main deploy、public URL smoke が日本語 scenario を確認するようにする。
- `scripts/ci/run-pages-demo-smoke.mjs` で公開 URL 上の日本語 synthesize 成功を判定できるようにする。
- `docs/web-github-pages-plan.md`、`README.md`、addon README の scope 表記を実装に合わせて更新する。

## 実装するために必要なエージェントチームの役割と人数

- `demo engineer` x1: `pages_demo` UI と runtime 接続を担当する。
- `asset pipeline engineer` x1: Pages artifact staging を担当する。
- `CI / deploy engineer` x1: Pages workflow と public smoke を担当する。
- `docs engineer` x1: 公開 scope と運用メモの更新を担当する。
- `review engineer` x1: 公開内容と cache / service worker リスクを確認する。

## 提供範囲

- GitHub Pages 日本語 demo。
- deploy 後の public URL smoke。
- 公開 scope と既知制約を反映した README / 運用メモ。

## テスト項目

- PR build で Japanese Pages artifact を生成できること。
- main deploy 後の公開 URL で日本語 synthesize が成立すること。
- public smoke が service worker cache の影響を受けても原因を追えること。
- English path を壊さず、日本語 path を追加できていること。

## 実装する unit テスト

- Pages artifact validator に、日本語用必須 asset と UI contract を確認する軽量チェックを追加する。
- smoke helper の Japanese scenario selection と pass / fail 判定を確認する script-level test を追加する。

## 実装する e2e テスト

- Pages demo の local smoke を日本語 scenario で実行する。
- `pages.yml` の main deploy 後に public URL smoke を実行し、日本語 synthesize 成功を確認する。

## 実装に関する懸念事項

- GitHub Pages は cache と service worker の影響が大きく、更新直後の smoke が stale asset を掴む可能性がある。
- 日本語 IME 入力のブラウザ挙動と synthesize backend は別レイヤなので、UI の文字入力確認だけでは不十分。
- artifact サイズが増えすぎると deploy 時間や初回ロードに影響する。

## レビューする項目

- 公開 scope が実装と文書で一致しているか。
- public smoke が公開 URL 上の日本語 synthesize を実際に見ているか。
- cache / service worker の運用注意が利用者向けに十分説明されているか。

## 一から作り直すとしたらどうするか

- Pages demo を最初から locale-aware UI と sample set を持つ形で作り、English-only 前提を避ける。
- deploy artifact に build metadata と asset hash manifest を必ず載せ、public smoke と cache 切り分けに使う。

## 後続タスクに連絡する内容

- `TKT-007` へ、公開 URL、必須 asset、既知制約、local / public smoke 手順を引き継ぐ。
- 次の Web 拡張タスクへ、Japanese path を閉じた後に multilingual parity や binary size 最適化へ進むことを渡す。
