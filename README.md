# godot-piper-plus

Godot 4.4 以降で使える、オフライン音声合成 addon です。
[`piper-plus`](https://github.com/ayutaz/piper-plus) を Godot 向け GDExtension として提供し、`PiperTTS` ノードと editor ツールを通してゲームやツールにローカル TTS を組み込めます。

## できること

- ローカル完結のニューラル音声合成
- 日本語 text input と英語 text input
- multilingual capability matrix に基づく言語ルーティング
- 同期 / 非同期 / streaming 合成
- `inspect_*` API による dry-run / timing 取得
- model downloader、dictionary editor、Inspector 拡張、test speech UI
- `openjtalk-native` の任意利用と builtin OpenJTalk fallback
- `execution_provider` と `gpu_device_id` による backend 切り替え
- Web 向け preview support

## 導入方法

1. Godot 4.4 以降の project に `addons/piper_plus` を配置します。
2. `Project > Project Settings > Plugins` で **Piper Plus TTS** を有効化します。
3. `Piper Plus: Download Models...` を開き、少なくとも 1 つモデルを追加します。
4. 日本語合成を使う場合は `naist-jdic` も追加します。

この repository 自体を Godot project として開いて試すこともできます。
demo scene は `res://demo/main.tscn` です。

## クイックスタート

最短で試すなら、英語モデルを使う構成が一番簡単です。英語用の `cmudict_data.json` は addon 側に同梱されています。

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

日本語を使う場合は、モデルに加えて `naist-jdic` を用意し、`dictionary_path` を設定してください。

## 主な API

- `synthesize(text)` / `synthesize_async(text)`
- `synthesize_streaming(text, playback)`
- `synthesize_request(request)`
- `synthesize_phoneme_string(phoneme_string)`
- `inspect_text(text)` / `inspect_request(request)` / `inspect_phoneme_string(phoneme_string)`
- `get_last_synthesis_result()`
- `get_last_inspection_result()`
- `get_language_capabilities()`
- `get_last_error()`

より詳しい API は [doc_classes/PiperTTS.xml](./doc_classes/PiperTTS.xml) を参照してください。

## モデルと言語サポート

モデル本体は package に同梱していません。
`res://piper_plus_assets/models/` に手動配置するか、editor の downloader から取得する前提です。

downloader の既定対象:

- `ja_JP-test-medium`
- `tsukuyomi-chan`
- `en_US-ljspeech-medium`
- `naist-jdic`

現在の言語サポート:

- `ja` / `en`: preview auto / explicit 対応
- `es` / `fr` / `pt`: experimental explicit-only
- `zh`: phoneme-only

正本は [tests/fixtures/multilingual_capability_matrix.json](./tests/fixtures/multilingual_capability_matrix.json)、人が読む投影は [docs/generated/multilingual_capability_matrix.md](./docs/generated/multilingual_capability_matrix.md) です。

## サポート状況

`2026-04-10` の GitHub Actions run `24223195868` では、`Build Web`、`Package Complete Addon`、Windows / macOS packaged smoke、Android export smoke、iOS export smoke が成功しています。

| プラットフォーム | アーキテクチャ | 状態 | 補足 |
|---|---|---|---|
| Windows | x86_64 | 確認済み | source build と packaged addon smoke を確認済み |
| Linux | x86_64 | 確認済み | CI build と headless integration を継続実行中 |
| macOS | arm64 | 確認済み | packaged addon smoke を CI で確認済み |
| Android | arm64-v8a | 進行中 | export smoke は CI で確認済み。残りは runtime 可否と Windows local export 差分 |
| iOS | arm64 | 確認済み | export / link smoke を CI で確認済み |
| Web | wasm32 | preview support | browser smoke を CI で確認済み。custom template と `EP_CPU` 前提 |

## Web Preview

Web は preview support です。

- custom Web export template が必要です
- toolchain は Godot 4.4.1 向けに `emsdk 3.1.62` を前提にしています
- `execution_provider` は `EP_CPU` 固定です
- `openjtalk-native` shared library は Web では使えません
- ブラウザ実行には `COOP` / `COEP` を付けた static server が必要です
- 2026-04-10 の CI では `threads` / `no-threads` の両 browser smoke が通過しています

ローカルで Web smoke を回す場合:

```bash
GODOT_SOURCE_DIR=/path/to/godot-source INSTALL_TO_EXPORT_TEMPLATES=1 bash scripts/ci/build-godot-web-templates.sh /tmp/godot-web-templates
bash scripts/ci/build-onnxruntime-web.sh /path/to/onnxruntime-source /tmp/ort-web
ONNXRUNTIME_DIR=/tmp/ort-web bash scripts/ci/build-web-side-module.sh /tmp/web-artifacts
GODOT=/path/to/godot bash scripts/ci/export-web-smoke.sh
```

`run-web-smoke.mjs` には Node.js と Playwright が必要です。
`npm install --no-save playwright` の後に `npx playwright install chromium` を実行してください。

## GitHub Pages Public Demo

GitHub Pages 向けの public demo は `M7` の preview support とは別の follow-up として実装しています。

- workflow は [`.github/workflows/pages.yml`](./.github/workflows/pages.yml) です
- scope は `no-threads` / `CPU-only` / English minimal demo / `multilingual-test-medium` 1 モデル同梱です
- export 入口は `index.html`、PWA と cross-origin isolation workaround を有効にしています
- `pull_request` では Pages demo の build と local smoke を実行し、`main` への push で自動 deploy します
- `workflow_dispatch` では current ref の build を手動実行でき、`deploy_pages=true` のときだけ Pages deploy を試行します
- 公開 URL は Pages deploy 成功と public URL smoke pass 後にのみ案内する方針です

repo 内の実装入口:

- public demo project: [`pages_demo`](./pages_demo)
- project staging: [`scripts/ci/prepare-pages-demo-assets.sh`](./scripts/ci/prepare-pages-demo-assets.sh)
- export: [`scripts/ci/export-pages-demo.sh`](./scripts/ci/export-pages-demo.sh)
- local / remote smoke: [`scripts/ci/run-pages-demo-smoke.mjs`](./scripts/ci/run-pages-demo-smoke.mjs)
- artifact contract: `public-demo-manifest.json` と `build-meta.json`

運用メモと進捗は [docs/web-github-pages-plan.md](./docs/web-github-pages-plan.md) と [docs/milestones.md](./docs/milestones.md) を参照してください。

## Editor ツール

addon は次の editor command を提供します。

- `Piper Plus: Download Models...`
- `Piper Plus: Dictionary Editor...`
- `Piper Plus: Test Speech...`

`PiperTTS` ノードには custom Inspector が入り、preset 適用、ダウンロード導線、辞書編集、試聴 UI を Inspector から開けます。

## package に含まれるもの / 含まれないもの

含まれるもの:

- `addons/piper_plus` 配下の GDExtension と editor plugin
- 英語用 `cmudict_data.json`
- Web preview 用 `web.*` entry

含まれないもの:

- 音声モデル (`.onnx` / `.onnx.json`)
- 日本語用 `naist-jdic`
- `openjtalk-native` 本体

## 既知の制約

- Android は CI export smoke まで確認済みですが、runtime 可否の最終確認が残っています
- Windows local の Android headless export では generic configuration error の切り分けが残っています
- Web は preview support です。正式サポートではありません
- `zh` は phoneme-only で、text input には使えません

## 開発者向け情報

詳しい進捗や技術メモは次を参照してください。

- [docs/milestones.md](./docs/milestones.md)
- [docs/tickets/README.md](./docs/tickets/README.md)
- [docs/web-platform-research.md](./docs/web-platform-research.md)
- [doc_classes/PiperTTS.xml](./doc_classes/PiperTTS.xml)

## 関連プロジェクト

- [piper-plus](https://github.com/ayutaz/piper-plus)
- [uPiper](https://github.com/ayutaz/uPiper)
- [dot-net-g2p](https://github.com/ayutaz/dot-net-g2p)

## ライセンス

[Apache License 2.0](./LICENSE)
