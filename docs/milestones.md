# 対応タスク一覧

更新日: 2026-03-23

この文書は `docs/` 配下の唯一の管理文書です。未対応の実作業だけを残し、完了済みマイルストーン、技術選定の経緯、調査メモは統合のうえ削除しています。

## 現状

- `P0` は完了です。
- `P1` は実装と検証まで完了しています。
- `P2` は repo 内実装まで完了しています。
- 現在の multilingual runtime は `ja/en` の最小実装です。さらなる上流 parity 拡張は別タスクとして扱います。
- 英語 text input は `cmudict_data.json` の配置が前提です。
- C++ 側の `ctest --test-dir build-p1-debug -C Debug --output-on-failure` は `123/123` pass です。
- Godot headless の `test/project` は `test/models/multilingual-test-medium.onnx` を使って parity テストを含む GDScript テストを実行できます。
- `openjtalk-native` は `openjtalk_wrapper.*` を境界に optional backend として読み込めます。無効 path 時は builtin OpenJTalk へ fallback します。
- CUDA は `execution_provider = EP_CUDA` と `gpu_device_id` で指定できます。CUDA 対応 ONNX Runtime が無い場合は CPU fallback します。

## 実行順

1. Asset Library 公開導線の最終整理
2. package / icon / README / changelog の公開向け整形
3. Asset Library 申請時の説明文とスクリーンショット準備

## P1

| ID | 状態 | タスク | 内容 | 完了条件 |
|---|---|---|---|---|
| P1-8 | 進行中 | Asset Library 登録 | package / icon / README / license / changelog を整備し、公開導線を整える | Asset Library へ申請できる状態になる |

## 直近の着手候補

- `P1-8` Asset Library 登録
- Asset Library 向け package の最終点検
- Asset Library 申請文面の作成
