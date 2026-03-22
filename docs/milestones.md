# 対応タスク一覧

更新日: 2026-03-22

この文書は `docs/` 配下の唯一の管理文書です。未対応の実作業だけを残し、完了済みマイルストーン、技術選定の経緯、調査メモは統合のうえ削除しています。

## 実行順

1. カスタム辞書の整合性修正
2. 英語 GPL フリー G2P と多言語 runtime 対応
3. モデル管理層の追加
4. 高度 runtime API と出力機能の整理
5. テストと CI の拡張
6. エディタ UI と配布導線の仕上げ

## P0

| ID | タスク | 内容 | 完了条件 |
|---|---|---|---|
| P0-1 | カスタム辞書保存形式の統一 | `addons/piper_plus/dictionary_editor.gd` の保存形式を `src/piper_core/custom_dictionary.cpp` の期待形式に合わせる | editor で保存した辞書を runtime がそのまま読み込める |
| P0-2 | カスタム辞書の runtime 接続 | 合成処理でカスタム辞書を実際に適用する | 合成経路で辞書置換が有効になり、テストで確認できる |
| P0-3 | 英語 GPL フリー G2P 実装 | 上流 `piper-plus` 相当の英語 phonemizer を Godot 側へ移植する | 英語モデルでテキスト入力から合成できる |
| P0-4 | 言語選択 API 追加 | `PiperTTS` に言語指定または `language_id` を追加する | GDScript から言語を切り替えて実行できる |
| P0-5 | 多言語 phonemizer 導入 | 英語以外の多言語 phonemizer と必要な分岐を追加する | 上流相当の多言語 runtime 構成が揃う |
| P0-6 | モデル管理層の追加 | モデルカタログ、alias 解決、config fallback、既定モデル配置方針を実装する | 明示パスなしでもモデル選択と解決ができる |
| P0-7 | README とコード内説明の現況更新 | 英語未実装、テスト状況、対応言語などの古い記述を現状に合わせる | `README.md`、`CLAUDE.md`、関連説明が実装状態と矛盾しない |

## P1

| ID | タスク | 内容 | 完了条件 |
|---|---|---|---|
| P1-1 | 高度 runtime API 公開 | `language_id`、`json-input`、`raw-phonemes`、`sentence-silence`、`phoneme-silence`、`test-mode` のうち必要なものを `PiperTTS` に公開する | Godot 側 API から必要機能を指定できる |
| P1-2 | timing 出力対応 | phoneme timing などの同期情報を取得できるようにする | lip sync や字幕同期に使える出力が得られる |
| P1-3 | raw phoneme 入力対応 | テキストではなく音素列を直接与える経路を追加する | 発音固定テストや検証に使える |
| P1-4 | dry-run / 検査モード追加 | 音声再生なしで解析や検証に使えるモードを追加する | 検査用途で安全に実行できる |
| P1-5 | parity テスト追加 | multilingual G2P、model manager、question marker、raw phoneme、timing のテストを追加する | 不足領域の自動テストが揃う |
| P1-6 | Layer 2 CI の対象拡張 | 既存の headless 実行に multilingual / model manager / parity 項目を追加する | workflow 上で不足領域の検証が自動実行される |
| P1-7 | エディタ UI 改善 | `addons/piper_plus/icon.svg`、Inspector カスタマイズ、テスト発話 UI を実装する | エディタ内でモデル選択と試聴が完結する |
| P1-8 | Asset Library 登録 | Godot Asset Library 向けの公開導線を整備する | Asset Library から導入可能になる |
| P1-9 | CHANGELOG 整備 | リリースノート管理用の `CHANGELOG.md` を追加する | リリース差分を追える |

## P2

| ID | タスク | 内容 | 完了条件 |
|---|---|---|---|
| P2-1 | `openjtalk_optimized` 移植 | 上流の最適化 OpenJTalk 経路を必要に応じて移植する | 効果と保守コストを評価したうえで採用可否が決まる |
| P2-2 | CUDA 固有制御対応 | CUDA や GPU device 選択を必要なら追加する | デスクトップ向け高度設定として利用できる |

## 直近の着手候補

- `P0-1` カスタム辞書保存形式の統一
- `P0-2` カスタム辞書の runtime 接続
- `P0-7` README とコード内説明の現況更新
- `P0-3` 英語 GPL フリー G2P 実装
