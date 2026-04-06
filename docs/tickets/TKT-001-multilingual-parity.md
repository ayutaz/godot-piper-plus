# TKT-001 multilingual parity 拡張

- 状態: `未着手`
- 主マイルストーン: [M2 Language / Model / Backend 完成](../milestones.md#m2)
- 関連マイルストーン: [M5 Quality Gate 完成](../milestones.md#m5), [M8 Release / Asset Library 準備](../milestones.md#m8)
- 関連要求: `FR-3` `FR-4` `FR-6` `NFR-5`
- 依存チケット: なし
- 後続チケット: [`TKT-007`](TKT-007-release-finalization.md)

## 進捗

- [ ] 対象言語と routing 方針を確定する
- [ ] runtime 実装を拡張する
- [ ] unit test / headless test を追加する
- [ ] README / API 文書へ反映する

## タスク目的とゴール

- `ja/en` 最小対応で止まっている multilingual 実装を、要求定義に沿って `ja/en` を超える parity へ拡張する。
- upstream の `language_id_map` を持つモデルで、追加言語の routing、`language_id` / `language_code` 解決、inspection が一貫して動く状態を作る。
- 現行の `ja/en` 回帰を出さずに、対象言語の検証ケースを quality gate へ取り込む。

## 実装する内容の詳細

- `src/piper_core/piper.cpp` の `kSupportedMultilingualLanguages = {"ja", "en"}` 前提を見直し、対象言語を config 主導で扱える構造へ置き換える。
- `addons/piper_plus/models/css10/config.json` の `language_id_map` を基準候補として、少なくとも `zh` `es` `fr` `pt` を評価対象に入れる。
- `src/piper_core/language_detector.cpp` と `src/piper_core/multilingual_phonemize.*` の分割方針を整理し、script だけで判定可能な言語と、明示指定前提の言語を分ける。
- `src/piper_tts.cpp` の `language_code` 正規化と `language_id_map` 解決を追加言語でも通るように拡張する。
- `inspect_*` と `get_last_inspection_result()` が追加言語でも `resolved_language_id` / `resolved_language_code` を返すようにする。
- `doc_classes/PiperTTS.xml`、`README.md`、`addons/piper_plus/README.md` に supported language matrix と制約を書き出す。

## 実装するために必要なエージェントチームの役割と人数

- `runtime / phonemizer engineer` x2: multilingual segmentation、phonemizer routing、fallback 制御を実装する。
- `API / test engineer` x1: `PiperTTS` 側の `language_code` / `language_id` 契約と GDScript test を拡張する。
- `docs / QA engineer` x1: 対象言語マトリクス、制約、検証項目を文書へ反映する。

## 提供範囲

- コード: `src/piper_core/`, `src/piper_tts.cpp`
- テスト: `test/project/test_piper_tts.gd`, 必要なら C++ test
- 文書: `doc_classes/PiperTTS.xml`, `README.md`, `addons/piper_plus/README.md`, `docs/milestones.md`
- 対象外: voice model 自体の追加同梱、クラウド翻訳や外部言語判定サービスの導入

## テスト項目

- `language_id_map` を持つ multilingual model で追加言語が解決できる。
- `language_code` の別表記や大小文字混在が canonical code へ正規化される。
- auto-routing 対象言語は混在文を適切に sentence / segment へ分割できる。
- explicit 指定が必要な言語は silent skip ではなく説明可能な failure か明示 route を返す。
- `inspect_*` の結果に `resolved_language_id` / `resolved_language_code` が正しく載る。
- 既存の `ja/en`、`openjtalk-native` fallback、GPU fallback が回帰していない。

## 実装する unit テスト

- C++: multilingual language set の抽出、unsupported language の扱い、segment routing の分岐をテストする。
- GDScript: `test/project/test_piper_tts.gd` に追加言語の `language_code` 正規化、`inspect_text`、`inspect_request`、`synthesize_request` のケースを追加する。
- GDScript: explicit `language_id` / `language_code` と auto-routing の優先順位を検証する。

## 実装する e2e テスト

- headless の `test/project` で multilingual model を使い、追加言語ごとの inspection と最小合成を流す。
- packaged addon でも multilingual model が load できることを smoke に反映する。
- release 前に対象言語マトリクスを CI の pass/fail 条件へ組み込む。

## 実装に関する懸念事項

- Latin script を共有する `en` `es` `fr` `pt` は自動判定の誤検出が起きやすい。
- upstream model ごとに phoneme inventory や `language_id_map` が異なる可能性がある。
- 対象言語を広げすぎると、モデル依存差分とテスト資産不足で品質保証が崩れる。

## レビューをする項目

- 追加言語のサポート範囲が `README` と API 文書で一致しているか。
- unsupported language の扱いが silent skip ではなく説明可能な failure になっているか。
- 既存 `ja/en` の挙動と test の pass 条件を壊していないか。
- `language_id_map` が無いモデルでの挙動が明確か。

## ここまでのタスクで一から作り直すとしたらどうするか

- 最初から `language_id_map` 駆動の capability table を `PiperTTS` 初期化時に構築し、`ja/en` ハードコードを作らない。
- auto-routing と explicit language selection を分離し、script で自動判定できる言語だけを auto に入れる。
- テストモデルと supported language matrix を同時に整備してから API を固定する。

## 後続のタスクに連絡する内容

- `TKT-007` には、最終的に採用した対象言語一覧、auto-routing 対象、explicit 指定必須言語、既知の制約を引き渡す。
- `M5` には、multilingual の pass 条件と skip 許容条件を明示して渡す。
- `TKT-002` と競合する場合は、Web でサポートしない言語や backend 制約も同時に整理する。
