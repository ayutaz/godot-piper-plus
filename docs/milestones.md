# 対応タスク一覧

更新日: 2026-03-23

この文書は `docs/` 配下の唯一の管理文書です。未対応の実作業だけを残し、完了済みマイルストーン、技術選定の経緯、調査メモは統合のうえ削除しています。

## 現状

- `P0` `P1` `P2` の機能実装は完了しています。
- ただし release package と platform verification は未完です。
- Windows は source build の Godot headless まで確認済みですが、package 化した addon は editor/headless で load 失敗を再現しています。
- Linux の Godot headless CI はありますが、現状は all-skip でも green になり得ます。
- macOS は arm64 build と C++ test まで、Android/iOS は build までで、Godot runtime / export 検証は未実施です。
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
| R1 | 未着手 | package に debug/editor binary を含める | `piper_plus.gdextension` が要求する debug binary を package script/validator が落とさないようにする | packaged addon を Godot editor/headless で load できる |
| R2 | 未着手 | ONNX Runtime sidecar dependency を package へ反映する | `onnxruntime_providers_shared.dll` などの sidecar を package script/validator の対象に含める | runtime dependency 欠落なしで packaged addon が起動する |
| R3 | 未着手 | all-skip CI を failure 扱いにする | `PiperTTS class is unavailable` や model bundle 欠落で green にならないよう、Godot integration の判定を強化する | package/load 失敗が CI で検出される |
| R4 | 未着手 | Windows packaged addon smoke test | package 後の addon を Godot editor/headless で実行確認する | packaged addon の Windows load が CI または手順で再現確認できる |
| R5 | 未着手 | macOS arm64 runtime 検証 | macOS arm64 で Godot から addon load / initialize / synthesize を確認する | macOS runtime 可否が文書で確定する |
| R6 | 未着手 | Android arm64 runtime / export 検証 | export した Godot project で addon load と synthesize を確認する | Android を「build only」から外せる |
| R7 | 未着手 | iOS arm64 runtime / export 検証 | export/link を含めて addon load と synthesize を確認する | iOS を「build only」から外せる |
| R8 | 未着手 | Web 非対応の明文化維持 | README / package / Asset Library 文面で Web を対象外として明記する | Web の過大説明が無い |

## Asset Library

| ID | 状態 | タスク | 内容 | 完了条件 |
|---|---|---|---|---|
| P1-8 | 進行中 | Asset Library 登録 | package / icon / README / license / changelog を整備し、公開導線を整える | `R1` から `R5` までの blocking issue を解消し、Asset Library へ申請できる状態になる |

## 直近の着手候補

- `R1` package に debug/editor binary を含める
- `R2` ONNX Runtime sidecar dependency を package へ反映する
- `R3` all-skip CI を failure 扱いにする
