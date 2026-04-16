# 要求定義

更新日: 2026-04-14

この文書は `godot-piper-plus` の製品要求を定義する基準文書です。進捗管理や日々の残タスク管理は `docs/milestones.md` で扱い、この文書では「何を完成とみなすか」を固定します。

## 目的

- `piper-plus` を Godot 向け GDExtension addon として提供し、ローカル実行の高品質な音声合成を Godot プロジェクトへ組み込めるようにする
- `addons/piper_plus` を単位とした配布可能な addon package を成立させる
- runtime API、editor ツール、package、platform verification を含めた release 完了条件を明確化する

## 対象範囲

- 対象製品は Godot 4.4 以降向け addon `Piper Plus TTS`
- 主な利用者は、ゲームやツール内でオフライン TTS を使いたい Godot 開発者
- 配布単位は `addons/piper_plus`
- 実装の中心は `piper-plus` の C++ コア再利用と Godot 向け GDExtension 化

## 成果物

- `PiperTTS` ノードを含む GDExtension addon
- editor plugin
- model downloader
- custom dictionary editor
- Inspector 拡張と test speech UI
- 配布用 README / LICENSE / third-party notice
- package assembly / validator / smoke test script

## 機能要件

### FR-1 ランタイム合成 API

- `PiperTTS` は Godot ノードとして利用できること
- 同期合成 `synthesize(text)` を提供すること
- 非同期合成 `synthesize_async(text)` を提供すること
- 低遅延再生向け `synthesize_streaming(text, playback)` を提供すること
- 実行中の合成を `stop()` で停止できること
- 初期化状態と処理中状態を取得できること

### FR-2 入力モード

- text input から合成できること
- request dictionary ベースの合成 `synthesize_request(request)` を提供すること
- raw phoneme 入力 `synthesize_phoneme_string(phoneme_string)` を提供すること
- `phonemes` に `String`、`Array`、`PackedStringArray` を受け取れること
- inspection API として `inspect_text`、`inspect_request`、`inspect_phoneme_string` を提供すること

### FR-3 言語処理

- 日本語 text input を OpenJTalk で音素化できること
- 英語 text input を CMU 辞書ベース G2P で音素化できること
- bilingual / multilingual モデルに対して `ja/en` の最小自動ルーティングを提供すること
- Windows と Web では minimum 6-language として `ja/en/zh/es/fr/pt` の text input / inspect / synthesize を扱えること
- `ja/en` を超える広い multilingual parity 拡張を行い、upstream の model config / `language_id_map` に応じた追加言語の routing、language selection、inspection を扱えること
- multilingual parity の最低保証は explicit selection と inspect / synthesize parity であり、auto-routing は capability matrix で明示した言語に限定すること
- `tests/fixtures/multilingual_capability_matrix.json` を正本にし、`docs/generated/multilingual_capability_matrix.md` を doc-readable projection として維持すること
- 6 言語 minimum parity へ到達するまでの暫定 tier / 既知制約は capability matrix と milestone / ticket で明示できること
- `language_id` と `language_code` による言語選択を提供すること
- `speaker_id` による multi-speaker モデル選択を提供すること

### FR-4 モデルと辞書の解決

- `model_path` は実ファイル、ディレクトリ、登録済みモデル名/alias を受け付けること
- `config_path` 未指定時は `<model>.json`、次に `config.json` を探索すること
- 日本語向け `dictionary_path` を設定できること
- `custom_dictionary_path` による runtime 辞書前処理を提供すること
- 英語 text input では `cmudict_data.json` を addon 同梱ディレクトリ、モデル同梱ディレクトリ、config 同階層から探索できること

### FR-5 音声生成制御

- `speech_rate`、`noise_scale`、`noise_w` を設定できること
- `sentence_silence_seconds` を設定できること
- `phoneme_silence_seconds` を設定できること
- `[[ phonemes ]]` 記法の直入力を扱えること
- `get_last_synthesis_result()` で timing を含む結果を取得できること
- `get_last_inspection_result()` で音素列と解決言語情報を取得できること

### FR-6 backend と execution provider

- builtin OpenJTalk backend を利用できること
- `openjtalk-native` shared library を optional backend として読み込めること
- `openjtalk-native` 読み込み失敗時は builtin OpenJTalk に fallback すること
- ONNX Runtime の execution provider として CPU、CoreML、DirectML、NNAPI、Auto、CUDA を扱えること
- `gpu_device_id` による GPU device 指定を提供すること
- GPU provider が使えない場合は CPU fallback すること

### FR-7 Editor 支援機能

- model downloader を提供すること
- custom dictionary editor を提供すること
- `PiperTTS` 用 custom Inspector を提供すること
- editor 上で test speech を実行できること
- preset 適用や導線を Inspector から辿れること
- Windows の test speech UI では `ja/en/zh/es/fr/pt` の言語選択と template text 提示を扱えること

### FR-8 出力形式

