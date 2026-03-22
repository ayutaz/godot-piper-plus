# Piper Plus TTS

`Piper Plus TTS` は、`piper-plus` の VITS 音声合成を Godot 4.4 以降から使えるようにする addon です。`PiperTTS` ノードと editor ツールを含みます。

## 内容

この package には次が含まれます。

- `addons/piper_plus` 配下の `PiperTTS` GDExtension
- `plugin.cfg` で有効化する editor plugin
- モデル downloader
- カスタム辞書 editor
- Inspector 拡張とテスト発話 UI
- `res://addons/piper_plus/dictionaries/cmudict_data.json` の英語 CMU 辞書データ

この package には次は含まれません。

- 音声モデル本体 (`.onnx` / `.onnx.json`)
- 日本語 OpenJTalk 用の `naist-jdic`

plugin を有効化したあと、editor の downloader から必要な asset を追加してください。

## 導入手順

1. `addons/piper_plus` を Godot project にコピーします。
2. Godot 4.4 以降で project を開きます。
3. `Project > Project Settings > Plugins` で **Piper Plus TTS** を有効化します。
4. `Piper Plus: Download Models...` を開き、少なくとも 1 つモデルを取得します。
5. 日本語合成を使う場合は `naist-jdic` も取得します。

CI で作られる release package には `piper_plus.gdextension` が参照する native binary が含まれます。source checkout 直後は、全 platform の binary が揃っていない場合があります。

## クイックスタート

```gdscript
extends Node

@onready var player: AudioStreamPlayer = $AudioStreamPlayer

func _ready() -> void:
    var tts := PiperTTS.new()
    add_child(tts)

    tts.model_path = "res://addons/piper_plus/models/ja_JP-test-medium/ja_JP-test-medium.onnx"
    tts.config_path = "res://addons/piper_plus/models/ja_JP-test-medium/ja_JP-test-medium.onnx.json"
    tts.dictionary_path = "res://addons/piper_plus/dictionaries/open_jtalk_dic_utf_8-1.11"

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

同期合成は `synthesize(text)`、request ベース制御は `synthesize_request(request)`、raw phoneme 入力は `synthesize_phoneme_string(phoneme_string)` を使います。検査用途では `inspect_*` API を使うと音声生成なしで phoneme と timing 関連の情報を取得できます。

## Editor ツール

addon は次の editor command を登録します。

- `Piper Plus: Download Models...`
- `Piper Plus: Dictionary Editor...`
- `Piper Plus: Test Speech...`

加えて、`PiperTTS` ノードには custom Inspector が入り、preset 適用、ダウンロード導線、辞書 editor、試聴 UI を Inspector から開けます。

downloader は `res://addons/piper_plus/models/<model-name>/` と `res://addons/piper_plus/dictionaries/` に file を配置します。辞書 editor の既定保存先は `res://addons/piper_plus/dictionaries/custom_dictionary.json` です。

## モデルと辞書

現在の downloader 対象は次です。

- `ja_JP-test-medium`
- `tsukuyomi-chan`
- `en_US-ljspeech-medium`
- `naist-jdic`

補足:

- 日本語 text input には `res://addons/piper_plus/dictionaries/open_jtalk_dic_utf_8-1.11` が必要です。
- 英語 text input は同梱の `cmudict_data.json` を使います。
- モデル file 自体は release archive に同梱せず、project 側で必要なものだけ配置する前提です。

## Package メモ

- `piper_plus.gdextension` の `compatibility_minimum` は `4.4` です。
- 配布単位は `addons/piper_plus` を想定しています。
- `demo/`、`src/`、test asset などの開発用 file は packaged addon の利用には不要です。

## ライセンス

この addon は Apache License 2.0 です。詳細は `LICENSE` を参照してください。  
third-party notice は `THIRD_PARTY_LICENSES.txt` にまとめています。
