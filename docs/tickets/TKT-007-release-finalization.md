# TKT-007 Release Package / 文書最終化

- 状態: `進行中`
- 主マイルストーン: [M8 Release / Asset Library 準備](../milestones.md#m8)
- 関連マイルストーン: [M4 Packaging / Documentation 完成](../milestones.md#m4)
- 関連要求: `FR-9` `NFR-6` release 完了条件
- 依存チケット: `TKT-001` `TKT-002` `TKT-003` `TKT-004` `TKT-006` `TKT-011`
- 後続チケット: なし

## 進捗

- [ ] 依存チケットの結果を集約する
- [ ] package / README / changelog / notice を最終状態へ更新する
- [ ] Asset Library 向け説明文を確定する
- [ ] final package と文書整合を確認する

## タスク目的とゴール

- 実装・検証結果を release package と公開文書へ反映し、Asset Library 申請可能な状態にする。
- ゴールは、配布物、README、license、changelog、公開説明が最終実装と矛盾しないこと。

## 実装する内容の詳細

- `README.md`、addon README、`CHANGELOG.md`、license / third-party notice を最終状態へ更新する。
- multilingual と Web のスコープ、platform の確定結果、既知制約を反映する。
- `TKT-011` が確定した browser smoke 条件、custom template 前提、Web preview 制約を release 文書へ反映する。
- package 生成手順と validator 条件を最終確認する。
- Asset Library 向け説明文、同梱範囲、注意事項を固定する。

## エージェントチームの役割と人数

| 役割 | 人数 | 主責務 |
|---|---:|---|
| release manager | 1 | 依存チケットの結果集約と最終判定 |
| package 担当 | 1 | package 内容、validator、配布境界の確認 |
| 文書担当 | 2 | README、addon README、changelog、公開説明の更新 |
| レビュー担当 | 1 | 配布整合と公開可否のレビュー |

## 提供範囲

- 最終 package と配布境界の整理。
- 文書一式の整合。
- Asset Library 申請に必要な説明情報。

## テスト項目

- package validator が最終 package に対して通ること。
- README と addon README が最終スコープと矛盾しないこと。
- Web preview の前提が `TKT-011` の smoke 条件と一致していること。
- サポート platform、未対応項目、既知制約が明記されていること。

## 実装する unit テスト

- package validator に不足 binary / dependency / partial package の failure case が維持されていることを確認する。
- 必要なら support matrix と package metadata の整合を確認する script-level check を追加する。

## 実装する e2e テスト

- package 組み立てから validator、代表 smoke の最終通し確認。
- Asset Library 提出物相当の内容チェック。

## 実装に関する懸念事項

- platform 結果が未確定のまま文書を閉じると、公開情報がすぐ陳腐化する。
- multilingual と Web の扱いを正式対応と計画中のどちらで書くかを誤ると齟齬が出る。

## レビューする項目

- 配布物と README の境界が一致しているか。
- 既知制約と未対応項目が隠れていないか。
- change log が利用者視点で読める形になっているか。
- Asset Library 向け記述が過大表現になっていないか。

## 一から作り直すとしたらどうするか

- package / validator / README を同じ metadata ソースから生成する形へ寄せ、手作業同期を減らす。
- release note と support matrix を CI 成果から半自動生成する。

## 後続タスクに連絡する内容

- 次回 release cycle へ、support matrix、既知制約、未完要求の扱いを引き継ぐ。
- Asset Library 公開後に必要な FAQ が見えたら `docs/` に別紙化する。