- 同期合成の戻り値は `AudioStreamWAV` であること
- streaming 合成は `AudioStreamGeneratorPlayback` へ chunk を push できること
- 音声出力は 22050 Hz、16-bit PCM を基準とすること

### FR-9 配布要件

- 配布物は `addons/piper_plus` 単位で成立すること
- `piper_plus.gdextension` に定義された binary 群を package へ反映できること
- README / LICENSE / third-party notice を package に含めること
- model file (`.onnx` / `.onnx.json`) は package に同梱しないこと
- `naist-jdic` は package に同梱しないこと
- `openjtalk-native` 本体は package に同梱しないこと

### FR-10 Web platform 対応

- Web export 向けの build / package / export 導線を提供すること
- `web.*` 向け GDExtension entry と manifest を整備すること
- Web で利用可能な inference backend と制約を定義し、runtime の可否を検証できること
- browser load または同等の smoke test により addon のロード成否を確認できること

### FR-11 言語別 template text / sample UX

- Windows test speech UI と Web / Pages demo で `ja/en/zh/es/fr/pt` の sample text を共通 catalog として扱えること
- 言語選択時に、その言語で合成確認に使う canonical template text を初期入力または選択候補として提示できること
- canonical template text は `piper-plus` 側の multilingual testing / sample 文と整合し、smoke / docs / UI で同じ正本を参照できること

## 非機能要件

### NFR-1 互換性

- Godot 4.4 以降で利用できること
- `piper_plus.gdextension` の `compatibility_minimum` は `4.4` を下回らないこと

### NFR-2 オフライン動作

- 音声合成はクラウド接続なしで完結すること
- model / dictionary の取得を除き、runtime 合成時にネットワーク依存を持たないこと

### NFR-3 パフォーマンス

- C++ / GDExtension 実装を前提に、ゲーム実行中に利用可能な性能を確保すること
- 長文に対しては streaming 合成により time-to-first-audio を短縮できること

### NFR-4 安全性と依存制約

- GPL 系依存は持ち込まないこと
- path / backend 読み込みまわりで明らかな unsafe 動作を避けること
- optional backend や辞書欠落時に、可能な範囲で説明可能な failure / fallback を返すこと

### NFR-5 品質保証

- C++ unit test を継続実行可能であること
- Godot headless test を継続実行可能であること
- package validator により manifest と実ファイルの整合を検証できること
- all-skip / pass 0 / addon 未登録 / model bundle 欠落を CI failure として検出できること

### NFR-6 文書化

- runtime API は `doc_classes/PiperTTS.xml` と整合していること
- 配布手順と package 範囲は addon README に記述されていること
- 残タスクと進捗は `docs/milestones.md` と `docs/tickets/` で追跡できること

## サポート対象

release 判定の対象 platform は次のとおりです。

| プラットフォーム | アーキテクチャ | 要求 |
|---|---|---|
| Windows | x86_64 | source build と packaged addon の利用が成立すること |
| Linux | x86_64 | CI build と headless integration が成立すること |
| macOS | arm64 | packaged addon が利用可能であること |
| Android | arm64-v8a | export smoke と runtime 可否を確認できること |
| iOS | arm64 | export/link smoke を確認できること |
| Web | wasm32 相当 | build/export 導線と browser runtime 可否を確認できること |

## 対象外

- voice model の同梱配布
- `naist-jdic` や `openjtalk-native` 本体の同梱配布
- クラウド TTS サービス連携

## release 完了条件

次を満たした時点で、このブランチの release 要求は満たしたとみなします。

- C++ test が通過していること
- desktop 向け source build / headless test が成立していること
- packaged addon smoke の結果が Windows と macOS で確定していること
- Android export smoke の結果と runtime 可否が確定していること
- iOS export/link smoke の結果が確定していること
- `ja/en` を超える multilingual parity 拡張の対象範囲と capability matrix が実装と検証項目に反映されていること
- Web build / export / runtime 可否が確定していること
- Windows と Web で `ja/en/zh/es/fr/pt` の text input / inspect / synthesize が成立し、template text 導線と sample catalog が docs / smoke / UI で一致していること
- package validator が manifest 上の binary / dependency を検証できること
- package / README / license / changelog が最終状態へ更新されていること
- Asset Library へ申請できる package 導線が整っていること

## 現時点で残っている release gate

- Android arm64 の runtime 可否確認と必要修正
- Windows local Android export の generic configuration error 切り分け
- Windows packaged addon の 6-language smoke、Web / Pages の 6-language workflow / public smoke の実証
- macOS / iOS / Web の確定結果、および Android / Web Japanese / Windows-Web 6-language / shared sample catalog の最終判定を反映した package / 文書 / changelog / Asset Library 文言の最終化

## 関連文書

- `README.md`
- `addons/piper_plus/README.md`
- `doc_classes/PiperTTS.xml`
- `docs/milestones.md`
- `docs/tickets/README.md`
- `docs/web-github-pages-plan.md`
- `CHANGELOG.md`
