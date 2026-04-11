# TKT-010 Web runtime adaptation / ORT Web 対応

- 状態: `完了`
- 主マイルストーン: [M7 Web Support 完成](../milestones.md#m7)
- 関連マイルストーン: [M5 Quality Gate 完成](../milestones.md#m5)
- 関連要求: `FR-10` `FR-4` `FR-6` `NFR-2` `NFR-4` `NFR-5`
- 親チケット: [`TKT-002`](TKT-002-web-platform.md)
- 依存チケット: [`TKT-008`](TKT-008-web-template-toolchain-bootstrap.md) [`TKT-009`](TKT-009-web-manifest-package-export-preset.md)
- 後続チケット: [`TKT-011`](TKT-011-web-browser-smoke-ci-docs.md)

## 進捗

- [x] `FindOnnxRuntime.cmake` に Web static-lib 分岐を追加する
- [x] model / config / `cmudict_data.json` を含む dictionary 読み込みを path 非依存の形へ分離する
- [x] Web で unsupported な backend や optional native backend の扱いを固定する
- [x] Phase 1 の最小 synthesize と除外機能の境界を整理する
- [x] `TKT-011` の browser smoke 結果で、Web runtime contract と最小 synthesize の成立を最終確認する

## タスク目的とゴール

- Web で addon が実際に動く runtime 条件を実装し、Phase 1 の最小 synthesize が可能な backend と resource load の形を確立する。
- ゴールは、`libonnxruntime_webassembly.a` を link でき、path 前提に依存しない model / config / dict load と説明可能な unsupported error を実装すること。

## 実装メモ

- 2026-04-10 の GitHub Actions run `24223195868` で `Build Web` と browser smoke が成功し、Web runtime contract に沿って addon load と最小 synthesize まで成立することを確認しました。
- browser smoke では `threads` / `no-threads` の両方で `RESULT total=9 pass=4 fail=0 skip=5`、`WEB_SMOKE status=pass` を確認しています。

## 実装する内容の詳細

- `cmake/FindOnnxRuntime.cmake` に `libonnxruntime_webassembly.a` を扱う Web 分岐を追加する。
- `src/piper_core/piper.cpp` の config / model load を byte / string ベースへ分離し、Web で memory session に寄せられる形へ整理する。
- `src/piper_core/piper.cpp` の `cmudict_data.json` 解決を、`docs/requirements.md` の探索契約を崩さずに `FileAccess` 由来 resource へ寄せる。
- `src/piper_tts.cpp` で `FileAccess` 由来の resource load と unsupported provider handling を追加し、`test/prepare-assets.sh` が作る fixture 配置でも読み込めるようにする。
- `src/piper_core/openjtalk_wrapper.c` の `openjtalk-native` 分岐は Web で unsupported として固定し、Japanese text input / dictionary bootstrap は Phase 1 対象外として明示的な scope 判定を返す。must follow-up は [`TKT-019`](./TKT-019-web-japanese-dictionary-bootstrap.md) で扱う。
- Web では `execution_provider` を `EP_CPU` 固定にし、それ以外は machine-readable な error を返す方針を実装する。
- Phase 1 の runtime scope は固定済みで、Japanese text input / dictionary bootstrap の必須 follow-up は [`TKT-018`](./TKT-018-web-japanese-support.md) と [`TKT-019`](./TKT-019-web-japanese-dictionary-bootstrap.md) で扱います。

## 実装するために必要なエージェントチームの役割と人数

- `C++ runtime engineer` x1: core loader と provider 制御を担当する。
- `ONNX Runtime integration engineer` x1: Web static-lib と link 条件を担当する。
- `Godot resource engineer` x1: `FileAccess` 経由 load と GDExtension 側接続を担当する。
- `QA engineer` x1: runtime failure 条件と最小 synthesize 検証を担当する。

## 提供範囲

- ORT Web static-lib の build / link 対応。
- path 非依存の model / config / `cmudict_data.json` 読み込み。
- Web 向け unsupported backend と error handling。
- Phase 1 に含める text frontend / phoneme path の runtime scope 決定。

## テスト項目

- Web build が `libonnxruntime_webassembly.a` を解決できること。
- Web で `EP_CPU` 以外を指定した場合に説明可能な failure を返すこと。
- model / config / `cmudict_data.json` が path 前提なしで読み込めること。
- Phase 1 の最小モデルで synthesize まで進めること。

## 実装する unit テスト

- unsupported provider が `get_last_error()` に反映される C++ test を追加する。
- path ではなく byte / string loader を使う config / model / `cmudict_data.json` 解決テストを追加する。
- Web 分岐の ORT 検出条件を確認する configure-level check を追加する。

## 実装する e2e テスト

- `test/project` と `test/prepare-assets.sh` を使った最小 Web fixture で export 後の runtime 初期化と synthesize-ready signal、または deterministic な unsupported error まで確認する。
- `TKT-011` の browser smoke が再利用できる success / error signal と resource 配置を test project 側へ追加し、runtime 条件の前段を固定する。

## 境界条件

- full browser smoke、CI workflow、README 反映は `TKT-011` の責務とし、このチケットでは runtime contract と signal の提供までを扱う。

## 実装に関する懸念事項

- ORT Web static-lib のサイズや初期化時間が preview support の許容範囲を超える可能性がある。
- memory session 化の設計を誤ると desktop / mobile 側へ回帰を出す可能性がある。
- Japanese text input を Phase 1 に含めると dictionary bootstrap が想定以上に重くなる可能性がある。

## レビューする項目

- Web 向け runtime 分岐が既存 platform の path / provider 挙動を壊していないか。
- unsupported 機能が黙って fallback せず、説明可能な error を返しているか。
- Phase 1 の scope が `preview` `CPU-only` に収まっているか。

## 一から作り直すとしたらどうするか

- model / config / dict loader を最初から platform 非依存 API に切り出し、path ベース実装を backend の 1 実装として扱う。
- provider と backend 制約を capability table 化し、platform ごとの分岐を散らさない構成にする。

## 後続のタスクに連絡する内容

- `TKT-011` へ、browser smoke で見るべき成功ログ、unsupported error、必要 resource 構成を渡す。
- `TKT-002` へ、Phase 1 で実際に動く runtime scope、Japanese text input の扱い、除外機能を返す。
- `TKT-019` へ、Phase 1 で除外した dictionary bootstrap と Japanese text path の制約を引き継ぐ。
- `TKT-007` へ、README に明記すべき unsupported backend と preview 制約を引き継ぐ。
