# マイルストーン管理

更新日: 2026-04-06

この文書は `docs/requirements.md` を基準に、要求から release 完了までの到達状況を管理するための文書です。要求定義側では「何を完成とみなすか」を固定し、この文書では「今どこまで進んでいるか」「何を次に閉じるか」を扱います。
実行単位のチケットは [docs/tickets/README.md](./tickets/README.md) で管理します。

状態は次の 4 種で統一します。

- `完了`: 要求に対する実装と検証の完了条件を満たしている
- `進行中`: 一部は満たしているが、完了条件をまだ閉じていない
- `未着手`: 要求は定義済みだが、実装または検証の作業に入れていない
- `要確認`: 実装や CI job はあるが、最終的な成否が未確定

## release 判定サマリ

| 状態 | マイルストーン | 対象要求 | 関連チケット | 現状 |
|---|---|---|---|---|
| 完了 | M1 Runtime API 完成 | `FR-1` `FR-2` `FR-5` `FR-8` | - | 同期 / 非同期 / streaming、request / raw phoneme / inspection、timing / silence 制御、出力形式は実装済み |
| 進行中 | M2 Language / Model / Backend 完成 | `FR-3` `FR-4` `FR-6` | [TKT-001](./tickets/TKT-001-multilingual-parity.md) | 日本語 / 英語 / `ja/en` の基本 contract、model 解決、backend fallback、GPU 指定までは完了。capability matrix は `tests/fixtures/` を正本、`docs/generated/` を投影として整理中 |
| 完了 | M3 Editor Workflow 完成 | `FR-7` | - | downloader、dictionary editor、Inspector 拡張、test speech UI は実装済み |
| 進行中 | M4 Packaging / Documentation 完成 | `FR-9` `NFR-6` | [TKT-002](./tickets/TKT-002-web-platform.md) [TKT-007](./tickets/TKT-007-release-finalization.md) | package assembly / validator と addon 文書は整備済み。最終反映は platform / Web / multilingual の結果待ち |
| 進行中 | M5 Quality Gate 完成 | `NFR-1` `NFR-2` `NFR-3` `NFR-4` `NFR-5` | [TKT-001](./tickets/TKT-001-multilingual-parity.md) [TKT-002](./tickets/TKT-002-web-platform.md) [TKT-003](./tickets/TKT-003-macos-packaged-smoke.md) [TKT-004](./tickets/TKT-004-android-export-runtime.md) [TKT-005](./tickets/TKT-005-windows-android-export-error.md) [TKT-006](./tickets/TKT-006-ios-export-link-smoke.md) [TKT-007](./tickets/TKT-007-release-finalization.md) | C++ test、headless strict 化、package validator は整備済み。multilingual matrix と Web の追加要求分を gate に昇格させる作業が残る |
| 進行中 | M6 Platform Verification 完成 | サポート対象 platform と release 完了条件 | [TKT-003](./tickets/TKT-003-macos-packaged-smoke.md) [TKT-004](./tickets/TKT-004-android-export-runtime.md) [TKT-005](./tickets/TKT-005-windows-android-export-error.md) [TKT-006](./tickets/TKT-006-ios-export-link-smoke.md) | Windows / Linux は概ね確定。macOS / Android / iOS は初回結果の確認と必要修正が残る |
| 未着手 | M7 Web Support 完成 | `FR-10` | [TKT-002](./tickets/TKT-002-web-platform.md) | `web.*` entry、build / export 導線、browser runtime 検証が未整備 |
| 進行中 | M8 Release / Asset Library 準備 | release 完了条件の最終集約 | [TKT-001](./tickets/TKT-001-multilingual-parity.md) [TKT-002](./tickets/TKT-002-web-platform.md) [TKT-003](./tickets/TKT-003-macos-packaged-smoke.md) [TKT-004](./tickets/TKT-004-android-export-runtime.md) [TKT-006](./tickets/TKT-006-ios-export-link-smoke.md) [TKT-007](./tickets/TKT-007-release-finalization.md) | package / README / changelog の最終化と申請導線の確定が残る |

## マイルストーン詳細

<a id="m1"></a>
### M1 Runtime API 完成

- 対象要求: `FR-1` `FR-2` `FR-5` `FR-8`
- 状態: `完了`
- 現状: `PiperTTS` ノードの同期 / 非同期 / streaming 合成、request API、raw phoneme 入力、inspection API、timing / silence 関連取得、`AudioStreamWAV` / `AudioStreamGeneratorPlayback` 出力は実装済みです。
- 残作業: 新規要求を進める中で回帰を出さないことだけを管理対象とします。
- 完了条件: runtime API の追加仕様変更が発生しない限り再オープンしません。

<a id="m2"></a>
### M2 Language / Model / Backend 完成

