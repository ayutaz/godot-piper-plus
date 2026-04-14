# TKT-024 Windows 6-language runtime / smoke / template UI

- 状態: `要確認`
- 主マイルストーン: [M11 Windows / Web 6-Language Text Input / Template UX 完成](../milestones.md#m11)
- 関連マイルストーン: [M5 Quality Gate 完成](../milestones.md#m5) [M6 Platform Verification 完成](../milestones.md#m6)
- 関連要求: `FR-3` `FR-7` `FR-11` `NFR-5` `NFR-6`
- 親チケット: `統合済み (旧 TKT-022)`
- 依存チケット: なし
- 後続チケット: [`TKT-007`](./TKT-007-release-finalization.md)

## 進捗

- [x] Windows で 6 言語 text input / inspect / synthesize の pass 条件を fixed contract に反映する
- [x] custom Inspector / test speech UI に言語 selector と template text 挿入を追加する
- [x] source build / headless test へ 6 言語 sample set と descriptor validation を反映する
- [ ] packaged addon smoke を 6 言語 sample set に広げて実行証跡を残す
- [ ] Windows の packaged addon 再現手順と既知制約を最終文書へ固定する

## タスク目的とゴール

- Windows 環境で、最低限 `ja/en/zh/es/fr/pt` の 6 言語 text input / inspect / synthesize を packaged addon と editor test speech UI の両方で再現可能にする。
- ゴールは、Windows 上で言語選択と template text が連動し、sample text からそのまま合成確認できること。

## 実装する内容の詳細

- `PiperTTS` 用 custom Inspector / test speech UI に language selector を追加し、統合済みの contract fixture / addon descriptor から template text を読み出す。
- Windows source build と packaged addon の両方で、6 言語の inspect / synthesize を通す最小 smoke を用意する。
- `multilingual-test-medium` を基準に、6 言語 sample text を explicit `language_code` または `language_id` で合成し、audible / non-silent output と説明可能な runtime summary を確認する。
- `zh` text input で必要な frontend / asset がある場合は、Windows package と local dev の両方で解決できるようにする。
- `es/fr/pt` は rule-based path を packaged addon 環境で成立させ、UI だけが先に 6 言語化しないよう smoke と同時に閉じる。
- 既存の Windows packaged addon smoke と headless integration を壊さず、6 言語 path の専用 scenario を足す。current branch では selector / preview session / alias 正規化 / descriptor read の実装と headless test までは反映済みです。

## 提供範囲

- Windows test speech UI の 6 言語 selector / template text UX。
- Windows packaged addon / local smoke の 6 言語 pass 条件。
- Windows local の sample text / required asset / known issue の記録。

## テスト項目

- Windows で 6 言語の text input / inspect / synthesize が成立すること。
- 言語 selector 切り替えで template text が期待どおりに変わること。
- packaged addon と source build の両方で 6 言語 sample set を再現できること。
- `zh` だけ runtime failure のまま残っていないこと。

## 実装する unit テスト

- language selector と template text 適用ロジックの editor-side test。
- Windows smoke helper の scenario selection と summary 判定 test。

## 実装する e2e テスト

- packaged addon を `test/project` に同期して、6 言語 sample text を順に inspect / synthesize する Windows smoke。
- editor test speech UI で言語 selector と template text が連動する最小 UI smoke。

## 実装に関する懸念事項

- Windows GUI を含む smoke は headless より flaky になりやすい。
- `zh` text input の資産や preprocessing が Windows package に追加で必要になる可能性がある。

## レビューする項目

- Windows 向け 6 言語対応が UI だけでなく packaged addon smoke まで閉じているか。
- selector と template text の正本が統合済みの catalog / descriptor 契約に揃っているか。
- Windows 固有の path / encoding 問題で sample 文が壊れていないか。

## 一から作り直すとしたらどうするか

- Windows test speech UI を最初から language-aware にして、sample text catalog を runtime から参照する前提で設計する。

## 後続タスクに連絡する内容

- `TKT-007` へ、Windows platform 表記と template text UX の最終表現を引き継ぐ。
