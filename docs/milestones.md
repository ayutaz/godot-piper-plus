# 対応タスク一覧

更新日: 2026-03-22

この文書は `docs/` 配下の唯一の管理文書です。未対応の実作業だけを残し、完了済みマイルストーン、技術選定の経緯、調査メモは統合のうえ削除しています。

## 現状

- `P0` は完了です。
- `P1` は実装と検証まで完了しています。
- 現在の multilingual runtime は `ja/en` の最小実装です。さらなる上流 parity 拡張は `P2` 以降で扱います。
- 英語 text input は `cmudict_data.json` の配置が前提です。
- C++ 側の `ctest --test-dir build-p1-debug -C Debug --output-on-failure` は `123/123` pass です。
- Godot headless の `test/project` は `test/models/multilingual-test-medium.onnx` を使って parity テストを含む GDScript テストを実行できます。
- `openjtalk-native` への backend 置換は `openjtalk_wrapper.*` を境界にした `P2` 扱いです。

## 実行順

1. Asset Library 公開導線の最終整理
2. OpenJTalk backend の将来方針整理
3. GPU / platform 固有制御の要否確認

## P1

| ID | 状態 | タスク | 内容 | 完了条件 |
|---|---|---|---|---|
| P1-8 | 進行中 | Asset Library 登録 | package / icon / README / license / changelog を整備し、公開導線を整える | Asset Library へ申請できる状態になる |

## P2

| ID | 状態 | タスク | 内容 | 完了条件 |
|---|---|---|---|---|
| P2-1 | 未着手 | `openjtalk_optimized` 移植 | 上流の最適化 OpenJTalk 経路を必要に応じて移植する | 効果と保守コストを評価したうえで採用可否が決まる |
| P2-2 | 未着手 | CUDA 固有制御対応 | CUDA や GPU device 選択を必要なら追加する | デスクトップ向け高度設定として利用できる |

## 直近の着手候補

- `P1-8` Asset Library 登録
- `P2-1` `openjtalk_optimized` / `openjtalk-native` 方針整理
- `P2-2` CUDA 固有制御対応
