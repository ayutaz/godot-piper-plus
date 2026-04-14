# TKT-022 Windows / Web 6-language input / synthesize / template text 対応

- 状態: `進行中`
- 主マイルストーン: [M11 Windows / Web 6-Language Text Input / Template UX 完成](../milestones.md#m11)
- 関連マイルストーン: [M5 Quality Gate 完成](../milestones.md#m5) [M8 Release / Asset Library 準備](../milestones.md#m8) [M10 Web Japanese Support / Pages Japanese Demo 完成](../milestones.md#m10)
- 関連要求: `FR-3` `FR-7` `FR-10` `FR-11` `NFR-2` `NFR-5` `NFR-6`
- 子チケット: [`TKT-023`](./TKT-023-six-language-capability-template-contract.md) [`TKT-024`](./TKT-024-windows-six-language-runtime-smoke.md) [`TKT-025`](./TKT-025-web-six-language-runtime-demo-smoke.md) [`TKT-026`](./TKT-026-six-language-docs-release-sync.md)
- 依存チケット: [`TKT-018`](./TKT-018-web-japanese-support.md)
- 後続チケット: [`TKT-007`](./TKT-007-release-finalization.md)

## 進捗

- [x] Windows / Web minimum 6-language parity と template text UX を follow-up milestone として定義する
- [x] capability contract / Windows / Web / docs sync の child ticket に責務分割する
- [ ] 6 言語 capability と template text catalog の正本を固定する
- [ ] Windows packaged addon と test speech UI で 6 言語合成を成立させる
- [ ] Web export と Pages demo で 6 言語合成を成立させる
- [ ] `TKT-007` へ release 文書化条件を handoff する

## タスク目的とゴール

- Windows と Web で、最低限 `ja/en/zh/es/fr/pt` の 6 言語 text input / inspect / synthesize を成立させる。
- あわせて、言語選択に応じた template text を Windows の test speech UI と Web / Pages demo へ出し分けられるようにする。
- ゴールは、`piper-plus` の multilingual sample と整合する sample text を 1 つの catalog として固定し、Windows / Web / smoke / docs が同じ正本を参照すること。

## 実装する内容の詳細

- `M10` で整備した `ja/en` Web baseline を出発点に、Windows packaged addon と Web export / Pages demo の両方へ 6 言語 minimum parity を広げる。
- 最低保証は `language_code` / `language_id` による explicit selection と、text input / inspect / synthesize の成立とする。auto-routing は capability matrix に明示した範囲だけでよい。
- 言語 selector の対象は `ja`, `en`, `zh`, `es`, `fr`, `pt` とし、選択時にその言語用の canonical template text を初期入力または挿入候補として提示できるようにする。
- sample text は `piper-plus` の multilingual testing / sample 文と整合するセットを使う。numeric な `language_id` の並びは `piper-plus` README の例に固定せず、model config と capability matrix を正本にする。
- Windows 側では custom Inspector / test speech UI、packaged addon smoke、必要なら `test/project` 側の fixture を更新する。
- Web 側では export artifact、browser smoke、Pages demo、public smoke、必要 asset staging を更新する。
- 文書は `README.md`、addon README、`docs/milestones.md`、`docs/generated/`、release 文書の順で同期し、`TKT-007` へ handoff する。

## 提供範囲

- 6 言語 minimum parity の scope / acceptance。
- sample text catalog と capability contract の固定。
- Windows test speech UI と Web / Pages demo の template text UX。
- Windows packaged addon smoke と Web / Pages smoke の pass 条件。

## テスト項目

- Windows と Web の両方で `ja/en/zh/es/fr/pt` の text input / inspect / synthesize が成立すること。
- 言語 selector で出る template text が UI / smoke / docs で一致すること。
- `zh` が phoneme-only のまま残らず、text input で説明可能な success path を持つこと。
- `es/fr/pt` が experimental-only のまま UI だけ先行しないこと。
- `TKT-007` が package / README / support matrix を迷わず更新できること。

## 実装に関する懸念事項

- `zh` text input を Windows / Web 両方で成立させるには追加 asset と frontend 契約が必要になる可能性がある。
- Web は日本語だけでなく 6 言語分の初期化時間と artifact サイズが増え、PR CI が重くなる。
- template text catalog を UI ごとに直書きすると、sample 文の差分がすぐ drift する。

## レビューする項目

- 6 言語 minimum parity を「Windows / Web での明示選択と text input 成立」として書けているか。
- `language_code` 基準の選択と sample text catalog が numeric `language_id` の思い込みに依存していないか。
- Windows と Web で別々の sample 文や別々の pass 条件を持っていないか。

## 一から作り直すとしたらどうするか

- 6 言語 sample text catalog を最初から fixture / generated doc / UI data の 3 面共有にする。
- Web Japanese follow-up を閉じた直後に、Windows / Web parity track を独立 milestone として切り出しておく。

## 後続タスクに連絡する内容

- `TKT-023` へ、capability matrix と template text catalog を 6 言語 minimum parity の正本として固定することを渡す。
- `TKT-024` と `TKT-025` へ、UI / smoke / docs は同じ sample text catalog を参照する方針を渡す。
- `TKT-007` へ、Windows / Web 6 言語対応が release 文書の必須入力になったことを引き継ぐ。
