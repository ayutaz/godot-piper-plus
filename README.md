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
- `language_id` / `language_code` 指定とモデル名/エイリアス解決
- Prosody（韻律）サポート：より自然なイントネーション（A1/A2/A3）
- 同期 / 非同期 / streaming 合成
- カスタム辞書エディタと runtime 適用（`custom_dictionary_path`）
- オフライン動作（クラウド不要）

## 現在の対応状況

現時点で `P0` は完了です。現実装のスコープは次のとおりです。

- 日本語 OpenJTalk と英語 CMU 辞書ベース G2P を使った text input 合成
- bilingual / multilingual モデルに対する `ja/en` 最小ルーティング
- `language_id` と `language_code` による言語選択
- `model_path` の実ファイル指定、モデル名/エイリアス解決、`config_path` fallback
- `custom_dictionary_path` による辞書前処理と `[[ phonemes ]]` 直入力
- 英語 text input では `cmudict_data.json` が必要です。`addons/piper_plus/dictionaries/`、モデル同梱ディレクトリ、または config 同階層を探索します。

## 検証状況

- 2026-03-22 時点で `ctest --test-dir build-test --output-on-failure` は `123/123` pass
- 2026-03-22 時点で Godot headless の `test/project` は `test/models/multilingual-test-medium.onnx` を使って GDScript テストを完走できます
- Windows の headless 実行では `addons/piper_plus/bin/onnxruntime.dll` が必要です。`test/prepare-assets.sh` は `onnxruntime.windows.x86_64.dll` しか無い場合でも plain 名へ複製します

## サポートプラットフォーム

| プラットフォーム | アーキテクチャ | 状態 |
|----------------|-------------|------|
| Windows | x86_64 | CI ビルド対象 |
| Android | arm64-v8a | CI ビルド対象 |
| iOS | arm64 | CI ビルド対象 |
| Linux | x86_64 | CI ビルド対象 |
| macOS | arm64 (Apple Silicon) | CI ビルド対象 |

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
    • 日本語: OpenJTalk
    • 英語: CMU辞書ベースの英語 G2P
    • bilingual / multilingual: Unicode ベースの ja/en 分割
    ↓
音素エンコーディング（PUA マッピング）
    ↓
VITS推論（ONNX Runtime）
    ↓
AudioStreamWAV / AudioStreamGenerator 出力（22050Hz, 16bit PCM）
```

## 依存ライブラリ

| ライブラリ | 用途 | リンク方式 |
|-----------|------|-----------|
| [godot-cpp](https://github.com/godotengine/godot-cpp) | GDExtension C++バインディング | サブモジュール |
| [ONNX Runtime](https://onnxruntime.ai/) | VITS推論エンジン | 動的リンク |
| [OpenJTalk](https://open-jtalk.sourceforge.net/) | 日本語音素化 | 静的リンク |

## 関連プロジェクト

| プロジェクト | 説明 |
|-------------|------|
| [piper-plus](https://github.com/ayutaz/piper-plus) | TTSエンジン本体（C++/Python） |
| [uPiper](https://github.com/ayutaz/uPiper) | Unity向けプラグイン |
| [dot-net-g2p](https://github.com/ayutaz/dot-net-g2p) | 純C# 日英G2Pライブラリ |

## ライセンス

[Apache License 2.0](LICENSE)
