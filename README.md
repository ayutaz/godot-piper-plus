# godot-piper-plus

`godot-piper-plus` は Godot 4.4 以降で使える、オフライン音声合成 addon です。
[`piper-plus`](https://github.com/ayutaz/piper-plus) を Godot 向け GDExtension として提供し、`PiperTTS` ノードと editor ツールでローカル TTS を組み込めます。

すぐ試したい人向け:

- 公開デモ: [https://ayutaz.github.io/godot-piper-plus/](https://ayutaz.github.io/godot-piper-plus/)
- package README: [`addons/piper_plus/README.md`](./addons/piper_plus/README.md)
- API リファレンス: [`doc_classes/PiperTTS.xml`](./doc_classes/PiperTTS.xml)

## できること

- ローカル完結のニューラル音声合成
- `ja/en/zh/es/fr/pt` の explicit text input / inspect / synthesize
- `ja/en` auto-routing と multilingual capability 確認
- 同期 / 非同期 / streaming 合成
- `inspect_*` API による dry-run / timing 取得
- model downloader、dictionary editor、Inspector 拡張、test speech UI、言語別 template text
- `execution_provider` と `gpu_device_id` による backend 切り替え

## 利用前に知っておくこと

- 音声 model は package に同梱していません
- 日本語 text input には `naist-jdic` が必要です
- downloader の既定保存先は `res://piper_plus_assets/models/` と `res://piper_plus_assets/dictionaries/` です
- asset を配置した後の runtime はローカル完結で動きます
- この repository をそのまま Godot project として開く場合の demo scene は `res://demo/main.tscn` です

## 導入手順

1. Godot 4.4 以降の project に `addons/piper_plus` を配置します。
2. `Project > Project Settings > Plugins` で **Piper Plus TTS** を有効化します。
3. `Piper Plus: Download Models...` を開き、少なくとも 1 つ model を追加します。
4. 日本語合成を使う場合は `naist-jdic` も追加します。
5. scene に `PiperTTS` を追加するか、script から `PiperTTS.new()` で生成します。

最短で試すなら英語 model が一番簡単です。英語用の `cmudict_data.json` は addon 側に同梱されています。

## 最小コード例

次の例は `AudioStreamPlayer` を 1 つ持つ scene を前提にしています。
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

- model を手動配置した場合は、`model_path` と `config_path` を実際の配置先に合わせてください。
- 日本語を使う場合は、`dictionary_path` に `res://piper_plus_assets/dictionaries/open_jtalk_dic_utf_8-1.11` を設定してください。

## モデルと辞書

model 本体は package に同梱していません。
`res://piper_plus_assets/models/` に手動配置するか、editor の downloader から取得する前提です。

まず選びやすい構成:

| 用途 | 推奨 asset | 補足 |
|---|---|---|
| 英語をすぐ試す | `en_US-ljspeech-medium` | 一番始めやすい構成 |
| 日本語を試す | `ja_JP-test-medium` | `naist-jdic` が必要 |
| 日本語の音質を上げたい | `tsukuyomi-chan` | `naist-jdic` が必要 |
| Web demo / smoke と同じ条件で試す | `multilingual-test-medium` | 公開デモで使用 |
| 日本語 text input 用辞書 | `naist-jdic` | 日本語では必須 |

現在の言語サポート:

- `ja` / `en`: preview tier。auto / explicit の text input をサポート
- `zh` / `es` / `fr` / `pt`: experimental explicit-only。`language_code` または `language_id` の明示指定を前提に text input をサポート
- Windows / Web の repo 実装では、6 言語 selector / template text / smoke を shared catalog と descriptor foundation で揃える構成を採っています

より厳密な正本は [`tests/fixtures/multilingual_capability_matrix.json`](./tests/fixtures/multilingual_capability_matrix.json) と [`tests/fixtures/multilingual_sample_text_catalog.json`](./tests/fixtures/multilingual_sample_text_catalog.json) です。runtime descriptor foundation は [`addons/piper_plus/model_descriptors/multilingual-test-medium.json`](./addons/piper_plus/model_descriptors/multilingual-test-medium.json) です。人が読む資料は [`docs/generated/multilingual_capability_matrix.md`](./docs/generated/multilingual_capability_matrix.md) と [`docs/generated/multilingual_sample_text_catalog.md`](./docs/generated/multilingual_sample_text_catalog.md) を参照してください。

## サポート状況

現時点のユーザー向け目安です。詳細な検証履歴と最新の正本は [`docs/milestones.md`](./docs/milestones.md) を参照してください。

| プラットフォーム | 状態 | 補足 |
|---|---|---|
| Windows | 確認済み | source build と packaged addon smoke を確認済み |
| Linux | 確認済み | CI build と headless integration を継続実行中 |
| macOS | 確認済み | packaged addon smoke を CI で確認済み |
| Android | 進行中 | export smoke は確認済み。残りは runtime 可否と Windows local export 差分 |
| iOS | 確認済み | export / link smoke を CI で確認済み |
| Web export | preview support | browser smoke は `no-threads` preset で canonical 6-language synthesize gate、`Web Threads` preset で non-blocking な English/core regression smoke を回す構成です。repo では 6-language selector / template text / Japanese dictionary staging / local-public smoke loop まで実装済みで、残りは workflow 実証と public deploy の確定です。custom template と `EP_CPU` 前提 |

## GitHub Pages 公開デモ

公開デモは GitHub Pages で公開中です。

- URL: [https://ayutaz.github.io/godot-piper-plus/](https://ayutaz.github.io/godot-piper-plus/)
- 現在の公開 URL は `main` に deploy 済みの artifact を配信します
- repo 側の Pages demo 実装は canonical 6-language selector / template text、shared descriptor foundation、staged `naist-jdic`、`ja/en/zh/es/fr/pt` の local / public smoke loop まで拡張済みです
- 同梱 model: `multilingual-test-medium`
- 日本語 text input は staged `naist-jdic` を使います

公開デモは addon の雰囲気をすぐ確認するための入口です。addon 自体の Web export はまだ preview support で、public URL の live scope は最新 deploy 結果に従います。

## Web export

addon 自体の Web export は preview support です。

- custom Web export template が必要です
- toolchain は Godot 4.4.1 向けに `emsdk 3.1.62` を前提にしています
- `execution_provider` は `EP_CPU` 固定です
- `openjtalk-native` shared library は Web では使えません
- self-hosting する場合は `COOP` / `COEP` 付き static server か、同等の cross-origin isolation workaround が必要です
- local browser smoke は `GODOT=/path/to/godot bash scripts/ci/export-web-smoke.sh build/web-smoke` で再現できます。既定では `Web` preset が `ja/en/zh/es/fr/pt` の synthesize gate、`Web Threads` preset が `en` の non-blocking regression smoke を実行します。Node.js と Playwright が必要です
- Pages demo の local smoke は `node scripts/ci/run-pages-demo-smoke.mjs --root <site_dir> --scenario <language_code>` で再現できます。scenario は sample text catalog の 6 言語と同じです

公開デモの運用メモは [`docs/web-github-pages-plan.md`](./docs/web-github-pages-plan.md) を参照してください。

## Editor ツール

addon は次の editor command を提供します。

- `Piper Plus: Download Models...`
- `Piper Plus: Dictionary Editor...`
- `Piper Plus: Test Speech...`

`PiperTTS` ノードには custom Inspector が入り、preset 適用、ダウンロード導線、辞書編集、試聴 UI を Inspector から開けます。

## 主な API

最初に使うことが多い API は次のとおりです。

- `initialize()`
- `synthesize(text)` / `synthesize_async(text)`
- `synthesize_streaming(text, playback)`
- `inspect_text(text)` / `inspect_request(request)`
- `get_last_error()` / `get_language_capabilities()`

詳しい API は [`doc_classes/PiperTTS.xml`](./doc_classes/PiperTTS.xml) を参照してください。

## package に含まれるもの / 含まれないもの

含まれるもの:

- `addons/piper_plus` 配下の GDExtension と editor plugin
- 英語用 `cmudict_data.json`
- Web preview 用 `web.*` entry

含まれないもの:

- 音声 model (`.onnx` / `.onnx.json`)
- 日本語用 `naist-jdic`
- `openjtalk-native` 本体

## 既知の制約

- Android は CI export smoke まで確認済みですが、runtime 可否の最終確認が残っています
- Windows local の Android headless export では generic configuration error の切り分けが残っています
- Web export は preview support です。正式サポートではありません
- multilingual auto-routing は `ja/en` が中心です。`zh/es/fr/pt` は explicit selection を前提とする experimental tier です
- Windows packaged addon の 6 言語 smoke と Web / Pages workflow 実証は継続中です

## 詳しい資料

- package 利用者向け: [`addons/piper_plus/README.md`](./addons/piper_plus/README.md)
- API: [`doc_classes/PiperTTS.xml`](./doc_classes/PiperTTS.xml)
- 進捗とサポート状況: [`docs/milestones.md`](./docs/milestones.md)
- チケット一覧: [`docs/tickets/README.md`](./docs/tickets/README.md)
- Pages 公開メモ: [`docs/web-github-pages-plan.md`](./docs/web-github-pages-plan.md)
- runtime descriptor: [`addons/piper_plus/model_descriptors/multilingual-test-medium.json`](./addons/piper_plus/model_descriptors/multilingual-test-medium.json)
- 言語 contract: [`docs/generated/multilingual_capability_matrix.md`](./docs/generated/multilingual_capability_matrix.md) と [`docs/generated/multilingual_sample_text_catalog.md`](./docs/generated/multilingual_sample_text_catalog.md)
- CI / CD: [`.github/workflows/build.yml`](./.github/workflows/build.yml) と [`.github/workflows/pages.yml`](./.github/workflows/pages.yml)

## 関連プロジェクト

- [piper-plus](https://github.com/ayutaz/piper-plus)
- [uPiper](https://github.com/ayutaz/uPiper)
- [dot-net-g2p](https://github.com/ayutaz/dot-net-g2p)

## ライセンス

[Apache License 2.0](./LICENSE)
