# TKT-026 6-language docs / release sync

- 状態: `未着手`
- 主マイルストーン: [M11 Windows / Web 6-Language Text Input / Template UX 完成](../milestones.md#m11)
- 関連マイルストーン: [M4 Packaging / Documentation 完成](../milestones.md#m4) [M8 Release / Asset Library 準備](../milestones.md#m8)
- 関連要求: `FR-3` `FR-10` `FR-11` `NFR-6`
- 親チケット: [`TKT-022`](./TKT-022-windows-web-six-language-support.md)
- 依存チケット: [`TKT-024`](./TKT-024-windows-six-language-runtime-smoke.md) [`TKT-025`](./TKT-025-web-six-language-runtime-demo-smoke.md)
- 後続チケット: [`TKT-007`](./TKT-007-release-finalization.md)

## 進捗

- [ ] support matrix と template text catalog の文書表現を固定する
- [ ] README / addon README / Pages 運用メモへ 6 言語 scope を反映する
- [ ] Windows / Web の required asset と known issue を整理する
- [ ] `TKT-007` へ release 文書化条件を handoff する

## タスク目的とゴール

- Windows / Web 6 言語対応と template text UX の結果を、利用者向け文書と release 文書へ反映する。
- ゴールは、README、addon README、support matrix、Pages 運用メモ、release note が同じ 6 言語 scope を表現すること。

## 実装する内容の詳細

- `README.md` と `addons/piper_plus/README.md` の言語サポート表記を `ja/en` baseline から 6 言語 minimum parity に更新する。
- `docs/generated/multilingual_capability_matrix.md` と template text catalog の投影を更新し、Windows / Web 6 言語 scope と required asset を説明できるようにする。
- `docs/web-github-pages-plan.md` に Pages demo の 6 言語 selector、template text catalog、public smoke の運用メモを反映する。
- Windows test speech UI と Web / Pages demo で使う template text が利用者向け文書でも同じ文になっていることを確認する。
- `TKT-007` が最終 package / changelog / Asset Library 文言を閉じられるよう、support 表記と known issue を handoff する。

## 提供範囲

- 6 言語 support matrix の文書表現。
- template text catalog の利用者向け説明。
- Windows / Web の required asset / known issue / smoke 再現手順。

## テスト項目

- README と addon README の言語サポート表記が実装と一致すること。
- template text catalog の文と UI / smoke が一致すること。
- Pages 運用メモが 6 言語 public smoke の実態と一致すること。

## 実装する unit テスト

- 必要なら support matrix / sample text catalog / README 記述の整合チェック。

## 実装する e2e テスト

- release 文書に反映した Windows / Web の再現手順を docs と同じコマンドで追えること。

## 実装に関する懸念事項

- template text の句読点や表記揺れが docs と UI でずれると、利用者にも CI にもノイズが出る。
- support matrix を先に更新しすぎると、未実装言語が対応済みに見えてしまう。

## レビューする項目

- 6 言語 support の表現が過大でも過小でもないか。
- Windows / Web で required asset の説明が抜けていないか。
- `TKT-007` へ渡す handoff が release note / Asset Library 文言まで足りているか。

## 一から作り直すとしたらどうするか

- support matrix、sample text catalog、README の一部を共通 metadata から生成する。

## 後続タスクに連絡する内容

- `TKT-007` へ、Windows / Web 6 言語対応の最終文言、sample text catalog、required asset、known issue を引き継ぐ。
