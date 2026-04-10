# TKT-006 iOS Export / Link Smoke 確認

- 状態: `完了`
- 主マイルストーン: [M6 Platform Verification 完成](../milestones.md#m6)
- 関連マイルストーン: [M5 Quality Gate 完成](../milestones.md#m5) [M8 Release / Asset Library 準備](../milestones.md#m8)
- 関連要求: iOS arm64 release gate
- 依存: なし
- 後続チケット: [TKT-007](./TKT-007-release-finalization.md)

## 進捗

- [x] iOS export smoke の初回結果を確定する
- [x] Xcode project / link failure を切り分ける
- [x] 必要なら binary / export 条件を修正する
- [x] 結果を文書へ反映する

## タスク目的とゴール

- iOS arm64 の export と Xcode link smoke の成否を確定し、release 判定へ反映できる状態にする。
- ゴールは、Godot export から `xcodebuild` までが再現可能に通るか、失敗するなら原因と制約が説明できること。

## 実装メモ

- 2026-04-10 の GitHub Actions run `24223195868` で `iOS Export Smoke` が `success` になり、Godot export から `xcodebuild` までの smoke を CI 上で確認しました。
- これにより、iOS arm64 は export / link smoke の観点で release gate に反映できる状態です。

## 実装する内容の詳細

- `scripts/ci/export-ios-smoke.sh` と `export_presets.cfg` の前提を確認する。
- export 生成物、Xcode project、link 手順の失敗点を調べる。
- 必要なら iOS binary の package 反映、project settings、link 条件を修正する。
- 結果を README と milestones へ反映する。

## エージェントチームの役割と人数

| 役割 | 人数 | 主責務 |
|---|---:|---|
| iOS export 担当 | 1 | export preset と Xcode project 差分確認 |
| Build 調査担当 | 1 | link error と binary 配置の切り分け |
| CI 検証担当 | 1 | 再実行と結果確定 |
| 文書担当 | 1 | iOS 制約と結果反映 |

## 提供範囲

- iOS export/link smoke の pass / fail 確定。
- 必要時の export / package 修正。
- release 文書への結果反映。

## テスト項目

- `--export-debug iOS` が Xcode project を生成すること。
- `xcodebuild` の Debug build が通ること。
- iOS 向け native library が link 対象として解決されること。

## 実装する unit テスト

- validator の iOS binary / static library 検証を必要に応じて強化する。
- `export-ios-smoke.sh` の preflight check を追加し、Xcode / SDK 未整備時の失敗理由を明示する。

## 実装する e2e テスト

- `scripts/ci/export-ios-smoke.sh` の再実行。
- 生成 `.xcodeproj` に対する `xcodebuild` smoke。

## 実装に関する懸念事項

- Xcode / SDK version 差分で再現性が揺れる可能性がある。
- export 成功後の link で Apple platform 固有の制約が出る可能性がある。

## レビューする項目

- iOS binary と manifest の対応が正しいか。
- export 成功と link 成功を混同していないか。
- 制約がある場合、release 判定と README の書き分けができているか。

## 一から作り直すとしたらどうするか

- iOS smoke は export と `xcodebuild` を最初から別チェックに分け、どちらが失敗したかを即座に判別できるようにする。

## 後続タスクに連絡する内容

- `TKT-007` へ、iOS の最終判定、必要 SDK/Xcode 条件、既知制約を渡す。
- package 反映が必要なら `M4` に差分を返す。
