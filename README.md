# godot-piper-plus

[piper-plus](https://github.com/ayutaz/piper-plus)のGodotプラグイン - 高品質なニューラル音声合成エンジン

## 概要

godot-piper-plusは、[piper-plus](https://github.com/ayutaz/piper-plus)（VITS ニューラルTTSエンジン）をGodotエンジン向けにGDExtensionとして移植するプロジェクトです。ローカルで動作する高品質な音声合成をGodotゲームに組み込むことができます。

## Godotでの開き方

Godotプロジェクトとして開く場合は、このリポジトリのルートディレクトリを指定してください。デモシーンは `res://demo/main.tscn` です。

## 機能

- 高品質なニューラル音声合成（VITS / piper-plusベース）
- 日本語テキスト音声合成（OpenJTalk）
- 英語テキスト音声合成（CMU辞書ベースの英語 G2P）
- multilingual capability matrix に基づく言語サポート
- GDExtension（C++）によるネイティブパフォーマンス
- OpenJTalk による日本語音素化
- `openjtalk-native` DLL の任意利用と builtin OpenJTalk fallback
- `language_id` / `language_code` 指定とモデル名/エイリアス解決
- Prosody（韻律）サポート：より自然なイントネーション（A1/A2/A3）
- 同期 / 非同期 / streaming 合成
- カスタム辞書エディタと runtime 適用（`custom_dictionary_path`）
- CUDA 実行プロバイダと `gpu_device_id` 指定
- オフライン動作（クラウド不要）
- Web preview support は `CPU-only` 前提で、custom Web export template が必要です

## 現在の対応状況

現時点で repo 内の `P0` `P1` `P2` の機能実装は完了しています。release package と platform verification は継続中ですが、`R1` から `R4` の package/CI 是正は実装済みです。現実装のスコープは次のとおりです。

- 日本語 OpenJTalk と英語 CMU 辞書ベース G2P を使った text input 合成
- bilingual / multilingual モデルに対する capability-based routing
- `get_language_capabilities()` / `get_last_error()` による multilingual contract と machine-readable failure の取得
- `language_id` と `language_code` による言語選択
- `model_path` の実ファイル指定、モデル名/エイリアス解決、`config_path` fallback
- `synthesize_request` / `inspect_request` による request ベース API
- `synthesize_phoneme_string` / `inspect_phoneme_string` による raw phoneme 入力
- `sentence_silence_seconds` / `phoneme_silence_seconds` の制御
- `inspect_*` と `get_last_synthesis_result()` による dry-run / timing / 結果取得
- `custom_dictionary_path` による辞書前処理と `[[ phonemes ]]` 直入力
- `resolved_segments` を含む inspection / synthesis metadata の取得
- `openjtalk_library_path` による `openjtalk-native` shared library 指定と builtin OpenJTalk fallback
- `execution_provider = EP_CUDA` と `gpu_device_id` による GPU device 指定
- Inspector 拡張、preset 適用、テスト発話 UI、モデル downloader、辞書 editor
- multilingual text input の現状サポートは capability matrix 基準です。`ja` / `en` は preview auto / explicit、`es` / `fr` / `pt` は experimental explicit-only、`zh` は phoneme-only です。正本は `tests/fixtures/multilingual_capability_matrix.json`、人が読む投影は `docs/generated/multilingual_capability_matrix.md` です
- 英語 text input では `cmudict_data.json` が必要です。`res://piper_plus_assets/dictionaries/`、`addons/piper_plus/dictionaries/`、モデル同梱ディレクトリ、または config 同階層を探索します。
- CUDA を使う場合は CUDA 対応 ONNX Runtime binary を別途用意してください。既定の CPU 向け binary では自動的に CPU fallback します。

## 検証状況

- 2026-04-06 時点で `ctest --test-dir build-p1-debug -C Debug --output-on-failure` は `131/131` pass です。multilingual capability matrix の freshness check と matrix-first C++ test を含みます
- 2026-04-06 時点で Godot headless の `test/project` は `32 pass / 0 fail / 1 skip` です。skip は OpenJTalk dictionary asset が無い環境の既知ケースで、multilingual request / streaming test は通過しています
- 2026-03-23 時点で `scripts/ci/package-addon.sh` は `.gdextension` に書かれた debug / release binary を全 platform 分拾い、Windows の `onnxruntime_providers_shared.dll` も package へ含めます
- 2026-03-23 時点で `scripts/ci/validate-addon-package.sh` は manifest 上の全 binary / dependency を検証し、local で Windows だけの partial package を渡した場合も Android/iOS binary 欠落で失敗することを確認しています
- 2026-03-23 時点で Windows の packaged addon は、現在の local build bin から組み立てた package を使う headless smoke (`scripts/ci/smoke-test-packaged-addon.sh`) で再確認済みです
- Windows の headless 実行では `addons/piper_plus/bin/onnxruntime.dll` が必要です。`test/prepare-assets.sh` は `onnxruntime.windows.x86_64.dll` しか無い場合でも plain 名へ複製します
- 2026-03-23 時点で `openjtalk-native` 無効パス時の builtin fallback と `gpu_device_id` の GDScript test を追加済みです。compiled `naist-jdic` が無い環境では builtin fallback の日本語 test は skip されます
- `test/project/main.gd` と `test/run-tests.sh` は all-skip / pass 0 / `PiperTTS class is unavailable` / model bundle 欠落を CI failure として扱うように更新済みです
- GitHub Actions には Windows / macOS packaged addon smoke test に加えて、Android export smoke と iOS export/link smoke の job を追加しました。初回実行結果の確認はまだ残っています
- 2026-03-23 時点で Android arm64 の debug GDExtension build は Windows local で通過しています。`test/project/export_presets.cfg` と `scripts/ci/export-android-smoke.sh` / `scripts/ci/export-ios-smoke.sh` / `scripts/ci/install-godot-export-templates.sh` を追加しました
- 2026-03-23 時点で Windows + Godot 4.6 の local Android headless export は generic な configuration error でまだ通っていません。Android/iOS の可否確定は CI の初回結果待ちです

## サポートプラットフォーム

| プラットフォーム | アーキテクチャ | 状態 | 備考 |
|----------------|-------------|------|------|
| Windows | x86_64 | source build と local packaged smoke で動作確認済み | packaged addon smoke の CI job も追加済み |
| Linux | x86_64 | CI build と headless integration あり | all-skip / pass 0 を failure 扱いに更新済み |
| macOS | arm64 (Apple Silicon) | CI build と C++ test 済み | packaged addon smoke の CI job を追加済み。初回実行結果は未確認 |
| Android | arm64-v8a | CI build / package gate / export smoke job 追加済み | debug / release binary は validator 対象。初回 CI 結果と runtime 可否の確定待ち |
| iOS | arm64 | CI build / package gate / export-link smoke job 追加済み | debug / release binary は validator 対象。初回 CI 結果の確定待ち |
| Web | wasm32 | preview support | custom Web export template と `EP_CPU` 前提です。`openjtalk-native` は未対応です |

release package をそのまま「全 platform で使える addon」と言い切るにはまだ早く、残っている主作業は macOS packaged runtime の確認、Android/iOS export smoke の初回結果確認、Web preview smoke / CI の初回確認、必要ならその場で出る export/link 問題の修正です。

## Web Preview

Web は preview support です。前提は次のとおりです。

- custom Web export template が必要です
- toolchain は Godot 4.4.1 向けに `emsdk 3.1.62` を前提にしています
- 実行プロバイダは `EP_CPU` 固定です
- `openjtalk-native` shared library は Web では使えません
- ブラウザ実行には `COOP` / `COEP` を付けた static server が必要です

ローカル smoke は次の流れで実行します。

```bash
GODOT_SOURCE_DIR=/path/to/godot-source INSTALL_TO_EXPORT_TEMPLATES=1 bash scripts/ci/build-godot-web-templates.sh /tmp/godot-web-templates
bash scripts/ci/build-onnxruntime-web.sh /path/to/onnxruntime-source /tmp/ort-web
ONNXRUNTIME_DIR=/tmp/ort-web bash scripts/ci/build-web-side-module.sh /tmp/web-artifacts
GODOT=/path/to/godot bash scripts/ci/export-web-smoke.sh
```

`run-web-smoke.mjs` は Node.js と Playwright が必要です。ローカルでは `npm install --no-save playwright` の後に `npx playwright install chromium` を実行してください。

## 対応モデル

モデルは `res://piper_plus_assets/models/` へ手動配置するか、エディタの downloader から取得する前提です。legacy fallback として `addons/piper_plus/models/` も探索します。`model_path` には `.onnx` の実ファイルだけでなく、登録済みモデル名やエイリアスも指定できます。`config_path` を省略した場合は `<model>.json`、次に同じディレクトリの `config.json` を探索します。

| モデル名 | 言語 | Prosody対応 | 説明 |
|---------|------|------------|------|
| ja_JP-test-medium | 日本語 | なし | 標準日本語モデル |
| en_US-ljspeech-medium | 英語 | なし | 英語 G2P 対応の英語モデル |
| tsukuyomi-chan | 日本語 | あり | Prosody対応日本語モデル（より自然なイントネーション） |

## アーキテクチャ

```
テキスト入力
    ↓
[任意] `custom_dictionary_path` による前処理
    ↓
`[[ phonemes ]]` 直入力のパース
    ↓
Phonemizer（音素変換）
    • 日本語: builtin OpenJTalk または `openjtalk-native`
    • 英語: CMU辞書ベースの英語 G2P
    • bilingual / multilingual:
        - `ja/en`: capability-based auto / explicit routing
        - `es/fr/pt`: explicit-only
        - `zh`: phoneme-only
    ↓
音素エンコーディング（PUA マッピング）
    ↓
VITS推論（ONNX Runtime / CPU, DirectML, CoreML, NNAPI, CUDA）
    ↓
AudioStreamWAV / AudioStreamGenerator 出力（22050Hz, 16bit PCM）
```

## 依存ライブラリ

| ライブラリ | 用途 | リンク方式 |
|-----------|------|-----------|
| [godot-cpp](https://github.com/godotengine/godot-cpp) | GDExtension C++バインディング | サブモジュール |
| [ONNX Runtime](https://onnxruntime.ai/) | VITS推論エンジン | 動的リンク |
| [OpenJTalk](https://open-jtalk.sourceforge.net/) | 日本語音素化 | 静的リンク |
| `openjtalk-native` | 日本語音素化 backend（任意） | 動的ロード |

## 関連プロジェクト

| プロジェクト | 説明 |
|-------------|------|
| [piper-plus](https://github.com/ayutaz/piper-plus) | TTSエンジン本体（C++/Python） |
| [uPiper](https://github.com/ayutaz/uPiper) | Unity向けプラグイン |
| [dot-net-g2p](https://github.com/ayutaz/dot-net-g2p) | 純C# 日英G2Pライブラリ |

## ライセンス

[Apache License 2.0](LICENSE)
