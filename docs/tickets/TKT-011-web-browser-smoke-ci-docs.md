# TKT-011 Web browser smoke / CI / 文書反映

- 状態: `要確認`
- 主マイルストーン: [M7 Web Support 完成](../milestones.md#m7)
- 関連マイルストーン: [M4 Packaging / Documentation 完成](../milestones.md#m4) [M5 Quality Gate 完成](../milestones.md#m5) [M8 Release / Asset Library 準備](../milestones.md#m8)
- 関連要求: `FR-10` `FR-9` `NFR-5` `NFR-6`
- 親チケット: [`TKT-002`](TKT-002-web-platform.md)
- 依存チケット: [`TKT-009`](TKT-009-web-manifest-package-export-preset.md) [`TKT-010`](TKT-010-web-runtime-ort-adaptation.md)
- 後続チケット: [`TKT-007`](TKT-007-release-finalization.md)

## 進捗

- [x] Web export と browser smoke を CI job と local 再現 script に落とす
- [x] COOP / COEP 前提の static server 条件を固定する
- [x] smoke の pass / fail 判定とログ採取方法を addon load と最小モデル synthesize まで含めて固定する
- [x] `test/project` と packaged addon を使う fixture 手順を既存 test 基盤へ接続する
- [x] README と addon README に Web preview の前提と制約を反映する
- [ ] CI runner と local emsdk 環境で、重い Web build / export / browser smoke の初回結果を確定する

## タスク目的とゴール

- Web preview support の最終 gate として browser smoke、CI、利用者向け文書を閉じ、Web の可否を CI と local の両方で再現可能にする。
- ゴールは、Web export 後に browser 上で addon load と最小モデル synthesize の成否を CI で判定でき、同じ script を local でも再実行でき、README でも前提条件と制約が一致していること。

## 実装メモ

- `build-web` job、`scripts/ci/export-web-smoke.sh`、`scripts/ci/web-smoke-server.mjs`、`scripts/ci/run-web-smoke.mjs`、`test/project` の `web_smoke` fixture は追加済みです。
- 現時点で未完了なのは、重い Web build/export/browser smoke を CI runner と local emsdk 環境で実行して初回結果を確定することです。

## 実装する内容の詳細

- `.github/workflows/build.yml` に Web build、export、browser smoke の job を追加する。
- `TKT-008` の custom template artifact と `TKT-009` の export preset を CI が同じ名前と配置で消費するようにする。
- `test/project` と `test/prepare-assets.sh` を使い、既存 fixture project と packaged addon の両方で Web smoke を再現できるようにする。
- COOP / COEP を付けた最小 static server と headless browser 実行手順を script 化し、CI と local で同じ entrypoint を使う。
- browser から console log または DOM を使って addon load / 最小モデル synthesize 成功を判定する smoke を追加する。
- failure 時に取得すべき log、artifact、browser console の保存方針を定義する。
- `README.md` と `addons/piper_plus/README.md` に custom template 前提、`CPU-only`、unsupported 項目、local 検証手順を反映する。

## 実装するために必要なエージェントチームの役割と人数

- `CI / QA engineer` x1: workflow、server、headless browser 実行を担当する。
- `browser automation engineer` x1: smoke 判定と log 採取を担当する。
- `docs engineer` x1: README と addon README の更新を担当する。
- `review engineer` x1: preview support の表現と再現性を確認する。

## 提供範囲

- Web browser smoke と CI workflow。
- local 再現用の server / smoke 手順と fixture project 導線。
- Web preview の利用条件と制約を記した README。

## テスト項目

- Web export artifact を COOP / COEP 付き server で起動できること。
- `test/project` と packaged addon のどちらでも同じ pass 条件を使えること。
- headless browser から addon load と最小モデル synthesize 成否を CI と local で同じ判定基準で判定できること。
- 失敗時に console log と CI artifact から原因を追えること。
- README の前提条件が実際の smoke 条件と一致していること。

## 実装する unit テスト

- smoke script の pass / fail 判定ロジックを確認する script-level test を追加する。
- validator や workflow helper に、Web smoke の必須 artifact、template path、COOP / COEP header 条件を確認する軽量チェックを追加する。

## 実装する e2e テスト

- Godot Web export から static server、headless browser 実行までを通した browser smoke を CI で実行する。
- `test/project` と packaged addon の両方で、CI と同じ script / 判定条件で local 再現手順が通ることを確認する。

## 実装に関する懸念事項

- browser smoke は CI 環境差や起動タイミングで flaky になりやすい。
- COOP / COEP や secure context 条件を満たさないと addon 以前に失敗し、切り分けが難しくなる。
- audio policy や初回ロード時間のばらつきで false negative が発生する可能性がある。

## レビューする項目

- smoke test が単なる export 成功ではなく browser 上の addon load と最小モデル synthesize を見ているか。
- README と addon README が preview support の制約を過不足なく説明しているか。
- CI cost と log 量が過剰になっていないか。

## 一から作り直すとしたらどうするか

- browser smoke 用の最小 fixture project と判定 UI を最初から用意し、export / runtime / docs を同じ fixture で閉じる。
- local と CI が同じ server / browser wrapper を使う構成にして、検証条件の分岐を減らす。

## 後続のタスクに連絡する内容

- `TKT-002` へ、Web preview support の最終 pass 条件、既知制約、残る Phase 2 項目を返す。
- `TKT-007` へ、README と release note に載せるべき Web の前提、unsupported 項目、再現手順を渡す。
- `M5` と `M8` へ、browser smoke のログ取得方法と release gate の扱いを引き継ぐ。
