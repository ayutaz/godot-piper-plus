# TKT-005 Windows Local Android Export Error 切り分け

- 状態: `進行中`
- 主マイルストーン: [M6 Platform Verification 完成](../milestones.md#m6)
- 関連マイルストーン: [M5 Quality Gate 完成](../milestones.md#m5)
- 関連要求: Android local reproducibility
- 依存: なし
- 後続チケット: [TKT-004](./TKT-004-android-export-runtime.md), [TKT-007](./TKT-007-release-finalization.md)

## 進捗

- [ ] verbose log 付きの再現手順を固定する
- [ ] SDK / Java / template / keystore の差分を分類する
- [ ] script で吸収できる問題を修正する
- [ ] 既知制約または恒久対策を文書化する

## タスク目的とゴール

- Windows + Godot 4.6 環境で出ている generic configuration error の原因を切り分ける。
- ゴールは、CI 専用問題か local 設定問題か script 問題かを判定し、恒久対応または既知制約として固定すること。

## 実装する内容の詳細

- Windows local の export 条件を洗い出す。
- `export_presets.cfg`、editor settings、SDK / Java / keystore、template 導入状態の差分を比較する。
- generic error を具体メッセージへ落とすため、ログ採取と再現手順を整理する。
- 修正できる場合は script へ吸収し、できない場合は既知制約として文書化する。

## エージェントチームの役割と人数

| 役割 | 人数 | 主責務 |
|---|---:|---|
| Windows 調査担当 | 1 | local 環境差分の採取 |
| Android export 担当 | 1 | export 条件と preset の見直し |
| ログ解析担当 | 1 | Godot / JDK / SDK ログから原因特定 |
| 文書担当 | 1 | 再現手順と回避策の記録 |

## 提供範囲

- generic configuration error の原因分類。
- 回避策または修正。
- `TKT-004` へ流せる Windows local 手順。
- 対象ファイル: `scripts/ci/export-android-smoke.ps1`, `test/project/export_presets.cfg`, Windows local の export 前提メモ

## テスト項目

- local Windows で同じ手順を踏んだときの再現性。
- 修正後に error が解消するか、または制約が説明可能になるか。
- CI には影響しないこと。

## 実装する unit テスト

- `export-android-smoke.ps1` に SDK / Java / export templates / keystore の preflight check を追加し、generic error 前に失敗理由を出せるようにする。
- Android preset の前提が崩れていないかを確認する script-level assertion を追加する。

## 実装する e2e テスト

- Windows local での headless Android export 再実行。
- verbose log 付きの export 実行とログ比較。

## 実装に関する懸念事項

- generic error が Godot 本体や export template 側の挙動で、addon 側だけでは直せない可能性がある。
- local 固有 path と editor global settings の影響が強い。

## レビューする項目

- 再現手順が第三者でも追えるか。
- script で吸収すべき問題と、文書に残すべき制約を切り分けられているか。
- `TKT-004` の CI 判定と混線していないか。

## 一から作り直すとしたらどうするか

- Windows local 用にも最初から self-contained な Android export bootstrap script を用意し、global settings を直接触らない形へ寄せる。

## 後続タスクに連絡する内容

- `TKT-004` へ、Windows local で必要な前提条件、回避策、再現不可条件を共有する。
- `TKT-007` へ、local only の既知制約を README に載せる必要があるか判断材料を渡す。
