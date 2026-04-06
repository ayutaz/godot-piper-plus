# TKT-002 Web Platform 対応

- 状態: `未着手`
- 主マイルストーン: [M7 Web Support 完成](../milestones.md#m7)
- 関連マイルストーン: [M4 Packaging / Documentation 完成](../milestones.md#m4) [M5 Quality Gate 完成](../milestones.md#m5) [M8 Release / Asset Library 準備](../milestones.md#m8)
- 関連要求: `FR-10` `FR-9` `NFR-5` `NFR-6`
- 依存: なし
- 後続チケット: [TKT-007](./TKT-007-release-finalization.md)

## 進捗

- [ ] Web backend と制約の feasibility を確定する
- [ ] `web.*` manifest / package / build 導線を実装する
- [ ] browser smoke 手順を自動化する
- [ ] README と package 文書へ制約を反映する

## タスク目的とゴール

- Web export 向けの build / package / export 導線を整え、browser 上で addon のロード可否を確認できる状態にする。
- ゴールは、Web で何が動くか、何が制約か、どこまで release 対象に含めるかを実装と文書で固定すること。

## 実装する内容の詳細

- `web.*` 向け GDExtension entry と manifest 反映要件を定義する。
- Web で使える inference backend と不可機能を切り分ける。
- package script と validator に Web binary / sidecar の扱いを追加する。
- Web export と browser smoke の実行手順を CI または再現可能な script に落とす。
- README と addon README に Web 制約を明記する。

## エージェントチームの役割と人数

| 役割 | 人数 | 主責務 |
|---|---:|---|
| 技術調査担当 | 1 | Godot Web GDExtension と ONNX Runtime Web 可否の調査 |
| Build 実装担当 | 2 | manifest、build script、package 反映の実装 |
| 検証担当 | 1 | export / browser smoke /制約確認 |
| 文書担当 | 1 | 利用条件、対象外、導入手順の明文化 |
| レビュー担当 | 1 | release 対象としての妥当性確認 |

## 提供範囲

- Web export に必要な addon packaging と manifest 整備。
- browser load を基準にした smoke test。
- runtime 制約の明文化。

## テスト項目

- Web export が manifest 不整合で失敗しないこと。
- browser 上で addon load 成否を判定できること。
- 未対応 backend を選んだ場合の失敗が説明可能であること。
- package validator が Web 成果物を見落とさないこと。

## 実装する unit テスト

- package / validator の Web manifest 解釈テスト。
- Web 向け binary 名、entry 名、依存関係の検証テスト。
- 未対応 provider を弾く設定検証テスト。

## 実装する e2e テスト

- Godot Web export の smoke test。
- browser load 成否を確認する headless 実行または同等の script。
- packaged addon を使った最小 Web project の起動確認。

## 実装に関する懸念事項

- GDExtension と ONNX Runtime の Web 対応境界が不明確な可能性がある。
- 実行可能でも性能要件を満たさない可能性がある。
- browser 制約により desktop/mobile と同じ API 保証が難しい可能性がある。

## レビューする項目

- Web を正式サポートに含める範囲が明確か。
- unsupported 機能が明示され、黙って失敗しないか。
- package / validator / README の 3 者が一致しているか。
- CI で再現可能な検証手順になっているか。

## 一から作り直すとしたらどうするか

- 最初に Web を別 delivery target として切り分け、desktop/mobile と同じ binary/package 前提を持ち込まない。
- runtime 機能も feature flag で段階開放し、load-only、inspect-only、synthesis-enabled を明確に分ける。

## 後続タスクに連絡する内容

- `TKT-007` へ、Web を正式サポートに入れるか制約付き preview にするかの判断を渡す。
- `M5` 向けに Web smoke の pass 条件と失敗時ログ採取方法を共有する。
