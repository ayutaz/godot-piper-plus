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
- `openjtalk-native` を任意で使うための `openjtalk_library_path` 導線
- CUDA / `gpu_device_id` を指定するための runtime property
- Web preview support 用の `web.*` GDExtension entry

この package には次は含まれません。

- 音声モデル本体 (`.onnx` / `.onnx.json`)
- 日本語 OpenJTalk 用の `naist-jdic`
- `openjtalk-native` 本体 DLL / `.so` / `.dylib`
- Web 用 `openjtalk-native` shared library

plugin を有効化したあと、editor の downloader から必要な asset を追加してください。

## 導入手順

1. `addons/piper_plus` を Godot project にコピーします。
2. Godot 4.4 以降で project を開きます。
3. `Project > Project Settings > Plugins` で **Piper Plus TTS** を有効化します。
4. `Piper Plus: Download Models...` を開き、少なくとも 1 つモデルを取得します。
5. 日本語合成を使う場合は `naist-jdic` も取得します。

## Web Preview Support

Web は preview support です。

- custom Web export template が必要です
- toolchain は Godot 4.4.1 向けに `emsdk 3.1.62` を前提にしています
- `execution_provider` は `EP_CPU` 固定です
- `openjtalk-native` shared library は Web では使えません
- `COOP` / `COEP` 付きの static server 上で確認してください

ローカル smoke は `scripts/ci/export-web-smoke.sh` を使います。Node.js と Playwright が必要なので、事前に `npm install --no-save playwright` と `npx playwright install chromium` を実行してください。

2026-04-10 の GitHub Actions run `24223195868` では、Web browser smoke、macOS packaged addon smoke、Android export smoke、iOS export smoke が成功しています。
source checkout 直後は全 platform の binary が揃っていないため、local では source build または package 内容の個別確認を前提に扱ってください。Android の runtime 可否と Windows local export error は継続確認中です。

## クイックスタート

```gdscript
extends Node

@onready var player: AudioStreamPlayer = $AudioStreamPlayer

func _ready() -> void:
    var tts := PiperTTS.new()
    add_child(tts)

    tts.model_path = "res://piper_plus_assets/models/ja_JP-test-medium/ja_JP-test-medium.onnx"
    tts.config_path = "res://piper_plus_assets/models/ja_JP-test-medium/ja_JP-test-medium.onnx.json"
    tts.dictionary_path = "res://piper_plus_assets/dictionaries/open_jtalk_dic_utf_8-1.11"
    # 任意: openjtalk-native を使う場合
    # tts.openjtalk_library_path = "res://addons/piper_plus/bin/openjtalk_native.dll"
    # 任意: CUDA 対応 ONNX Runtime を配置している場合
    # tts.execution_provider = PiperTTS.EP_CUDA
    # tts.gpu_device_id = 0

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

multilingual text input の現状サポートは capability matrix 基準です。

- `ja` / `en`: preview auto / explicit 対応
- `es` / `fr` / `pt`: experimental explicit-only adapter
- `zh`: phoneme-only。text input では使えません。raw phoneme input を使ってください

正本は `tests/fixtures/multilingual_capability_matrix.json`、人が読む投影は `docs/generated/multilingual_capability_matrix.md` です。詳細な support matrix はそちらを参照してください。
runtime では `get_language_capabilities()` で loaded model の contract を取得でき、validation / routing failure は `get_last_error()` から machine-readable に確認できます。

## Editor ツール

addon は次の editor command を登録します。

- `Piper Plus: Download Models...`
- `Piper Plus: Dictionary Editor...`
- `Piper Plus: Test Speech...`

加えて、`PiperTTS` ノードには custom Inspector が入り、preset 適用、ダウンロード導線、辞書 editor、試聴 UI を Inspector から開けます。

downloader は既定で `res://piper_plus_assets/models/<model-name>/` と `res://piper_plus_assets/dictionaries/` に file を配置します。legacy fallback として `addons/piper_plus` 側の既存 asset も解決できます。辞書 editor の既定保存先は `res://piper_plus_assets/dictionaries/custom_dictionary.json` です。

## モデルと辞書

現在の downloader 対象は次です。

- `ja_JP-test-medium`
- `tsukuyomi-chan`
- `en_US-ljspeech-medium`
- `naist-jdic`

補足:

- 日本語 text input には `res://piper_plus_assets/dictionaries/open_jtalk_dic_utf_8-1.11` が必要です。legacy fallback として `addons/piper_plus/dictionaries/open_jtalk_dic_utf_8-1.11` も解決します。
- 英語 text input は同梱の `cmudict_data.json` を使います。
- multilingual text input で `es` / `fr` / `pt` を使う場合は experimental explicit-only adapter です。
- `zh` は multilingual model の capability には残せますが、text input では phonemize できません。
- モデル file 自体は release archive に同梱せず、project 側で必要なものだけ配置する前提です。
- `openjtalk_library_path` を空にするか無効な path を指定した場合は builtin OpenJTalk backend へ戻ります。
- `EP_CUDA` は CUDA 対応 ONNX Runtime binary がある環境でのみ有効で、使えない場合は CPU fallback します。

## Package メモ

- `piper_plus.gdextension` の `compatibility_minimum` は `4.4` です。
- 配布単位は `addons/piper_plus` を想定しています。
- `demo/`、`src/`、test asset などの開発用 file は packaged addon の利用には不要です。
- package script は `piper_plus.gdextension` の全 `bin` 参照を正として assemble します。
- validator は desktop / mobile の debug / release binary を含めて manifest 整合を確認します。
- Windows packaged addon smoke は local で再確認済みです。2026-04-10 の CI では macOS packaged smoke、Android export smoke、iOS export smoke、Web browser smoke も成功しています。残っているのは Android runtime と Windows local export 差分の整理です。

## ライセンス

この addon は Apache License 2.0 です。詳細は `LICENSE` を参照してください。  
third-party notice は `THIRD_PARTY_LICENSES.txt` にまとめています。
