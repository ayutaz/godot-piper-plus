# CLAUDE.md

このファイルは、このリポジトリで Claude Code が作業する際のガイドです。

## プロジェクト概要

godot-piper-plus は、[piper-plus](https://github.com/ayutaz/piper-plus) を Godot 向け GDExtension として移植するプロジェクトです。ローカル実行の VITS TTS を Godot から扱えるようにします。

参照実装として [uPiper](https://github.com/ayutaz/uPiper) と [dot-net-g2p](https://github.com/ayutaz/dot-net-g2p) を見ることはありますが、現実装の中心は piper-plus の C++ コア再利用です。

## 現在の状態

- `P0` は完了扱いです。
- `P1` は機能実装まで完了していますが、package / platform verification の follow-up が残っています。
- `P2` は repo 内実装まで完了しています。
- text input 合成は、日本語 OpenJTalk、英語 CMU 辞書ベース G2P、`ja/en` 最小 multilingual runtime が入っています。
- `custom_dictionary_path` は runtime 接続済みです。
- `language_id` / `language_code` は `PiperTTS` から公開済みです。
- `model_path` は実ファイル指定に加えて、登録済みモデル名やエイリアス解決に対応しています。
- `config_path` は省略可能で、`<model>.json` と同一ディレクトリの `config.json` を順に探索します。
- `synthesize_request` / `inspect_request`、raw phoneme 入力、timing 出力、silence override が使えます。
- `openjtalk-native` は `openjtalk_wrapper.*` を境界に optional backend として読み込めます。無効 path 時は builtin OpenJTalk に fallback します。
- `execution_provider = EP_CUDA` と `gpu_device_id` による GPU device 指定が使えます。CUDA 対応 ONNX Runtime が無い場合は CPU fallback します。
- editor には downloader、dictionary editor、Inspector 拡張、test speech UI があります。
- 英語 text input では `cmudict_data.json` が必要で、モデル同梱、config 同階層、`addons/piper_plus/dictionaries` を探索します。
- C++ テストは `123/123` pass です。
- Windows の source build は、addon bin と `test/models` を同期した状態で Godot headless の `test/project` を完走できます。
- ただし packaged addon は 2026-03-23 時点で Windows editor/headless load failure を再現しており、debug binary / runtime dependency の package 導線が未解決です。
- Linux の Godot headless CI はありますが、現状は all-skip でも green になり得ます。
- macOS は arm64 build と C++ test まで、Android/iOS は build までで、Godot runtime / export 検証は未実施です。
- Web は現状サポート対象外です。
- builtin OpenJTalk fallback の日本語テストは compiled `naist-jdic` が無い環境では skip されます。

## 設計方針

### コア方針

- piper-plus の既存 C++ 実装を優先して再利用する
- GDExtension が C++ なので、日本語 G2P は OpenJTalk 静的リンクを前提にする
- GPL 系依存は持ち込まない
- 英語 G2P は CMU 辞書ベースの自前 C++ 実装で扱う
- multilingual は `ja/en` 最小構成を維持し、追加 parity 拡張は別タスクとして扱う
- `openjtalk-native` を使う場合も `openjtalk_wrapper.*` を安定境界として backend 側で吸収する

### 現行データフロー

```text
テキスト入力
    ↓
[任意] custom_dictionary_path による前処理
    ↓
[[ phonemes ]] 直入力のパース
    ↓
Phonemizer
    • 日本語: builtin OpenJTalk または openjtalk-native
    • 英語: CMU 辞書ベース英語 G2P
    • bilingual / multilingual: Unicode ベースの ja/en 分割
    ↓
音素エンコーディング
    ↓
[必要時] speaker_id / language_id / prosody を付与
    ↓
ONNX Runtime で VITS 推論
    • CPU / CoreML / DirectML / NNAPI / CUDA
    ↓
AudioStreamWAV / AudioStreamGenerator 出力
```

## 主なコード配置

```text
src/
├── register_types.cpp
├── piper_tts.cpp / piper_tts.h
├── audio_queue.h
└── piper_core/
    ├── piper.cpp / piper.hpp
    ├── piper_test_utils.cpp / piper_test_utils.hpp
    ├── openjtalk_phonemize.cpp / openjtalk_phonemize.hpp
    ├── english_phonemize.cpp / english_phonemize.hpp
    ├── language_detector.cpp / language_detector.hpp
    ├── multilingual_phonemize.cpp / multilingual_phonemize.hpp
    ├── custom_dictionary.cpp / custom_dictionary.hpp
    ├── phoneme_parser.cpp / phoneme_parser.hpp
    ├── phoneme_ids.hpp
    └── openjtalk_*.c / *.h

addons/piper_plus/
├── bin/
├── dictionaries/
└── *.gd / *.gdextension

tests/
├── test_piper_core.cpp
├── test_english_phonemize.cpp
├── test_language_detector.cpp
├── test_phoneme_parser.cpp
└── ...

test/project/
└── test_piper_tts.gd
```

## ビルドと検証

### CMake

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
```

### テスト

```bash
cmake -B build-test -DBUILD_TESTS=ON
cmake --build build-test --target piper_plus_tests
ctest --test-dir build-test --output-on-failure
```

### Godot headless

```bash
godot --headless --path test/project
```

注記:
- `test/prepare-assets.sh` か同等の手順で、`addons/piper_plus/bin/` と `test/models/` を `test/project` 側へ同期してから実行する
- 2026-03-23 時点では Windows source build + asset 同期済み環境で `multilingual-test-medium.onnx` を使って `test_piper_tts.gd` が完走する
- 現状の CI 判定は all-skip を failure と見なしていないため、package/load failure を完全には検出できない
- builtin OpenJTalk fallback の日本語ケースは compiled `naist-jdic` が無い場合 skip される

## 現在の優先タスク

優先順位は [docs/milestones.md](C:/Users/yuta/Desktop/Private/godot-piper-plus/docs/milestones.md) を基準にする。

- `R1` package に debug/editor binary を含める
- `R2` ONNX Runtime sidecar dependency を package へ反映する
- `R3` all-skip CI を failure 扱いにする
- `R4` Windows packaged addon smoke test
- `P1-8` Asset Library 登録

## 参照プロジェクト

| プロジェクト | パス / URL | 用途 |
|-------------|-----------|------|
| piper-plus | `../piper-plus` / https://github.com/ayutaz/piper-plus | upstream 本体 |
| uPiper | https://github.com/ayutaz/uPiper | Unity 向け参照実装 |
| dot-net-g2p | https://github.com/ayutaz/dot-net-g2p | 日英 G2P 設計の参照 |

## ライセンス

Apache License 2.0
