# TKT-023 6-language capability / template text contract

- 状態: `未着手`
- 主マイルストーン: [M11 Windows / Web 6-Language Text Input / Template UX 完成](../milestones.md#m11)
- 関連マイルストーン: [M5 Quality Gate 完成](../milestones.md#m5)
- 関連要求: `FR-3` `FR-11` `NFR-5` `NFR-6`
- 親チケット: [`TKT-022`](./TKT-022-windows-web-six-language-support.md)
- 依存チケット: [`TKT-022`](./TKT-022-windows-web-six-language-support.md)
- 後続チケット: [`TKT-024`](./TKT-024-windows-six-language-runtime-smoke.md) [`TKT-025`](./TKT-025-web-six-language-runtime-demo-smoke.md) [`TKT-026`](./TKT-026-six-language-docs-release-sync.md)

## 進捗

- [ ] 6 言語 minimum parity の capability contract を固定する
- [ ] template text catalog の正本と投影先を固定する
- [ ] `language_code` / alias / sample text / required asset の schema を固定する
- [ ] `TKT-024` と `TKT-025` が再利用する canonical sample set を handoff する

## タスク目的とゴール

- Windows / Web で共有する 6 言語 capability matrix と template text catalog の正本を固定する。
- ゴールは、`ja/en/zh/es/fr/pt` の language selector、smoke、README、Pages demo、Windows test speech UI が同じ catalog を参照できること。

## 実装する内容の詳細

- `tests/fixtures/multilingual_capability_matrix.json` と `docs/generated/multilingual_capability_matrix.md` の扱いを、6 言語 minimum parity と platform note を表現できる形へ更新する。
- template text 用に、fixture / generated doc / UI data が共有できる正本を追加する。候補は `tests/fixtures/multilingual_sample_text_catalog.json` とその doc projection。
- catalog は最低限 `language_code`, `display_name`, `aliases`, `template_text`, `required_assets`, `windows_supported`, `web_supported`, `notes` を持てる形で固定する。
- canonical template text の初期案は `piper-plus` の multilingual testing guide と整合する次の 6 文とする。

| 言語 | code | canonical template text |
|---|---|---|
| Japanese | `ja` | `こんにちは、今日は良い天気ですね。` |
| English | `en` | `Hello, how are you today?` |
| Chinese | `zh` | `你好，今天天气很好。` |
| Spanish | `es` | `Hola, como estas hoy?` |
| French | `fr` | `Bonjour, comment allez-vous?` |
| Portuguese | `pt` | `Ola, como voce esta hoje?` |

- `language_id` の値は model config / `language_id_map` を正本とし、sample text catalog 側は `language_code` を主キーにする。`piper-plus` README の言語順を fixed constant として持ち込まない。
- `zh` text input の asset / frontend 契約、`es/fr/pt` rule-based path の Windows / Web 前提、`ja` の `naist-jdic`、`en` の `cmudict_data.json` を catalog から追えるようにする。

## 提供範囲

- 6 言語 capability contract。
- template text catalog と schema。
- `TKT-024` と `TKT-025` が使う canonical sample text set。

## テスト項目

- capability matrix と template text catalog が docs / tests / UI で drift しないこと。
- `language_code` と alias 解決で 6 言語を一意に扱えること。
- required asset の説明が Windows / Web の runtime 契約と一致すること。

## 実装する unit テスト

- capability / sample text catalog の schema validation。
- generated doc と fixture の同期チェック。
- sample text catalog と capability matrix の `language_code` 集合一致チェック。

## 実装する e2e テスト

- `TKT-024` と `TKT-025` の smoke が同じ catalog を読み、同じ sample 文で動作確認できること。

## 実装に関する懸念事項

- template text を docs にだけ置くと UI や smoke が別文を持って drift しやすい。
- `zh` は現状 `phoneme-only` のため、catalog を先に固定しても runtime 契約が追いつかなければ false promise になる。

## レビューする項目

- sample text が `piper-plus` 側の multilingual sample と整合しているか。
- `language_id` ではなく `language_code` を主キーにしているか。
- Windows / Web の asset 要件が catalog で説明可能になっているか。

## 一から作り直すとしたらどうするか

- capability matrix と sample text catalog を最初から同じ metadata tree に入れ、generated doc と UI data を自動生成する。

## 後続タスクに連絡する内容

- `TKT-024` へ、Windows test speech UI と packaged addon smoke が使う 6 言語 sample text を渡す。
- `TKT-025` へ、Web export / Pages demo / public smoke が使う 6 言語 sample text と required asset note を渡す。
- `TKT-026` へ、README と support matrix に載せる canonical sample text と tier 表現を渡す。
