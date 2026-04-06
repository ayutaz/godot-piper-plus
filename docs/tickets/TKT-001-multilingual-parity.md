# TKT-001 multilingual parity 拡張

- 状態: `完了`
- 主マイルストーン: [M2 Language / Model / Backend 完成](../milestones.md#m2)
- 関連マイルストーン: [M5 Quality Gate 完成](../milestones.md#m5), [M8 Release / Asset Library 準備](../milestones.md#m8)
- 関連要求: `FR-3` `FR-4` `FR-6` `NFR-5`
- 依存チケット: なし
- 後続チケット: [`TKT-007`](TKT-007-release-finalization.md)

## 進捗

- [x] capability matrix を正本化する
- [x] MVP と stretch の境界を固定する
- [x] matrix-first の C++ / headless test を追加する
- [x] README / API 文書 / milestone を capability-based に揃える

## 完了メモ

- multilingual contract は `tests/fixtures/multilingual_capability_matrix.json` を正本、`docs/generated/multilingual_capability_matrix.md` を投影として固定しました。
- `PiperTTS` は `get_language_capabilities()`、`get_last_error()`、`resolved_segments`、`synthesize_streaming_request()` を含む capability-first contract に更新済みです。
- request ごとの `SynthesisConfig` を使う request-local routing に揃え、`inspect_*` と `synthesize_*` の language resolution を共通化しました。
- `es` / `fr` / `pt` は `experimental explicit-only`、`zh` は `phoneme-only` / `raw_phoneme_only` として machine-readable に露出しています。
- 検証は `ctest --test-dir build-p1-debug -C Debug --output-on-failure` で `131/131` pass、`godot_console.exe --path test/project --headless` で `32 pass / 0 fail / 1 skip` です。skip は OpenJTalk dictionary asset が無い既知ケースです。

## タスク目的とゴール

- `ja/en` の最小 multilingual に止まっている現状を、capability-first の contract に置き換える。
- upstream の `language_id_map` と model config から、explicit-only / auto / phoneme-only を説明可能に分ける。
- `tests/fixtures/multilingual_capability_matrix.json` を正本にし、`docs/generated/multilingual_capability_matrix.md` を doc-readable projection として維持する。
- `inspect_*` と `synthesize_*` が同じ language resolution 結果を共有し、unsupported input は silent skip ではなく明示 failure になる状態を作る。
- 既存の `ja/en` 回帰を出さずに、対象言語の検証ケースを quality gate へ取り込む。

## MVP

- 1 つの multilingual model に対して capability matrix を正本として読み込めること。
- `language_code` の正規化と `language_id` の解決が、model config / `language_id_map` と整合すること。
- `inspect_*` が resolved language 情報を返し、`synthesize_*` と同じ前処理結果を使うこと。
- `explicit-only` 言語は明示指定で通り、`phoneme-only` 言語は raw phoneme input でのみ通ること。
- unsupported language は説明可能な failure を返し、silent skip をしないこと。

## Stretch

以下は今回の ticket 完了範囲外です。必要になった時点で別 ticket または `M5` / `TKT-007` 側へ切り出します。

- `ja/en` 以外で auto-routing を許可する言語を、script 判定と golden test が揃った範囲に限定して追加する。
- 複数の upstream model family をまたぐ capability matrix を追加する。
- packaged addon smoke や broader CI matrix を multilingual contract の昇格条件として扱う。
- `zh` のような phoneme-only 言語を text phonemizer 対応へ昇格させるのは別 ticket に分離する。

## 実装する内容の詳細

- `language_id_map` を model capability の起点として扱い、`language_id` と `language_code` の正規化を capability table へ集約する。
- language の扱いを `preview` `experimental explicit-only` `phoneme-only` `unsupported` に分け、README / API 文書 / tests で同じ分類を使う。
- auto-routing は script で高信頼に切れる language に限定し、Latin 系の曖昧さは explicit-only に留める。
- text phonemizer を持たない language は model に存在しても text input では読めない前提を明示する。
- `inspect_*` と `get_last_inspection_result()` は resolved language と failure semantics を一貫させる。
- release 前の pass / fail は `M5` の quality gate に分離し、この ticket では contract の固定に集中する。

## 実装するために必要なエージェントチームの役割と人数

- `capability / runtime engineer` x2: capability table、language resolution、routing boundary を設計する。
- `API / test engineer` x1: `PiperTTS` 側の contract と matrix-first test を整備する。
- `docs / QA engineer` x1: supported matrix、制約、検証項目を文書へ反映する。

## 提供範囲

- テスト: `tests/fixtures/`, `tests/test_language_detector.cpp`, 必要なら `test/project/test_piper_tts.gd`
- 文書: `README.md`, `addons/piper_plus/README.md`, `docs/requirements.md`, `docs/milestones.md`, `docs/tickets/README.md`
- 対象外: voice model 自体の追加同梱、クラウド翻訳や外部言語判定サービスの導入

## テスト項目

- capability matrix にある language ごとの routing mode が docs と一致する。
- experimental explicit-only 言語は text adapter として通るが parity-grade ではない。
- phoneme-only 言語は raw phoneme input でのみ通る。
- unsupported language は説明可能な failure になる。
- `inspect_*` の結果に resolved language 情報が載り、`synthesize_*` と整合する。
- 既存の `ja/en`、`openjtalk-native` fallback、GPU fallback が回帰していない。

## 実装する unit テスト

- C++: `tests/fixtures/multilingual_capability_matrix.json` を読み、routing mode、text support、auto support、error semantics を検証する。
- C++: explicit-only 言語の phonemizer smoke を matrix 由来の sample text で検証する。
- C++: phoneme-only 言語の text support が存在しないことを確認する。
- C++: `docs/generated/multilingual_capability_matrix.md` が JSON matrix と一致することを確認する。
- GDScript: `language_code` 正規化、`inspect_text`、`inspect_request`、`synthesize_request` の case を matrix の contract に合わせて検証する。

## 実装する e2e テスト

- headless の `test/project` で multilingual model を使い、capability matrix に沿った inspection と最小合成を流す。
- packaged addon smoke での explicit-only / unsupported path の最終確認は `M5` と `TKT-003` から `TKT-007` へ引き継ぐ。
- release 前の broader CI matrix への昇格は `M5` と `TKT-007` 側で扱う。

## 実装に関する懸念事項

- Latin script を共有する `en` `es` `fr` `pt` は自動判定の誤検出が起きやすい。
- upstream model ごとに phoneme inventory や `language_id_map` が異なる可能性がある。
- text phonemizer を持たない language を text support と誤認すると、後続の quality gate が崩れる。
- 仕様を docs と tests の両方へ反映しないと、また実装・検証・文書が分離する。

## レビューをする項目

- capability matrix が README と API 文書で一致しているか。
- unsupported language の扱いが silent skip ではなく説明可能な failure になっているか。
- experimental explicit-only と phoneme-only の境界が曖昧になっていないか。
- `language_id_map` が無いモデルでの挙動が明確か。
- unit / headless / packaged smoke の責務が混ざっていないか。

## Clean-slate review

- 最初から `language_id_map` 駆動の capability table を `PiperTTS` 初期化時に構築し、`ja/en` ハードコードを作らない。
- auto-routing と explicit selection を分離し、script で自信を持って切れる言語だけを auto に入れる。
- `language_id_map` と text frontend capability は別概念として扱い、phoneme-only language を隠さない。
- `inspect_*` と `synthesize_*` は同じ `ResolvedRequest` を共有し、結果差分は inference 以降だけに限定する。
- capability matrix を仕様の正本にし、docs と tests はそこから派生させる。

## 後続のタスクに連絡する内容

- `TKT-007` には、最終的に採用した対象言語一覧、auto-routing 対象、explicit 指定必須言語、phoneme-only language、既知の制約を引き渡す。
- `M5` には、multilingual の pass 条件と skip 許容条件を capability matrix ベースで明示して渡す。
- `TKT-002` と競合する場合は、Web でサポートしない言語や backend 制約も同時に整理する。
