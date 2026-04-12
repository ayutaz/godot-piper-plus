# Piper Plus TTS

`Piper Plus TTS` は Godot 4.4 以降向けの音声合成 addon です。
`PiperTTS` ノード、editor plugin、model downloader、dictionary editor、test speech UI を含みます。

この README は `addons/piper_plus` を package として使う人向けです。repository 全体の説明は [README.md](../../README.md) を参照してください。

## この package に含まれるもの

- `PiperTTS` GDExtension
- editor plugin
- model downloader
- custom dictionary editor
- Inspector 拡張と test speech UI
- 英語用 `cmudict_data.json`
- `openjtalk-native` を任意で使うための `openjtalk_library_path` 導線
- CUDA / `gpu_device_id` 用の runtime property
- Web preview 用の `web.*` GDExtension entry

## この package に含まれないもの

- 音声モデル本体 (`.onnx` / `.onnx.json`)
- 日本語 OpenJTalk 用 `naist-jdic`
- `openjtalk-native` 本体 DLL / `.so` / `.dylib`

plugin を有効化したあと、editor の downloader から必要な asset を追加してください。

## 導入手順

1. `addons/piper_plus` を Godot project にコピーします。
2. Godot 4.4 以降で project を開きます。
3. `Project > Project Settings > Plugins` で **Piper Plus TTS** を有効化します。
4. `Piper Plus: Download Models...` から少なくとも 1 つモデルを追加します。
5. 日本語合成を使う場合は `naist-jdic` も追加します。

## 最小コード例

英語モデルを使う構成が一番試しやすいです。
`model_path` と `config_path` は downloader の既定配置を使った場合の例です。

```gdscript
extends Node

@onready var player: AudioStreamPlayer = $AudioStreamPlayer

func _ready() -> void:
    var tts := PiperTTS.new()
    add_child(tts)

    tts.model_path = "res://piper_plus_assets/models/en_US-ljspeech-medium/en_US-ljspeech-medium.onnx"
    tts.config_path = "res://piper_plus_assets/models/en_US-ljspeech-medium/en_US-ljspeech-medium.onnx.json"

    tts.synthesis_completed.connect(func(audio: AudioStreamWAV) -> void:
        player.stream = audio
        player.play()
    )
    tts.synthesis_failed.connect(func(message: String) -> void:
        push_error(message)
    )

    var err := tts.initialize()
    if err != OK:
        push_error("PiperTTS initialize failed: %s" % err)
        return

    err = tts.synthesize_async("Hello from Piper Plus.")
    if err != OK:
        push_error("PiperTTS synthesize_async failed: %s" % err)
```

日本語を使う場合は `dictionary_path` に `res://piper_plus_assets/dictionaries/open_jtalk_dic_utf_8-1.11` を設定してください。

## Editor ツール

addon は次の editor command を登録します。

- `Piper Plus: Download Models...`
- `Piper Plus: Dictionary Editor...`
- `Piper Plus: Test Speech...`

`PiperTTS` ノードには custom Inspector が入り、preset 適用、辞書編集、ダウンロード導線、試聴 UI を Inspector から開けます。

## モデルと辞書

downloader から取得しやすい asset:

- `ja_JP-test-medium`
- `tsukuyomi-chan`
- `en_US-ljspeech-medium`
- `multilingual-test-medium`
- `naist-jdic`

補足:

- 日本語 text input には `res://piper_plus_assets/dictionaries/open_jtalk_dic_utf_8-1.11` が必要です
- 英語 text input は同梱の `cmudict_data.json` を使います
- multilingual text input の現在の扱いは `ja/en` が preview auto / explicit、`es/fr/pt` が experimental explicit-only、`zh` が phoneme-only です
- `get_language_capabilities()` と `get_last_error()` で runtime 上の能力と failure を確認できます

詳しい matrix は [docs/generated/multilingual_capability_matrix.md](../../docs/generated/multilingual_capability_matrix.md) を参照してください。

## サポート状況

現時点のユーザー向け目安です。詳細な検証履歴と最新の正本は [docs/milestones.md](../../docs/milestones.md) を参照してください。

| プラットフォーム | 状態 | 補足 |
|---|---|---|
| Windows | 確認済み | packaged addon smoke を local で再確認済み |
| Linux | 確認済み | CI build と headless integration を継続実行中 |
| macOS | 確認済み | packaged addon smoke を CI で確認済み |
| Android | 進行中 | export smoke は確認済み。残りは runtime 可否と Windows local export 差分 |
| iOS | 確認済み | export / link smoke を CI で確認済み |
| Web | preview support | browser smoke を CI で確認済み |

## Web Preview Support

Web は preview support です。

- custom Web export template が必要です
- toolchain は Godot 4.4.1 向けに `emsdk 3.1.62` を前提にしています
- `execution_provider` は `EP_CPU` 固定です
- `openjtalk-native` shared library は Web では使えません
- `COOP` / `COEP` 付き static server か、同等の cross-origin isolation workaround 上で確認してください

ローカル smoke は `scripts/ci/export-web-smoke.sh` を使います。
Node.js と Playwright が必要なので、事前に `npm install --no-save playwright` と `npx playwright install chromium` を実行してください。

## package メモ

- `piper_plus.gdextension` の `compatibility_minimum` は `4.4` です
- 配布単位は `addons/piper_plus` を想定しています
- `demo/`、`src/`、test asset などの開発用 file は packaged addon の利用には不要です
- package script は `piper_plus.gdextension` の全 `bin` 参照を正として assemble します
- validator は desktop / mobile / Web の binary と dependency の整合を確認します

source checkout 直後は全 platform の binary が揃っていないため、local では source build または package 内容の個別確認を前提に扱ってください。

## 既知の制約

- Android runtime の最終確認は継続中です
- Windows local Android export error は未整理です
- Web は preview support です
- `zh` は phoneme-only で、text input には使えません

## ライセンス

この addon は Apache License 2.0 です。詳細は `LICENSE` を参照してください。  
third-party notice は `THIRD_PARTY_LICENSES.txt` にまとめています。