- 対象要求: `FR-3` `FR-4` `FR-6`
- 状態: `進行中`
- 関連チケット: [TKT-001 Multilingual Parity 拡張](./tickets/TKT-001-multilingual-parity.md)
- 現状: 日本語 OpenJTalk、英語 CMU 辞書ベース G2P、`ja/en` 最小 multilingual ルーティング、`language_id` / `language_code`、`speaker_id`、model alias / config fallback、`openjtalk-native` fallback、`EP_CUDA` / `gpu_device_id` は入っています。
- 残作業:
  - capability matrix の正本を `tests/fixtures/` に固定し、`docs/generated/` と `docs/tickets/` を同一内容へ揃える
  - upstream の `language_id_map` と model config に追従した routing / selection / inspection 方針を explicit-only / phoneme-only に分けて定義する
  - 追加言語に対する runtime API と検証ケースを matrix-first で整備する
- 完了条件:
  - `ja/en` を超える対象言語で routing / selection / inspection が成立する
  - 対象モデルの `language_id` / `language_code` 解決が文書とテストに反映される
  - capability matrix が docs と tests の正本として一致している
  - backend fallback と GPU fallback の既存要件を維持する

<a id="m3"></a>
### M3 Editor Workflow 完成

- 対象要求: `FR-7`
- 状態: `完了`
- 現状: model downloader、custom dictionary editor、custom Inspector、test speech UI、preset 導線は実装済みです。
- 残作業: `M2` と `M7` の追加要求に伴う導線変更が必要になった場合だけ reopen します。
- 完了条件: editor 機能の新規要求が増えない限り再オープンしません。

<a id="m4"></a>
### M4 Packaging / Documentation 完成

- 対象要求: `FR-9` `NFR-6`
- 状態: `進行中`
- 関連チケット: [TKT-002 Web Platform 対応](./tickets/TKT-002-web-platform.md) [TKT-007 Release Package / 文書最終化](./tickets/TKT-007-release-finalization.md)
- 現状: `.gdextension` manifest ベースの package assembly / validator、addon README / LICENSE / third-party notice、package 範囲の整理は実施済みです。
- 残作業:
  - `M2` `M6` `M7` の結果を package / README / addon README / changelog へ最終反映する
  - multilingual と Web の要求追加後の配布境界を文書へ反映する
- 完了条件:
  - package に含めるもの / 含めないものが最終状態で一致している
  - runtime API と配布手順の文書が最終実装と一致している
  - Asset Library 提出物に転記できる説明が揃っている

<a id="m5"></a>
### M5 Quality Gate 完成

- 対象要求: `NFR-1` `NFR-2` `NFR-3` `NFR-4` `NFR-5`
- 状態: `進行中`
- 関連チケット: [TKT-001 Multilingual Parity 拡張](./tickets/TKT-001-multilingual-parity.md) [TKT-002 Web Platform 対応](./tickets/TKT-002-web-platform.md) [TKT-003 macOS Packaged Smoke 確認](./tickets/TKT-003-macos-packaged-smoke.md) [TKT-004 Android Export / Runtime 確認](./tickets/TKT-004-android-export-runtime.md) [TKT-005 Windows Local Android Export Error 切り分け](./tickets/TKT-005-windows-android-export-error.md) [TKT-006 iOS Export / Link Smoke 確認](./tickets/TKT-006-ios-export-link-smoke.md)
- 現状: `compatibility_minimum = 4.4`、オフライン runtime 前提、C++ unit test 継続実行、Godot headless strict 化、package validator による binary / dependency 検証までは整っています。
- 残作業:
  - `M2` の multilingual matrix-first 検証ケースを追加する
  - `M7` の Web build / export / runtime 検証を quality gate に組み込む
  - `M6` の platform smoke 結果を最終的な pass / fail として確定する
- 完了条件:
  - C++ test、headless test、package validator が継続実行可能である
  - all-skip / pass 0 / addon 未登録 / model bundle 欠落を CI failure として維持できる
  - multilingual matrix と Web を含む最終スコープの検証項目が定義済みである

<a id="m6"></a>
### M6 Platform Verification 完成

- 対象要求: サポート対象 platform、release 完了条件の platform 部分
- 状態: `進行中`
- 関連チケット: [TKT-003 macOS Packaged Smoke 確認](./tickets/TKT-003-macos-packaged-smoke.md) [TKT-004 Android Export / Runtime 確認](./tickets/TKT-004-android-export-runtime.md) [TKT-005 Windows Local Android Export Error 切り分け](./tickets/TKT-005-windows-android-export-error.md) [TKT-006 iOS Export / Link Smoke 確認](./tickets/TKT-006-ios-export-link-smoke.md)

| プラットフォーム | 状態 | 現状 | 完了条件 |
|---|---|---|---|
| Windows | `完了` | source build の headless と packaged addon smoke をローカルで再確認済み | 既存結果を維持し、release 文書へ反映する |
| Linux | `完了` | CI build と headless integration があり、strict failure 判定も導入済み | 既存結果を維持し、release 文書へ反映する |
| macOS arm64 | `要確認` | build / C++ test / packaged smoke job はあるが、初回 CI 実結果の確定が未了 | packaged addon smoke の成否を確定し、必要修正を反映する |
| Android arm64-v8a | `進行中` | export script、`export_presets.cfg`、CI job はある。Windows local では generic configuration error が未解決 | CI で export smoke と runtime 可否を確定し、必要修正を反映する |
| iOS arm64 | `要確認` | export/link smoke script と CI job はあるが、初回結果の確定が未了 | export/link smoke の成否を確定し、必要修正を反映する |

