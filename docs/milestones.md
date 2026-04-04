# 対応タスク一覧

更新日: 2026-04-05

この文書は `docs/` 配下の唯一の管理文書です。未完の実装と公開準備だけを残し、完了済みマイルストーンや調査メモは要約だけに留めます。

## 現状

- `P0` `P1` `P2` の機能実装は完了しています。
- release package と platform verification だけが残作業です。
- Windows は source build の Godot headless と packaged addon smoke をローカルで再確認済みです。
- Linux の Godot headless CI は strict 化済みで、all-skip や model bundle 欠落を failure 扱いにしています。
- macOS は arm64 build と C++ test に加えて packaged addon smoke test の CI job まで追加済みで、残りは結果確認です。
- Android/iOS は `test/project/export_presets.cfg`、`scripts/ci/install-godot-export-templates.sh`、`scripts/ci/export-android-smoke.sh`、`scripts/ci/export-ios-smoke.sh`、CI job まで追加済みで、残りは export/link の実結果確認と必要な修正です。
- Android arm64 の debug GDExtension build は Windows local で通過しています。
- Windows + Godot 4.6 の local Android headless export は generic な configuration error で未解決です。
- C++ 側の `ctest --test-dir build-p1-debug -C Debug --output-on-failure` は `123/123` pass です。
- Web は現状サポート対象外です。

## 残っている実装

| ID | 状態 | タスク | 残っている実装 | 完了条件 |
|---|---|---|---|---|
| R5 | 進行中 | macOS arm64 runtime 検証 | packaged addon smoke test の CI 実行結果を確認し、失敗した場合は load / dependency / runtime path の差分を修正する | macOS packaged addon smoke の成否を確定し、文書へ反映する |
| R6 | 進行中 | Android arm64 runtime / export 検証 | `export-android-smoke` の初回結果を確認し、必要なら `export_presets.cfg`、SDK/keystore 解決、Android export 条件を修正する。Windows local の generic export error も切り分ける | Android export smoke が CI で再現可能になり、runtime 可否を確定できる |
| R7 | 進行中 | iOS arm64 runtime / export 検証 | `export-ios-smoke` の初回結果を確認し、必要なら Xcode project/link / export 設定の差分を修正する | iOS export/link smoke の成否を確定し、文書へ反映する |
| P1-8 | 進行中 | Asset Library 登録 | `R5-R7` の結果を反映した package / README / license / changelog を最終化し、公開導線を整える | Asset Library へ申請できる状態になる |

## 実行順

1. `R5` macOS packaged addon smoke の CI 実行結果を確認する
2. `R6` Android export smoke の初回結果を確認し、必要なら `export_presets.cfg` と SDK/keystore 解決を修正する
3. `R6` Windows local の generic Android export error の原因を切り分ける
4. `R7` iOS export/link smoke の初回結果を確認し、必要なら Xcode project/link 差分を修正する
5. `P1-8` Asset Library 申請用の package / 文書を最終化する

## 補足

- `R1-R4` と `R8` は完了済みです。
- 現在の multilingual runtime は `ja/en` の最小実装で、追加 parity 拡張は別タスクです。
- 英語 text input は `cmudict_data.json` の配置が前提です。
- `openjtalk-native` は `openjtalk_wrapper.*` を境界に optional backend として読み込めます。無効 path 時は builtin OpenJTalk へ fallback します。
- CUDA は `execution_provider = EP_CUDA` と `gpu_device_id` で指定できます。CUDA 対応 ONNX Runtime が無い場合は CPU fallback します。
