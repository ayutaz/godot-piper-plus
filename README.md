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
- ja/en 混在テキストの最小 multilingual ルーティング
- GDExtension（C++）によるネイティブパフォーマンス
- OpenJTalk による日本語音素化
- `openjtalk-native` DLL の任意利用と builtin OpenJTalk fallback
- `language_id` / `language_code` 指定とモデル名/エイリアス解決
- Prosody（韻律）サポート：より自然なイントネーション（A1/A2/A3）
- 同期 / 非同期 / streaming 合成
- カスタム辞書エディタと runtime 適用（`custom_dictionary_path`）
- CUDA 実行プロバイダと `gpu_device_id` 指定
- オフライン動作（クラウド不要）

## 現在の対応状況

現時点で repo 内の `P0` `P1` `P2` の機能実装は完了しています。ただし、配布 package とプラットフォーム検証は未完です。特に debug/editor 向け native binary の同梱、ONNX Runtime sidecar 依存の package 反映、Windows/macOS/mobile の Godot 実行検証が残っています。現実装のスコープは次のとおりです。

- 日本語 OpenJTalk と英語 CMU 辞書ベース G2P を使った text input 合成
- bilingual / multilingual モデルに対する `ja/en` 最小ルーティング
- `language_id` と `language_code` による言語選択
- `model_path` の実ファイル指定、モデル名/エイリアス解決、`config_path` fallback
- `synthesize_request` / `inspect_request` による request ベース API
- `synthesize_phoneme_string` / `inspect_phoneme_string` による raw phoneme 入力
- `sentence_silence_seconds` / `phoneme_silence_seconds` の制御
- `inspect_*` と `get_last_synthesis_result()` による dry-run / timing / 結果取得
- `custom_dictionary_path` による辞書前処理と `[[ phonemes ]]` 直入力
- `openjtalk_library_path` による `openjtalk-native` shared library 指定と builtin OpenJTalk fallback
- `execution_provider = EP_CUDA` と `gpu_device_id` による GPU device 指定
- Inspector 拡張、preset 適用、テスト発話 UI、モデル downloader、辞書 editor
- 英語 text input では `cmudict_data.json` が必要です。`addons/piper_plus/dictionaries/`、モデル同梱ディレクトリ、または config 同階層を探索します。
- CUDA を使う場合は CUDA 対応 ONNX Runtime binary を別途用意してください。既定の CPU 向け binary では自動的に CPU fallback します。

## 検証状況

- 2026-03-23 時点で `ctest --test-dir build-p1-debug -C Debug --output-on-failure` は `123/123` pass
- 2026-03-23 時点で Windows の source build は `test/prepare-assets.sh` 相当で asset を同期した上で Godot headless の `test/project` を完走できます
- 2026-03-23 時点で `scripts/ci/package-addon.sh` で再現した Windows package は Godot editor/headless で `PiperTTS` を load できず、配布 package は未解決です
- Windows の headless 実行では `addons/piper_plus/bin/onnxruntime.dll` が必要です。`test/prepare-assets.sh` は `onnxruntime.windows.x86_64.dll` しか無い場合でも plain 名へ複製します
- 2026-03-23 時点で `openjtalk-native` 無効パス時の builtin fallback と `gpu_device_id` の GDScript test を追加済みです。compiled `naist-jdic` が無い環境では builtin fallback の日本語 test は skip されます
- Linux の headless CI はありますが、現状は all-skip でも green になり得るため、package/load 失敗を完全には検出できません

## サポートプラットフォーム

| プラットフォーム | アーキテクチャ | 状態 | 備考 |
|----------------|-------------|------|------|
| Windows | x86_64 | source build で動作確認済み | packaged addon は debug binary / dependency package が未解決 |
| Linux | x86_64 | CI build と headless integration あり | all-skip でも green になり得る |
| macOS | arm64 (Apple Silicon) | CI build と C++ test のみ | Godot runtime / packaged addon は未検証 |
| Android | arm64-v8a | CI build のみ | Godot runtime / export は未検証 |
| iOS | arm64 | CI build のみ | Godot runtime / export は未検証 |
| Web | - | 未対応 | `web.*` GDExtension entry と build/export 導線がありません |

release package をそのまま「全 platform で使える addon」とみなせる状態ではまだありません。現状の最優先は package と platform verification の是正です。

## 対応モデル

モデルは `addons/piper_plus/models/` へ手動配置するか、エディタの downloader から取得する前提です。`model_path` には `.onnx` の実ファイルだけでなく、登録済みモデル名やエイリアスも指定できます。`config_path` を省略した場合は `<model>.json`、次に同じディレクトリの `config.json` を探索します。

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
    • bilingual / multilingual: Unicode ベースの ja/en 分割
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
