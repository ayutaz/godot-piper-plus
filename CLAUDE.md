# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## プロジェクト概要

godot-piper-plusは、[piper-plus](https://github.com/ayutaz/piper-plus)（VITS ニューラルTTSエンジン）をGodotエンジン向けにGDExtensionとして移植するプロジェクト。Windows、Android、iOSに対応する。

Unity向け移植 [uPiper](https://github.com/ayutaz/uPiper) (`/Users/s19447/Documents/uPiper`) が既に完成しており、そのアーキテクチャと設計パターンを参考にする。uPiper v1.2.0でネイティブOpenJTalkは完全削除され、dot-net-g2p（純C#）に移行済み。

### 参照プロジェクト

| プロジェクト | パス / URL | 役割 |
|-------------|-----------|------|
| piper-plus | `/Users/s19447/Documents/piper-plus` | TTSエンジン本体（C++/Python） |
| uPiper | `/Users/s19447/Documents/uPiper` | Unity向け移植（参考実装） |
| dot-net-g2p | https://github.com/ayutaz/dot-net-g2p | 純C# 日英G2Pライブラリ（uPiperに統合済み） |
| godot-piper-plus | このリポジトリ | Godot向け移植（開発中） |

## アーキテクチャ設計方針

### 採択方針: piper-plusの既存C++コード活用 + dot-net-g2pの設計を参考

G2P（音素変換）のアプローチとして**piper-plusの既存C++コードを活用**する方式を採択。

**選定理由:**
- GDExtension自体がC++のため、OpenJTalkのC/C++ライブラリを静的リンクするだけで統合完了（uPiperでのP/Invoke問題が存在しない）
- piper-plusに`openjtalk_phonemize.cpp`(193行) + `custom_dictionary.cpp`(363行)が既存
- dot-net-g2pをC++に全移植（推定6,000-10,000行/4-6週間）は費用対効果が低い
- naist-jdic辞書はどちらの方式でも必要

**注:** uPiper v1.2.0ではネイティブOpenJTalkを完全削除しdot-net-g2p（純C#）に移行済みだが、その理由はP/Invoke管理コストとプラットフォーム別ネイティブビルドの分離にあり、C++ネイティブであるGDExtensionでは当てはまらない。

### データフロー

```
テキスト入力
    ↓
カスタム辞書による前処理（piper-plus custom_dictionary.cpp）
    ↓
Phonemizer（音素変換）
    • 日本語: OpenJTalk（piper-plus openjtalk_phonemize.cpp、静的リンク）
    • 英語: Flite LTS + CMU辞書（dot-net-g2p DotNetG2P.Englishの設計を参考にC++実装）
    ↓
音素エンコーディング（PUA マッピング）
    ↓
[Prosody対応モデルの場合: A1/A2/A3抽出]
    ↓
VITS推論（ONNX Runtime）
    ↓
AudioStreamWAV出力（22050Hz, 16bit PCM）
```

### dot-net-g2pから取り込む追加機能

uPiperのDotNetG2PPhonemizer実装で追加された機能をC++側に移植する：

- **N音素の文脈依存ルール**: N_m（両唇音前）, N_n（歯茎音前）, N_ng（軟口蓋音前）, N_uvular（文末/母音前）
- **疑問符タイプ検出**: `?`(通常), `?!`(強調), `?.`(平叙), `?~`(確認), `$`(文末)
- **日英混在テキスト対応**: LanguageDetector + TextSegmenter相当のC++実装

### dot-net-g2pパッケージ構成（設計参考）

| パッケージ | 内容 |
|-----------|------|
| `DotNetG2P.Core` | 日本語G2Pエンジン（NJD 6段階パイプライン、音素変換、Prosody抽出） |
| `DotNetG2P.MeCab` | 純C# MeCab互換形態素解析エンジン（外部依存なし） |
| `DotNetG2P.English` | 英語G2P（CMU辞書135K語 + Flite LTS CARTツリーによるOOV推定） |
| `DotNetG2P.Multilingual` | 日英混在テキスト対応（Unicode文字種ベース言語判定） |

**日本語G2P処理パイプライン（OpenJTalk互換、dot-net-g2pの設計）:**
```
テキスト → TextNormalizer（全角/半角正規化）
    → MeCabTokenizer（形態素解析、naist-jdic辞書）
    → SetPronunciation（発音設定）
    → SetDigit（数字読み変換）
    → SetAccentPhrase（アクセント句結合、18ルール）
    → SetAccentType（アクセント結合型、C1-C5/F1-F5/P系列）
    → SetUnvoicedVowel（無声音化、6ルール）
    → 音素列出力
```

### Godot固有の設計

- **GDExtension (C++)**: ネイティブパフォーマンスのためGDExtensionを使用
- **ONNX推論**: Godotには組み込みNNランタイムがないため、ONNX Runtimeを直接リンク
- **音声出力**: Unity の `AudioClip` → Godot の `AudioStreamWAV` / `AudioStreamGenerator`
- **リソース管理**: Godotの `res://` / `user://` パスシステムに適応

### GDExtension バイナリ構成

GDExtensionの単一共有ライブラリ内にOpenJTalkを静的リンクし、ONNX Runtimeのみ動的依存とする（英語G2PはFlite LTS + CMU辞書をC++で自前実装、espeak-ng/piper-phonemizeはGPL-3.0ライセンス汚染のため不使用）：

| プラットフォーム | GDExtension出力 | ONNX Runtime | ビルドツール |
|----------------|----------------|-------------|------------|
| Windows x86_64 | `libpiper_plus.dll` | `onnxruntime.dll` (動的) | SCons/CMake + MSVC |
| Android arm64-v8a | `libpiper_plus.so` | `libonnxruntime.so` (動的) | SCons/CMake + NDK |
| iOS arm64 | `libpiper_plus.a` (静的) | 静的リンク | SCons/CMake + Xcode |
| Linux x86_64 | `libpiper_plus.so` | `libonnxruntime.so` (動的) | SCons/CMake |
| macOS arm64 | `libpiper_plus.framework` | framework内 | SCons/CMake |

**.gdextension依存ライブラリ設定（ONNX Runtime）:**
```ini
[dependencies]
windows.release = { "res://addons/piper_plus/bin/onnxruntime.dll" : "" }
linux.release = { "res://addons/piper_plus/bin/libonnxruntime.so" : "" }
```

### piper-plus C++ API（`piper.hpp`）

推論に必要な主要関数：
```cpp
void initialize(PiperConfig &config);
void terminate(PiperConfig &config);
void loadVoice(PiperConfig &config, std::string modelPath, std::string modelConfigPath, Voice &voice, std::optional<SpeakerId> &speakerId, bool useCuda);
void textToAudio(PiperConfig &config, Voice &voice, std::string text, std::vector<int16_t> &audioBuffer, SynthesisResult &result, const std::function<void()> &audioCallback);
```

### piper-plus 既存のG2P関連C++コード

| ファイル | 行数 | 内容 |
|---------|------|------|
| `openjtalk_phonemize.cpp/hpp` | 243行 | OpenJTalk C API呼び出し、PUA変換 |
| `custom_dictionary.cpp/hpp` | 450行 | JSON辞書解析、テキスト前処理 |
| `phoneme_parser.cpp/hpp` | - | インライン音素パーサ `[[ phonemes ]]` |
| `openjtalk_wrapper.c/h` | - | OpenJTalk Cバインディング |

Prosody結果（A1/A2/A3）: アクセント核からの相対位置、アクセント句内位置、アクセント句内モーラ数。

## ビルドシステム

### GDExtension構成（予定）

```
godot-piper-plus/
├── src/                        # GDExtension C++ソース
│   ├── register_types.cpp      # GDExtension登録
│   ├── piper_tts.cpp/h         # メインTTSノード
│   ├── openjtalk_phonemizer.cpp/h  # 日本語音素化
│   └── phoneme_encoder.cpp/h   # 音素エンコーダ
├── thirdparty/                 # 外部依存
│   ├── godot-cpp/              # godot-cpp（git submodule）
│   ├── onnxruntime/            # ONNX Runtimeヘッダ・ライブラリ
│   └── openjtalk/              # OpenJTalkラッパー
├── addons/piper_plus/          # Godotアドオン
│   ├── bin/                    # プラットフォーム別ビルド済バイナリ
│   ├── models/                 # ONNXモデル
│   └── dictionaries/           # OpenJTalk辞書・カスタム辞書
├── SConstruct                  # SCons ビルドスクリプト
└── demo/                       # デモプロジェクト
```

### ビルドコマンド（予定）

```bash
# godot-cppのビルド
cd thirdparty/godot-cpp && scons platform=<platform> target=template_release

# GDExtensionビルド
scons platform=<platform> target=template_release

# platform: windows, linux, macos, android, ios
```

### 依存ライブラリ

| ライブラリ | 用途 | リンク方式 | バージョン |
|-----------|------|-----------|-----------|
| godot-cpp | GDExtension C++バインディング | サブモジュール | Godot 4.x対応 |
| ONNX Runtime | VITS推論エンジン | **動的** (.gdextension dependencies) | >= 1.16.0 |
| OpenJTalk | 日本語音素化 | **静的** (GDExtensionに組込) | 1.11 |
| HTSEngine | 音声合成エンジンコア | **静的** | 1.10 |
| Flite LTS + CMU辞書 | 英語音素化 | **静的**（自前C++実装） | - |
| nlohmann/json | JSON設定ファイル解析 | header-only | - |

## uPiper（Unity版 v1.2.0）との対応関係

uPiperはv1.2.0でネイティブOpenJTalkを完全削除しdot-net-g2p（純C#）に移行済み。Godot版はGDExtension（C++）のため、piper-plusの既存C++コードでOpenJTalkを直接静的リンクする方式を採用。

| uPiper v1.2.0 (C#/Unity) | godot-piper-plus (C++/Godot) | 備考 |
|--------------------------|------------------------------|------|
| `PiperTTS` MonoBehaviour | `PiperTTS` Node (GDExtension) | |
| `DotNetG2PPhonemizer` (純C# G2P) | piper-plus `openjtalk_phonemize.cpp` (C++ + OpenJTalk静的リンク) | uPiperは純C#、Godotは既存C++ |
| `DotNetG2P.MeCab` (純C# MeCab) | OpenJTalk内蔵MeCab（静的リンク） | |
| `PhonemeEncoder` + `OpenJTalkToPiperMapping` | `PhonemeEncoder` (C++) | PUAマッピング共通 |
| `InferenceAudioGenerator` (Unity.InferenceEngine) | ONNX Runtime直接呼び出し | |
| `AudioChunkBuilder` | `AudioStreamWAV` / `AudioStreamGenerator` | |
| `CustomDictionary` | piper-plus `custom_dictionary.cpp` | |
| `UnifiedPhonemizer` (言語自動選択) | C++実装（dot-net-g2p Multilingualを参考） | |
| `StreamingAssets/` | `res://addons/piper_plus/` | |

## 重要な技術詳細

### PUA（Private Use Area）音素マッピング

日本語の複数文字音素をUnicode PUA文字にマッピング：
- 拗音子音: `ky` → `\ue006`, `ch` → `\ue00e`, `ts` → `\ue00f`, `sh` → `\ue010` 等
- N音素文脈依存（dot-net-g2pで追加）: `N_m` → `\ue019`, `N_n` → `\ue01a`, `N_ng` → `\ue01b`, `N_uvular` → `\ue01c`
- モデルの`phoneme_id_map`でPUAキーの有無により自動判定

### 対応モデル

| モデル名 | 言語 | Prosody | 用途 |
|---------|------|---------|------|
| ja_JP-test-medium | 日本語 | No | 標準日本語 |
| tsukuyomi-chan | 日本語 | Yes | 高品質日本語（A1/A2/A3） |
| en_US-ljspeech-medium | 英語 | No | 標準英語 |

### プラットフォーム固有の注意点

- **iOS**: 静的ライブラリのみ（動的リンク不可）、arm64のみ
- **Android**: ABIごとにビルドが必要（arm64-v8a, armeabi-v7a, x86, x86_64）、`-Os`でサイズ最適化
- **Windows**: MSVC動的ランタイム、UTF-8エンコーディング設定必要
- **macOS**: Apple Siliconのみ（Intel非推奨）

## ライセンス

Apache License 2.0
