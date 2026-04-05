# 対応タスク一覧

更新日: 2026-04-05

この文書は `docs/` 配下の唯一の管理文書です。未完の実装と公開準備だけを残し、完了済みマイルストーンや調査メモは要約だけに留めます。

## 現状

- テキスト入力から音声合成までの主要機能、エディタ支援機能、追加ランタイム backend 対応は完了しています。
- release package と platform verification だけが残作業です。
- Windows は source build の Godot headless と packaged addon smoke をローカルで再確認済みです。
- Linux の Godot headless CI は strict 化済みで、all-skip や model bundle 欠落を failure 扱いにしています。
- macOS は arm64 build と C++ test に加えて packaged addon smoke test の CI job まで追加済みで、残りは結果確認です。
- Android/iOS は `test/project/export_presets.cfg`、`scripts/ci/install-godot-export-templates.sh`、`scripts/ci/export-android-smoke.sh`、`scripts/ci/export-ios-smoke.sh`、CI job まで追加済みで、残りは export/link の実結果確認と必要な修正です。
- Android arm64 の debug GDExtension build は Windows local で通過しています。
- Windows + Godot 4.6 の local Android headless export は generic な configuration error で未解決です。
- C++ 側の `ctest --test-dir build-p1-debug -C Debug --output-on-failure` は `123/123` pass です。
- Web は現状サポート対象外です。

## このブランチで対応したこと

- Godot プロジェクトとして開発・検証できるように project file 群を追加し、`godot_loop_mcp` addon を取り込みました。
- 日本語 OpenJTalk、英語 CMU 辞書ベース G2P、`ja/en` 自動切り分けの最小 multilingual ルーティング、`language_id` / `language_code`、モデル名/alias 解決、`config_path` fallback を実装しました。
- `custom_dictionary_path` による辞書前処理、`[[ phonemes ]]` 直入力、関連 C++ / GDScript test を追加しました。
- `synthesize_request` / `inspect_request`、`synthesize_phoneme_string` / `inspect_phoneme_string`、timing / dry-run / result 取得 API を実装しました。
- Inspector 拡張、preset 適用、テスト発話 UI、model downloader、dictionary editor、addon 同梱 README / LICENSE / third-party notice を追加しました。
- `openjtalk-native` optional backend と builtin OpenJTalk fallback、`execution_provider = EP_CUDA`、`gpu_device_id` 指定を実装しました。
- package / CI まわりでは、`.gdextension` manifest ベースの package assembly と validator を強化し、Windows packaged addon smoke を再確認しました。
- platform verification まわりでは、Linux headless CI の failure 判定を strict 化し、Android / iOS export smoke 用の script、`export_presets.cfg`、CI job を追加しました。
- 文書面では `README.md`、`CHANGELOG.md`、addon package README を更新し、完了済み範囲と残作業を整理しました。

## 残っている実装

| 状態 | タスク | 残っている実装 | 完了条件 |
|---|---|---|---|
| 進行中 | macOS arm64 の packaged addon 動作確認 | packaged addon smoke test の CI 実行結果を確認し、失敗した場合は load / dependency / runtime path の差分を修正する | macOS packaged addon smoke の成否を確定し、文書へ反映する |
| 進行中 | Android arm64 の export / runtime 確認 | `export-android-smoke` の初回結果を確認し、必要なら `export_presets.cfg`、SDK/keystore 解決、Android export 条件を修正する。Windows local の generic export error も切り分ける | Android export smoke が CI で再現可能になり、runtime 可否を確定できる |
| 進行中 | iOS arm64 の export / link 確認 | `export-ios-smoke` の初回結果を確認し、必要なら Xcode project/link / export 設定の差分を修正する | iOS export/link smoke の成否を確定し、文書へ反映する |
| 進行中 | Asset Library 公開準備 | macOS / Android / iOS の確認結果を反映した package / README / license / changelog を最終化し、公開導線を整える | Asset Library へ申請できる状態になる |

## 実行順

1. macOS packaged addon smoke の CI 実行結果を確認する
2. Android export smoke の初回結果を確認し、必要なら `export_presets.cfg` と SDK/keystore 解決を修正する
3. Windows local の generic Android export error の原因を切り分ける
4. iOS export/link smoke の初回結果を確認し、必要なら Xcode project/link 差分を修正する
5. Asset Library 申請用の package / 文書を最終化する

## 補足

- package / CI の是正と基礎実装は完了済みです。
- 現在の多言語対応は `ja/en` を自動で切り分ける最小構成で、さらに広い parity 拡張は別タスクです。
- 英語 text input は `cmudict_data.json` の配置が前提です。
- `openjtalk-native` は `openjtalk_wrapper.*` を境界に optional backend として読み込めます。無効 path 時は builtin OpenJTalk へ fallback します。
- CUDA は `execution_provider = EP_CUDA` と `gpu_device_id` で指定できます。CUDA 対応 ONNX Runtime が無い場合は CPU fallback します。
