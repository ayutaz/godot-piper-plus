# 対応タスク一覧

更新日: 2026-03-22

この文書は `docs/` 配下の唯一の管理文書です。未対応の実作業だけを残し、完了済みマイルストーン、技術選定の経緯、調査メモは統合のうえ削除しています。

## 現状

- `P0` は完了です。
- 現在の multilingual runtime は `ja/en` の最小実装です。上流 parity の拡張は `P1` で扱います。
- 英語 text input は `cmudict_data.json` の配置が前提です。
- C++ 側の `ctest --test-dir build-test --output-on-failure` は `123/123` pass です。
- Godot headless の `test/project` は起動するが、現状は `PiperTTS class is unavailable` で GDScript 側が skip します。

## 実行順

1. 高度 runtime API と出力機能の整理
2. parity テストと CI の拡張
3. エディタ UI と配布導線の仕上げ

## P1

| ID | 状態 | タスク | 内容 | 完了条件 |
|---|---|---|---|---|
| P1-1 | 未着手 | 高度 runtime API 公開 | `json-input`、`raw-phonemes`、`sentence-silence`、`phoneme-silence`、`test-mode` のうち必要なものを `PiperTTS` に公開する | Godot 側 API から必要機能を指定できる |
| P1-2 | 未着手 | timing 出力対応 | phoneme timing などの同期情報を取得できるようにする | lip sync や字幕同期に使える出力が得られる |
| P1-3 | 未着手 | raw phoneme 入力対応 | テキストではなく音素列を直接与える経路を追加する | 発音固定テストや検証に使える |
| P1-4 | 未着手 | dry-run / 検査モード追加 | 音声再生なしで解析や検証に使えるモードを追加する | 検査用途で安全に実行できる |
| P1-5 | 未着手 | parity テスト追加 | multilingual runtime、model resolver、question marker、raw phoneme、timing のテストを追加する | 不足領域の自動テストが揃う |
| P1-6 | 未着手 | Layer 2 CI の対象拡張 | 既存の headless 実行に multilingual / model manager / parity 項目を追加し、`test/project` の GDExtension 読み込み確認まで含める | workflow 上で不足領域の検証が自動実行される |
| P1-7 | 未着手 | エディタ UI 改善 | `addons/piper_plus/icon.svg`、Inspector カスタマイズ、テスト発話 UI を実装する | エディタ内でモデル選択と試聴が完結する |
| P1-8 | 未着手 | Asset Library 登録 | Godot Asset Library 向けの公開導線を整備する | Asset Library から導入可能になる |
| P1-9 | 未着手 | CHANGELOG 整備 | リリースノート管理用の `CHANGELOG.md` を追加する | リリース差分を追える |

## P2

| ID | 状態 | タスク | 内容 | 完了条件 |
|---|---|---|---|---|
| P2-1 | 未着手 | `openjtalk_optimized` 移植 | 上流の最適化 OpenJTalk 経路を必要に応じて移植する | 効果と保守コストを評価したうえで採用可否が決まる |
| P2-2 | 未着手 | CUDA 固有制御対応 | CUDA や GPU device 選択を必要なら追加する | デスクトップ向け高度設定として利用できる |

## 直近の着手候補

- `P1-1` 高度 runtime API 公開
- `P1-5` parity テスト追加
- `P1-6` Layer 2 CI の対象拡張
