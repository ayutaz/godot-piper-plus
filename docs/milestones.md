# 対応タスク一覧

更新日: 2026-03-23

この文書は `docs/` 配下の唯一の管理文書です。未対応の実作業だけを残し、完了済みマイルストーン、技術選定の経緯、調査メモは統合のうえ削除しています。

## 現状

- `P0` `P1` `P2` の機能実装は完了しています。
- ただし release package と platform verification は未完です。
- Windows は source build の Godot headless に加えて、package 化した addon を使う headless smoke をローカルで再確認済みです。
- Linux の Godot headless CI は strict 化し、all-skip や model bundle 欠落を failure 扱いにする導線を追加しました。
- macOS は arm64 build と C++ test に加えて、packaged addon smoke test の CI job を追加しました。初回実行結果の確認は未了です。
- Android/iOS は debug/release の native binary を package に含める導線と validator に加え、`test/project/export_presets.cfg` と export smoke job を追加しました。初回実行結果の確認は未了です。
- Web は現状サポート対象外です。
- 現在の multilingual runtime は `ja/en` の最小実装です。さらなる上流 parity 拡張は別タスクとして扱います。
- 英語 text input は `cmudict_data.json` の配置が前提です。
- C++ 側の `ctest --test-dir build-p1-debug -C Debug --output-on-failure` は `123/123` pass です。
- `openjtalk-native` は `openjtalk_wrapper.*` を境界に optional backend として読み込めます。無効 path 時は builtin OpenJTalk へ fallback します。
- CUDA は `execution_provider = EP_CUDA` と `gpu_device_id` で指定できます。CUDA 対応 ONNX Runtime が無い場合は CPU fallback します。

## 実行順

1. package / CI / platform verification の是正
2. Windows / macOS / Android / iOS の Godot runtime 検証
3. Asset Library 公開導線の最終整理

## Release / Platform Readiness

| ID | 状態 | タスク | 内容 | 完了条件 |
|---|---|---|---|---|
| R1 | 完了 | package に debug/editor binary を含める | `piper_plus.gdextension` が要求する debug / release binary を package script/validator が全 platform 分拾うようにする | packaged addon が manifest と整合する |
| R2 | 完了 | ONNX Runtime sidecar dependency を package へ反映する | `onnxruntime_providers_shared.dll` などの sidecar を manifest / package script の対象に含める | runtime dependency 欠落なしで packaged addon を組み立てられる |
| R3 | 完了 | all-skip CI を failure 扱いにする | `PiperTTS class is unavailable` や model bundle 欠落で green にならないよう、Godot integration の判定を強化する | package/load 失敗が CI で検出される |
| R4 | 完了 | Windows packaged addon smoke test | package 後の addon を Godot editor/headless で実行確認する | ローカル手順で packaged addon の Windows load を再確認し、CI job も追加済み |
| R5 | 進行中 | macOS arm64 runtime 検証 | macOS arm64 で Godot から addon load / initialize / synthesize を確認する | packaged addon smoke test の CI 実行結果を確認し、可否を文書で確定する |
| R6 | 進行中 | Android arm64 runtime / export 検証 | export した Godot project で addon load と synthesize を確認する | `export_presets.cfg` と `export-android-smoke` job は追加済み。初回 CI 結果を確認し、必要な export 設定差分を詰める |
| R7 | 進行中 | iOS arm64 runtime / export 検証 | export/link を含めて addon load と synthesize を確認する | `export-ios-smoke` job は追加済み。初回 CI 結果を確認し、Xcode project/link の差分を詰める |
| R8 | 完了 | Web 非対応の明文化維持 | README / package / Asset Library 文面で Web を対象外として明記する | Web の過大説明が無い |

## Asset Library

| ID | 状態 | タスク | 内容 | 完了条件 |
|---|---|---|---|---|
| P1-8 | 進行中 | Asset Library 登録 | package / icon / README / license / changelog を整備し、公開導線を整える | `R1` から `R5` までの blocking issue を解消し、Asset Library へ申請できる状態になる |

## 直近の着手候補

- `R5` macOS packaged addon smoke の CI 実行結果を確認する
- `R6` Android export smoke の初回結果を確認し、Windows local の generic export error の原因を詰める
- `R7` iOS export/link smoke の初回結果を確認する
