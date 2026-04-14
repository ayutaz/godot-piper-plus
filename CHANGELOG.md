# 変更履歴

このファイルでは、このリポジトリの主要な変更を追跡します。

## [Unreleased]

### 追加

- リポジトリ内で release 差分を追跡するための `CHANGELOG.md` を追加
- `addons/piper_plus` 単体で配布しやすいように package 用 README / license / third-party notice を追加
- 6 言語の sample text catalog と runtime descriptor foundation を追加し、Windows / Web の language selector と smoke を同じ正本で揃える基盤を整備
- Web / Pages の canonical 6-language smoke と public URL smoke の導線を追加
- Web 日本語向け `naist-jdic` staged asset bootstrap と runtime contract を追加
- `zh` text input 用 pinyin dictionary asset と explicit text path を追加
- `openjtalk_library_path` による `openjtalk-native` shared library の optional backend 切替
- `execution_provider = EP_CUDA` と `gpu_device_id` による CUDA 実行プロバイダ指定

### 変更

- Windows / Linux / macOS で ONNX Runtime の追加 provider runtime も addon bin へ複製するように調整
- `openjtalk-native` の読み込み失敗時に builtin OpenJTalk backend へ fallback するように調整
- GDScript parity test に `gpu_device_id` と `openjtalk-native` fallback の確認を追加
- multilingual capability matrix を `zh` text input を含む explicit-only contract に更新
- test speech UI と Pages demo が共通 catalog / descriptor から template text を読むように調整
- Web smoke は `Web` preset を canonical 6-language synthesize gate、`Web Threads` preset を English/core regression smoke とする構成へ調整
- `openjtalk_wrapper` の dictionary path snapshot と native backend access を lock 下へ寄せ、race / TOCTOU を抑制
- README、addon README、milestones、Pages 運用メモを Web 日本語 / 6-language 実装に同期

## [0.1.0] - 2026-03-22

### 追加

- Godot 4.4 向け `Piper Plus TTS` の初回 GDExtension release
- 同期 / 非同期 / streaming 合成に対応した `PiperTTS` ノード
- OpenJTalk による日本語 text input と、同梱 CMU 辞書による英語 text input
- `ja/en` 最小 multilingual ルーティング、model/config fallback、custom dictionary 対応
- モデル downloader と custom dictionary editor
- Windows x86_64 / Linux x86_64 / macOS arm64 / Android arm64 / iOS arm64 向け CI package

### 補足

- release package には addon code と native library を含みますが、voice model file は含みません
- 日本語合成には別途 `naist-jdic` が必要で、editor ツールから取得します
