# godot-piper-plus

`godot-piper-plus` は Godot 4.4 以降で使える、オフライン音声合成 addon です。
[`piper-plus`](https://github.com/ayutaz/piper-plus) を Godot 向け GDExtension として提供し、`PiperTTS` ノードと editor ツールでローカル TTS を組み込めます。

すぐ試したい人向け:

- 公開デモ: [https://ayutaz.github.io/godot-piper-plus/](https://ayutaz.github.io/godot-piper-plus/)
- package README: [`addons/piper_plus/README.md`](./addons/piper_plus/README.md)
- API リファレンス: [`doc_classes/PiperTTS.xml`](./doc_classes/PiperTTS.xml)

## できること

- ローカル完結のニューラル音声合成
- 日本語と英語の text input
- 一部多言語 model の explicit 利用と capability 確認
- 同期 / 非同期 / streaming 合成
- `inspect_*` API による dry-run / timing 取得
- model downloader、dictionary editor、Inspector 拡張、test speech UI
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

- `ja` / `en`: text input 対応
- `es` / `fr` / `pt`: experimental。明示的な model / language 指定を前提
- `zh`: phoneme input のみ

より厳密な正本は [`tests/fixtures/multilingual_capability_matrix.json`](./tests/fixtures/multilingual_capability_matrix.json)、人が読む資料は [`docs/generated/multilingual_capability_matrix.md`](./docs/generated/multilingual_capability_matrix.md) を参照してください。

## サポート状況

現時点のユーザー向け目安です。詳細な検証履歴と最新の正本は [`docs/milestones.md`](./docs/milestones.md) を参照してください。

| プラットフォーム | 状態 | 補足 |
|---|---|---|
| Windows | 確認済み | source build と packaged addon smoke を確認済み |
| Linux | 確認済み | CI build と headless integration を継続実行中 |
| macOS | 確認済み | packaged addon smoke を CI で確認済み |
| Android | 進行中 | export smoke は確認済み。残りは runtime 可否と Windows local export 差分 |
| iOS | 確認済み | export / link smoke を CI で確認済み |
| Web export | preview support | browser smoke は `en/ja` gate に拡張済みです。main では `M9` の English minimal Pages demo を公開中で、この branch では `ja/en` public demo と Japanese smoke を追加実装し、workflow で実証中です。custom template と `EP_CPU` 前提 |

## GitHub Pages 公開デモ

公開デモは GitHub Pages で公開中です。

- URL: [https://ayutaz.github.io/godot-piper-plus/](https://ayutaz.github.io/godot-piper-plus/)
- main の公開 scope: `M9` の `no-threads` / `CPU-only` / English minimal demo
- current branch の追加 scope: `ja/en` public demo、Japanese startup self-test、public `ja` smoke の実装済み / 実証中
- 同梱 model: `multilingual-test-medium`
- 日本語 text input は staged `naist-jdic` を使います

公開デモは addon の雰囲気をすぐ確認するための入口です。addon 自体の Web export はまだ preview support です。`ja/en` 公開デモはこの branch で実装済みですが、`workflow_dispatch` / `main` deploy での実証は継続中です。

## Web export

addon 自体の Web export は preview support です。

- custom Web export template が必要です
- toolchain は Godot 4.4.1 向けに `emsdk 3.1.62` を前提にしています
- `execution_provider` は `EP_CPU` 固定です
- `openjtalk-native` shared library は Web では使えません
- self-hosting する場合は `COOP` / `COEP` 付き static server か、同等の cross-origin isolation workaround が必要です
- local browser smoke は `GODOT=/path/to/godot PIPER_WEB_SMOKE_SCENARIOS=en,ja bash scripts/ci/export-web-smoke.sh build/web-smoke` で再現できます。Node.js と Playwright が必要です
- Pages demo の local smoke は `node scripts/ci/run-pages-demo-smoke.mjs --root <site_dir> --scenario ja` で再現できます

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
- `zh` は phoneme input のみで、text input には使えません

## 詳しい資料

- package 利用者向け: [`addons/piper_plus/README.md`](./addons/piper_plus/README.md)
- API: [`doc_classes/PiperTTS.xml`](./doc_classes/PiperTTS.xml)
- 進捗とサポート状況: [`docs/milestones.md`](./docs/milestones.md)
- チケット一覧: [`docs/tickets/README.md`](./docs/tickets/README.md)
- Pages 公開メモ: [`docs/web-github-pages-plan.md`](./docs/web-github-pages-plan.md)
- CI / CD: [`.github/workflows/build.yml`](./.github/workflows/build.yml) と [`.github/workflows/pages.yml`](./.github/workflows/pages.yml)

## 関連プロジェクト

- [piper-plus](https://github.com/ayutaz/piper-plus)
- [uPiper](https://github.com/ayutaz/uPiper)
- [dot-net-g2p](https://github.com/ayutaz/dot-net-g2p)

## ライセンス

[Apache License 2.0](./LICENSE)
