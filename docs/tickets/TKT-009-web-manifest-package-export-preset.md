# TKT-009 Web manifest / package / export preset 整備

- 状態: `完了`
- 主マイルストーン: [M7 Web Support 完成](../milestones.md#m7)
- 関連マイルストーン: [M4 Packaging / Documentation 完成](../milestones.md#m4) [M5 Quality Gate 完成](../milestones.md#m5)
- 関連要求: `FR-10` `FR-9` `NFR-5` `NFR-6`
- 親チケット: [`TKT-002`](TKT-002-web-platform.md)
- 依存チケット: [`TKT-008`](TKT-008-web-template-toolchain-bootstrap.md)
- 後続チケット: [`TKT-010`](TKT-010-web-runtime-ort-adaptation.md) [`TKT-011`](TKT-011-web-browser-smoke-ci-docs.md)

## 進捗

- [x] `web.*` 向け `.gdextension` entry を addon 本体と test project の両方へ追加する
- [x] package script と validator に Web side module 成果物の扱いを追加する
- [x] test project の Web export preset を追加し、custom template 前提を反映する
- [x] `TKT-008` で固定した thread / no-thread artifact matrix を package / manifest / export preset へ反映する
- [x] Web artifact を追加しても model / `naist-jdic` / `openjtalk-native` を package に含めない境界を固定する
- [x] `TKT-011` の Web export / browser smoke 結果で、manifest / package / export preset 契約が実際に成立していることを最終確認する

## タスク目的とゴール

- Web export に必要な manifest、package、validator、test project preset を揃え、後続の runtime 実装と browser smoke が同じ artifact 契約を前提に進められる状態にする。
- ゴールは、`web.*` entry と package 実ファイル、validator 条件、test project の export preset が矛盾なく揃っていること。

## 実装メモ

- 2026-04-10 の GitHub Actions run `24223195868` で package 生成、validator、Web export、browser smoke が通り、manifest / package / export preset 契約が CI 上で成立していることを確認しました。
- `FR-9` の境界は維持されており、Web 対応後も model、`naist-jdic`、`openjtalk-native` 本体は package に含めていません。

## 実装する内容の詳細

- `addons/piper_plus/piper_plus.gdextension` に、`TKT-008` で固定した採用セットに従って `web.debug.wasm32` / `web.release.wasm32` と threads/no-thread entry を追加する。
- `test/project/addons/piper_plus/piper_plus.gdextension` を addon 本体と同期する。
- `scripts/ci/package-addon.sh` に Web side module artifacts を package 対象として追加する。
- `scripts/ci/validate-addon-package.sh` に Web manifest entry と required binary の整合検証を追加する。
- `test/project/export_presets.cfg` に Web preset を追加し、custom template 前提、preset 名、template path key を固定する。
- `test/prepare-assets.sh` で Web manifest / export preset を使う fixture project と packaged addon の契約が崩れないようにする。
- `FR-9` に従い、Web 対応後も model file、`naist-jdic`、`openjtalk-native` 本体は package 対象外のまま維持し、その境界を validator 観点と文書へ反映する。

## 実装するために必要なエージェントチームの役割と人数

- `package engineer` x1: manifest と package assembly の整合を担当する。
- `build integration engineer` x1: export preset と script 接続を担当する。
- `QA / docs engineer` x1: validator 条件と利用前提の説明を担当する。

## 提供範囲

- Web 向け `.gdextension` manifest。
- package assembly と validator の Web 対応。
- test project の Web export preset と fixture 側契約。

## テスト項目

- `web.*` manifest entry が package 実ファイルと一致すること。
- validator が Web binary 欠落や命名不一致を検出できること。
- test project の Web export preset が `TKT-008` の custom template artifact 配置を前提に成立すること。
- package に model、`naist-jdic`、`openjtalk-native` が混入しないこと。

## 実装する unit テスト

- validator に Web manifest entry の required binary 検証を追加する。
- package script の assemble 結果を確認する script-level check を追加し、Web artifact の欠落や threads/no-thread matrix 不整合を failure にする。

## 実装する e2e テスト

- packaged addon を使った最小 Web export を 1 回通し、manifest 不整合で止まらないことを確認する。
- `test/project` と `test/prepare-assets.sh` を使った Web export smoke を実行し、想定した artifact と preset 条件が揃うことを確認する。

## 実装に関する懸念事項

- `web.*` entry 名と実際の artifact 名が少しでもずれると load 以前に export が壊れる。
- thread / no-thread の 2 系統を持つ場合、package と validator の条件が複雑化する。
- custom template 前提を package に混ぜ込みすぎると、利用者向け前提と配布物の境界が曖昧になる。

## レビューする項目

- addon 本体 manifest と test project manifest が同期しているか。
- package script と validator が同じ artifact 契約を見ているか。
- export preset が preview support の前提を正しく表現しているか。
- Web 対応後も `FR-9` の package 境界が崩れていないか。

## 一から作り直すとしたらどうするか

- manifest、package、validator、export preset の正本を 1 つの metadata file に寄せ、各ファイルを生成する構成にする。
- platform ごとの required artifact を code 化して、手作業のファイル名同期を減らす。

## 後続のタスクに連絡する内容

- `TKT-010` へ、確定した Web artifact 名、manifest entry、package path を渡す。
- `TKT-011` へ、browser smoke と CI が前提にすべき export preset 名、template path key、validator 条件、package 成果物構成、package 対象外の境界を渡す。
- `TKT-002` へ、`W2` の完了条件と package 契約の更新内容を返す。
