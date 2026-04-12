# TKT-020 Web 日本語 browser smoke / CI gate

- 状態: `進行中`
- 主マイルストーン: [M10 Web Japanese Support / Pages Japanese Demo 完成](../milestones.md#m10)
- 関連マイルストーン: [M5 Quality Gate 完成](../milestones.md#m5)
- 関連要求: `FR-10` `NFR-5` `NFR-6`
- 親チケット: [`TKT-018`](./TKT-018-web-japanese-support.md)
- 依存チケット: [`TKT-019`](./TKT-019-web-japanese-dictionary-bootstrap.md)
- 後続チケット: [`TKT-021`](./TKT-021-pages-japanese-demo-public-smoke.md) [`TKT-007`](./TKT-007-release-finalization.md)

## 進捗

- [x] `test/project` に日本語 smoke scenario を追加する
- [x] local 再現 script に日本語 path を追加する
- [x] CI の Web job で Japanese scenario を gate にする
- [x] failure 時の log / artifact 採取を日本語 path でも揃える
- [x] `TKT-021` が再利用できる判定条件を handoff する
- [ ] `workflow_dispatch` と PR `Build Web` で `ja` scenario pass を実証する

## タスク目的とゴール

- Web 日本語対応を CI と local の両方で落とせる quality gate にする。
- ゴールは、browser 上で日本語 text input と synthesize の成否を deterministic に判定でき、English scenario と同じ entrypoint で再現できること。

## 実装する内容の詳細

- `test/project` の smoke fixture に日本語 sample、dictionary 前提、期待ログを追加する。
- `TKT-019` の handoff として、canonical model は `multilingual-test-medium`、sample text は `こんにちは`、dictionary 欠落時の canonical error code は `ERR_OPENJTALK_DICTIONARY_NOT_READY` とする。
- `scripts/ci/export-web-smoke.sh`、`scripts/ci/run-web-smoke.mjs`、`scripts/ci/web-smoke-server.mjs` で日本語 scenario を選択・判定できるようにする。
- workflow の `Build Web` または同等 job に、日本語 scenario の pass / fail を release gate として組み込む。
- failure 時に browser console、runtime error、asset manifest のどこを見るかを固定し、CI artifact へ保存する。
- 既存 English smoke を温存しつつ、最低でも `en` と `ja` の 2 系統を回す。
- canonical browser smoke contract:
  - scenario `en`: `test_piper_tts.test_initialize_with_model`, `test_piper_tts.test_synthesize_basic` が pass
  - scenario `ja`: `test_piper_tts.test_japanese_dictionary_error_surface`, `test_piper_tts.test_japanese_request_time_dictionary_error_surface`, `test_piper_tts.test_japanese_text_input_with_dictionary` が pass
  - machine-readable console summary: `WEB_SMOKE summary=<json>`
  - failure diagnostics: `web-smoke-report-*.json` と `web-smoke-report-*.png` を export artifact に残す

## 実装するために必要なエージェントチームの役割と人数

- `QA / CI engineer` x1: workflow と pass 条件を担当する。
- `browser automation engineer` x1: smoke script と log 採取を担当する。
- `fixture engineer` x1: `test/project` の smoke fixture を担当する。
- `review engineer` x1: flaky さと gate 妥当性を確認する。

## 提供範囲

- Japanese browser smoke と CI gate。
- local 再現手順。
- failure 時のログ採取と原因切り分け導線。

## テスト項目

- Japanese scenario が CI と local で同じ判定を使えること。
- dictionary 欠落と synthesize 成功を区別して検出できること。
- English scenario の既存 smoke が壊れていないこと。
- README の検証手順が実際の script と一致していること。

## 実装する unit テスト

- smoke helper の scenario selection と pass / fail 判定を確認する script-level test を追加する。
- 必須 asset と sample text の組み合わせを確認する軽量 validator を追加する。

## 実装する e2e テスト

- Godot Web export から static server、headless browser 実行までを通した Japanese browser smoke を CI で実行する。
- local でも同じ script で `ja` scenario を再現し、CI と同じ判定で通ることを確認する。

## 実装に関する懸念事項

- 日本語 path は英語より初期化時間と phonemization 時間が長く、timeout 設計が甘いと flaky になる。
- browser 上の IME 入力そのものは smoke に乗せづらく、最低限 text path と synthesize path を別に確認する必要がある。
- log が不足すると dictionary 不足なのか runtime failure なのか切り分けづらい。

## レビューする項目

- gate が export success ではなく browser 上の日本語 synthesize success を見ているか。
- local と CI の entrypoint が分岐していないか。
- timeout と retry が過剰で flaky failure を隠していないか。

## 一から作り直すとしたらどうするか

- smoke fixture を最初から multi-scenario 前提で作り、`en` と `ja` を同じ runner で切り替える構成にする。
- browser log と runtime summary を JSON 化して、テキスト grep 依存を減らす。

## 後続タスクに連絡する内容

- `TKT-021` へ、Pages public smoke に再利用すべき Japanese pass 条件と log 採取方針を渡す。
- `TKT-007` へ、README と release 文書に載せる local 再現手順と CI gate 条件を渡す。
