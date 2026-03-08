# godot-piper-plus

[piper-plus](https://github.com/ayutaz/piper-plus)のGodotプラグイン - 高品質なニューラル音声合成エンジン

## 概要

godot-piper-plusは、[piper-plus](https://github.com/ayutaz/piper-plus)（VITS ニューラルTTSエンジン）をGodotエンジン向けにGDExtensionとして移植するプロジェクトです。ローカルで動作する高品質な音声合成をGodotゲームに組み込むことができます。

## 機能

- 高品質なニューラル音声合成（VITS / piper-plusベース）
- 多言語対応（日本語、英語）
- GDExtension（C++）によるネイティブパフォーマンス
- OpenJTalkによる高精度な日本語音素化
- Prosody（韻律）サポート：より自然なイントネーション（A1/A2/A3）
- カスタム辞書：技術用語・固有名詞の読み変換
- オフライン動作（クラウド不要）

## サポートプラットフォーム

| プラットフォーム | アーキテクチャ | 状態 |
|----------------|-------------|------|
| Windows | x86_64 | ビルド済 |
| Android | arm64-v8a | ビルド済 |
| iOS | arm64 | ビルド済 |
| Linux | x86_64 | ビルド済 |
| macOS | arm64 (Apple Silicon) | ビルド済 |

## 対応モデル

| モデル名 | 言語 | Prosody対応 | 説明 |
|---------|------|------------|------|
| ja_JP-test-medium | 日本語 | No | 標準日本語モデル |
| en_US-ljspeech-medium | 英語 | No | 標準英語モデル（英語G2P未実装） |
| tsukuyomi-chan | 日本語 | Yes | Prosody対応日本語モデル（より自然なイントネーション） |

## アーキテクチャ

```
テキスト入力
    ↓
カスタム辞書による前処理
    ↓
Phonemizer（音素変換）
    • 日本語: OpenJTalk（静的リンク）
    • 英語: 未実装（Flite LTS + CMU辞書で実装予定）
    ↓
音素エンコーディング（PUA マッピング）
    ↓
VITS推論（ONNX Runtime）
    ↓
AudioStreamWAV出力（22050Hz, 16bit PCM）
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
