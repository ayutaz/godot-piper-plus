# 変更履歴

このファイルでは、このリポジトリの主要な変更を追跡します。

## [Unreleased]

### 追加

- リポジトリ内で release 差分を追跡するための `CHANGELOG.md` を追加
- `addons/piper_plus` 単体で配布しやすいように package 用 README / license / third-party notice を追加

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