- 残作業:
  - macOS packaged addon smoke の結果確認と必要修正
  - Android export smoke / runtime 可否の確定と必要修正
  - Windows local Android export の generic configuration error 切り分け
  - iOS export/link smoke の結果確認と必要修正
- 完了条件:
  - サポート対象の desktop / mobile platform の成否が文書付きで確定している
  - CI と local の差分が説明可能な状態になっている

<a id="m7"></a>
### M7 Web Support 完成

- 対象要求: `FR-10`
- 状態: `未着手`
- 関連チケット: [TKT-002 Web Platform 対応](./tickets/TKT-002-web-platform.md)
- 現状: `web.*` 向け GDExtension entry、build / export 導線、runtime backend 制約、browser smoke test は未整備です。
- 残作業:
  - Web の対象アーキテクチャ、利用可能 backend、制約事項を定義する
  - `web.*` entry と package 反映を実装する
  - browser load または同等の smoke test を整備する
  - README と package 文書へ Web の前提と制約を反映する
- 完了条件:
  - Web export 向け build / package / export 導線が成立する
  - runtime 可否と制約が明文化される
  - browser smoke で addon のロード成否を確認できる

<a id="m8"></a>
### M8 Release / Asset Library 準備

- 対象要求: release 完了条件の最終集約
- 状態: `進行中`
- 依存: `M2` `M4` `M5` `M6` `M7`
- 関連チケット: [TKT-001 Multilingual Parity 拡張](./tickets/TKT-001-multilingual-parity.md) [TKT-002 Web Platform 対応](./tickets/TKT-002-web-platform.md) [TKT-003 macOS Packaged Smoke 確認](./tickets/TKT-003-macos-packaged-smoke.md) [TKT-004 Android Export / Runtime 確認](./tickets/TKT-004-android-export-runtime.md) [TKT-006 iOS Export / Link Smoke 確認](./tickets/TKT-006-ios-export-link-smoke.md) [TKT-007 Release Package / 文書最終化](./tickets/TKT-007-release-finalization.md)
- 現状: package / validator / README 類の基礎は揃っていますが、platform verification と新規要求分の確定待ちです。
- 残作業:
  - `M2` `M6` `M7` の結果を package / README / license / changelog に反映する
  - Asset Library 提出時の説明、同梱範囲、注意事項を最終化する
- 完了条件:
  - `docs/requirements.md` の release 完了条件をすべて閉じている
  - Asset Library へ申請できる package 導線が整っている

## 直近の実行順

1. [TKT-001 Multilingual Parity 拡張](./tickets/TKT-001-multilingual-parity.md) で、capability matrix、routing 方針、検証ケースを確定する
2. [TKT-002 Web Platform 対応](./tickets/TKT-002-web-platform.md) で、backend 制約、build / export 導線、smoke test 方針を確定する
3. [TKT-003 macOS arm64 packaged addon smoke 確認](./tickets/TKT-003-macos-packaged-smoke.md) で、CI 実結果を確認し、必要修正を入れる
4. [TKT-004 Android arm64 export / runtime 確認](./tickets/TKT-004-android-export-runtime.md) で、export smoke / runtime 可否を確認し、必要なら `export_presets.cfg`、SDK / keystore、runtime 条件を修正する
5. [TKT-005 Windows Local Android Export Error 切り分け](./tickets/TKT-005-windows-android-export-error.md) で、generic configuration error を切り分ける
6. [TKT-006 iOS Export / Link Smoke 確認](./tickets/TKT-006-ios-export-link-smoke.md) で、CI 実結果を確認し、必要修正を入れる
7. [TKT-007 Release Package / 文書最終化](./tickets/TKT-007-release-finalization.md) で、`M2` `M6` `M7` の結果を package / 文書 / changelog に反映し、Asset Library 公開準備を閉じる

## ブロッカー / 未確定事項

- multilingual capability matrix の正本化がまだ完了していません。
- Web で許容する backend と runtime 制約がまだ定義されていません。
- macOS / Android / iOS の初回 CI 結果がまだ release 判定へ織り込まれていません。
- Windows local Android export の generic configuration error が Android 検証のノイズ源として残っています。

## 完了済みの要約

- runtime API、editor workflow、package assembly / validator の基礎実装は概ね完了しています。
- Windows packaged addon smoke と Linux headless strict CI は整備済みです。
- Android / iOS 向け export smoke script と CI job は追加済みで、残りは実結果の確定です。
- 現在の multilingual contract は capability-based に整理し直しています。ここから先は `M2` の新規要求として扱います。
