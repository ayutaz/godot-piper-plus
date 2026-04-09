# マイルストーン管理

更新日: 2026-04-09

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
| 完了 | M2 Language / Model / Backend 完成 | `FR-3` `FR-4` `FR-6` | - | multilingual capability contract、`language_code` / `language_id` 解決、matrix-first 検証、backend fallback、GPU 指定まで完了。正本は `tests/fixtures/`、投影は `docs/generated/` に固定済み |
| 完了 | M3 Editor Workflow 完成 | `FR-7` | - | downloader、dictionary editor、Inspector 拡張、test speech UI は実装済み |
| 進行中 | M4 Packaging / Documentation 完成 | `FR-9` `NFR-6` | [TKT-002](./tickets/TKT-002-web-platform.md) [TKT-009](./tickets/TKT-009-web-manifest-package-export-preset.md) [TKT-011](./tickets/TKT-011-web-browser-smoke-ci-docs.md) [TKT-007](./tickets/TKT-007-release-finalization.md) | package assembly / validator と addon 文書は整備済み。multilingual contract は反映済みで、残りは platform / Web 結果の最終反映 |
| 進行中 | M5 Quality Gate 完成 | `NFR-1` `NFR-2` `NFR-3` `NFR-4` `NFR-5` | [TKT-002](./tickets/TKT-002-web-platform.md) [TKT-003](./tickets/TKT-003-macos-packaged-smoke.md) [TKT-004](./tickets/TKT-004-android-export-runtime.md) [TKT-005](./tickets/TKT-005-windows-android-export-error.md) [TKT-006](./tickets/TKT-006-ios-export-link-smoke.md) [TKT-008](./tickets/TKT-008-web-template-toolchain-bootstrap.md) [TKT-009](./tickets/TKT-009-web-manifest-package-export-preset.md) [TKT-010](./tickets/TKT-010-web-runtime-ort-adaptation.md) [TKT-011](./tickets/TKT-011-web-browser-smoke-ci-docs.md) | C++ test、headless strict 化、package validator、multilingual matrix-first 検証、Web Phase 1 の受け入れ条件固定までは整備済み。残りは Web と platform smoke の最終 gate 化 |
| 進行中 | M6 Platform Verification 完成 | サポート対象 platform と release 完了条件 | [TKT-003](./tickets/TKT-003-macos-packaged-smoke.md) [TKT-004](./tickets/TKT-004-android-export-runtime.md) [TKT-005](./tickets/TKT-005-windows-android-export-error.md) [TKT-006](./tickets/TKT-006-ios-export-link-smoke.md) | Windows / Linux は概ね確定。macOS / Android / iOS は初回結果の確認と必要修正が残る |
| 進行中 | M7 Web Support 完成 | `FR-10` | [TKT-002](./tickets/TKT-002-web-platform.md) [TKT-008](./tickets/TKT-008-web-template-toolchain-bootstrap.md) [TKT-009](./tickets/TKT-009-web-manifest-package-export-preset.md) [TKT-010](./tickets/TKT-010-web-runtime-ort-adaptation.md) [TKT-011](./tickets/TKT-011-web-browser-smoke-ci-docs.md) | `W0` feasibility / scope 固定は完了。残りは `W1` custom template / toolchain、`W2` manifest / package、`W3` runtime adaptation、`W4` browser smoke / 文書反映 |
| 進行中 | M8 Release / Asset Library 準備 | release 完了条件の最終集約 | [TKT-002](./tickets/TKT-002-web-platform.md) [TKT-003](./tickets/TKT-003-macos-packaged-smoke.md) [TKT-004](./tickets/TKT-004-android-export-runtime.md) [TKT-006](./tickets/TKT-006-ios-export-link-smoke.md) [TKT-011](./tickets/TKT-011-web-browser-smoke-ci-docs.md) [TKT-007](./tickets/TKT-007-release-finalization.md) | package / README / changelog の基礎と multilingual 反映は完了。残りは platform / Web 結果の最終反映と申請導線の確定 |

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
- 状態: `完了`
- 関連チケット: なし
- 現状: 日本語 OpenJTalk、英語 CMU 辞書ベース G2P、multilingual capability-first routing、`language_id` / `language_code` / `speaker_id` 解決、model alias / config fallback、`openjtalk-native` fallback、`EP_CUDA` / `gpu_device_id` は実装済みです。`tests/fixtures/multilingual_capability_matrix.json` を正本、`docs/generated/multilingual_capability_matrix.md` を投影として固定し、`get_language_capabilities()` / `get_last_error()` / `resolved_segments` を含む API contract と matrix-first の C++ / headless 検証も反映済みです。
- 残作業: 現時点なし。release package への最終反映は `M4` と `M8` で管理します。
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
- 関連チケット: [TKT-002 Web Platform 対応](./tickets/TKT-002-web-platform.md) [TKT-009 Web manifest / package / export preset 整備](./tickets/TKT-009-web-manifest-package-export-preset.md) [TKT-011 Web browser smoke / CI / 文書反映](./tickets/TKT-011-web-browser-smoke-ci-docs.md) [TKT-007 Release Package / 文書最終化](./tickets/TKT-007-release-finalization.md)
- 現状: `.gdextension` manifest ベースの package assembly / validator、addon README / LICENSE / third-party notice、package 範囲の整理、multilingual contract の文書反映は実施済みです。
- 残作業:
  - `M6` `M7` の結果を package / README / addon README / changelog へ最終反映する
  - Web 要求追加後の配布境界を文書へ反映する
- 完了条件:
  - package に含めるもの / 含めないものが最終状態で一致している
  - runtime API と配布手順の文書が最終実装と一致している
  - Asset Library 提出物に転記できる説明が揃っている

<a id="m5"></a>
### M5 Quality Gate 完成

- 対象要求: `NFR-1` `NFR-2` `NFR-3` `NFR-4` `NFR-5`
- 状態: `進行中`
- 関連チケット: [TKT-002 Web Platform 対応](./tickets/TKT-002-web-platform.md) [TKT-003 macOS Packaged Smoke 確認](./tickets/TKT-003-macos-packaged-smoke.md) [TKT-004 Android Export / Runtime 確認](./tickets/TKT-004-android-export-runtime.md) [TKT-005 Windows Local Android Export Error 切り分け](./tickets/TKT-005-windows-android-export-error.md) [TKT-006 iOS Export / Link Smoke 確認](./tickets/TKT-006-ios-export-link-smoke.md) [TKT-008 Web custom template / toolchain bootstrap](./tickets/TKT-008-web-template-toolchain-bootstrap.md) [TKT-009 Web manifest / package / export preset 整備](./tickets/TKT-009-web-manifest-package-export-preset.md) [TKT-010 Web runtime adaptation / ORT Web 対応](./tickets/TKT-010-web-runtime-ort-adaptation.md) [TKT-011 Web browser smoke / CI / 文書反映](./tickets/TKT-011-web-browser-smoke-ci-docs.md) [TKT-007 Release Package / 文書最終化](./tickets/TKT-007-release-finalization.md)
- 現状: `compatibility_minimum = 4.4`、オフライン runtime 前提、C++ unit test 継続実行、Godot headless strict 化、package validator による binary / dependency 検証、multilingual matrix-first の C++ / headless 検証までは整っています。
- 残作業:
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
- 状態: `進行中`
- 関連チケット: [TKT-002 Web Platform 対応](./tickets/TKT-002-web-platform.md) [TKT-008 Web custom template / toolchain bootstrap](./tickets/TKT-008-web-template-toolchain-bootstrap.md) [TKT-009 Web manifest / package / export preset 整備](./tickets/TKT-009-web-manifest-package-export-preset.md) [TKT-010 Web runtime adaptation / ORT Web 対応](./tickets/TKT-010-web-runtime-ort-adaptation.md) [TKT-011 Web browser smoke / CI / 文書反映](./tickets/TKT-011-web-browser-smoke-ci-docs.md)
- 現状: feasibility と設計メモは [docs/web-platform-research.md](./web-platform-research.md) に整理済みです。`W0` として Phase 1 の scope は `preview support`、`CPU-only`、custom Web export template 前提、`web.*` manifest、addon load と最小モデル synthesize を見る CI browser smoke、制約文書化までで固定済みです。`W1` から `W4` はすでにコード着手しており、Web 向け toolchain 伝播、custom template/bootstrap script、`web.*` manifest / package / export preset、ORT Web static-lib の入口、Web での `EP_CPU` 固定 error handling、model / config / `cmudict_data.json` の path 非依存 loader、Phase 1 の最小 synthesize 境界を返す runtime contract、browser smoke / CI / README 反映の土台は入りました。残りは重い Web build/export/browser smoke の初回実行結果を CI / local で確定することです。
- 実装スコープ:
  - Phase 1: `preview support`。custom template、`web.*` manifest、Web 向け runtime 制約、browser smoke、README 反映までを release gate に含める
  - Phase 2: Japanese text input の dictionary bootstrap、multilingual parity 拡張、binary size 最適化などの広がりは preview 後に扱う
- 残作業:
  - [TKT-008 Web custom template / toolchain bootstrap](./tickets/TKT-008-web-template-toolchain-bootstrap.md) を成立させる
  - [TKT-009 Web manifest / package / export preset 整備](./tickets/TKT-009-web-manifest-package-export-preset.md) を閉じる
  - [TKT-010 Web runtime adaptation / ORT Web 対応](./tickets/TKT-010-web-runtime-ort-adaptation.md) を実装する
  - [TKT-011 Web browser smoke / CI / 文書反映](./tickets/TKT-011-web-browser-smoke-ci-docs.md) を閉じる
- 完了条件:
  - Web export 向け build / package / export 導線が成立する
  - `web.*` manifest、package validator、test project export preset が同じ artifact 契約を参照している
  - runtime 可否と制約が明文化される
  - CI 上の browser smoke で addon load と最小モデル synthesize の成否を確認でき、同じ script を local でも再実行でき、README と addon README に制約が反映される
- 実装マイルストーン:

| 状態 | ID | チケット | マイルストーン | 主な変更対象 | 完了条件 |
|---|---|---|---|---|---|
| 完了 | `W0` | [TKT-002](./tickets/TKT-002-web-platform.md) | feasibility / scope 固定 | `docs/web-platform-research.md`, `docs/milestones.md`, `docs/tickets/TKT-002-web-platform.md` | Phase 1 を `preview support`、`CPU-only`、custom template、addon load と最小モデル synthesize を見る CI browser smoke 前提で固定し、`W1` から `W4` の分割を確定している |
| 進行中 | `W1` | [TKT-008](./tickets/TKT-008-web-template-toolchain-bootstrap.md) | custom template / toolchain bootstrap | `CMakeLists.txt`, `cmake/HTSEngine.cmake`, `scripts/ci/install-godot-export-templates.sh`, 必要なら Web template build script | `dlink_enabled=yes` 前提の custom Web export template と Emscripten build の入口が再現でき、thread / no-thread の binary 方針、artifact 名、出力配置が固定されている |
| 進行中 | `W2` | [TKT-009](./tickets/TKT-009-web-manifest-package-export-preset.md) | manifest / package / export preset 整備 | `addons/piper_plus/piper_plus.gdextension`, `test/project/addons/piper_plus/piper_plus.gdextension`, `test/project/export_presets.cfg`, `scripts/ci/package-addon.sh`, `scripts/ci/validate-addon-package.sh`, `test/prepare-assets.sh` | `W1` で固定した Web artifact matrix を `web.*` entry、package、validator、test project の Web export preset へ矛盾なく反映できている |
| 進行中 | `W3` | [TKT-010](./tickets/TKT-010-web-runtime-ort-adaptation.md) | runtime adaptation と ORT Web 対応 | `cmake/FindOnnxRuntime.cmake`, `src/piper_core/piper.cpp`, `src/piper_tts.cpp`, 必要なら `src/piper_core/openjtalk_wrapper.c` | `libonnxruntime_webassembly.a` を link でき、model / config / `cmudict_data.json` を含む resource 読み込みが path 非依存になり、unsupported backend と Phase 1 除外機能が説明可能な error を返す |
| 進行中 | `W4` | [TKT-011](./tickets/TKT-011-web-browser-smoke-ci-docs.md) | browser smoke / CI / 文書反映 | `.github/workflows/build.yml`, `test/project`, `test/prepare-assets.sh`, smoke 用 script 群, `README.md`, `addons/piper_plus/README.md` | COOP / COEP 前提の browser smoke が既存 test fixture と package 成果物を使って CI / local で再現可能になり、addon load と最小モデル synthesize の成否、Web の前提と制約が README と package 文書へ反映される |

<a id="m8"></a>
### M8 Release / Asset Library 準備

- 対象要求: release 完了条件の最終集約
- 状態: `進行中`
- 依存: `M2` `M4` `M5` `M6` `M7`
- 関連チケット: [TKT-002 Web Platform 対応](./tickets/TKT-002-web-platform.md) [TKT-003 macOS Packaged Smoke 確認](./tickets/TKT-003-macos-packaged-smoke.md) [TKT-004 Android Export / Runtime 確認](./tickets/TKT-004-android-export-runtime.md) [TKT-006 iOS Export / Link Smoke 確認](./tickets/TKT-006-ios-export-link-smoke.md) [TKT-011 Web browser smoke / CI / 文書反映](./tickets/TKT-011-web-browser-smoke-ci-docs.md) [TKT-007 Release Package / 文書最終化](./tickets/TKT-007-release-finalization.md)
- 現状: package / validator / README 類の基礎と multilingual の反映は完了しています。残るのは platform verification と Web 要求の確定待ちです。
- 残作業:
  - `M6` `M7` の結果を package / README / license / changelog に反映する
  - Asset Library 提出時の説明、同梱範囲、注意事項を最終化する
- 完了条件:
  - `docs/requirements.md` の release 完了条件をすべて閉じている
  - Asset Library へ申請できる package 導線が整っている

## 直近の実行順

1. [TKT-008 Web custom template / toolchain bootstrap](./tickets/TKT-008-web-template-toolchain-bootstrap.md) で、custom Web export template と Emscripten build の入口を固定する
2. [TKT-009 Web manifest / package / export preset 整備](./tickets/TKT-009-web-manifest-package-export-preset.md) で、`web.*` entry、package、validator、export preset を揃える
3. [TKT-010 Web runtime adaptation / ORT Web 対応](./tickets/TKT-010-web-runtime-ort-adaptation.md) で、Web 向け backend と path 非依存 I/O を実装する
4. [TKT-011 Web browser smoke / CI / 文書反映](./tickets/TKT-011-web-browser-smoke-ci-docs.md) で、browser smoke、CI、README 反映を閉じる
5. [TKT-003 macOS arm64 packaged addon smoke 確認](./tickets/TKT-003-macos-packaged-smoke.md) で、CI 実結果を確認し、必要修正を入れる
6. [TKT-004 Android arm64 export / runtime 確認](./tickets/TKT-004-android-export-runtime.md) で、export smoke / runtime 可否を確認し、必要なら `export_presets.cfg`、SDK / keystore、runtime 条件を修正する
7. [TKT-005 Windows Local Android Export Error 切り分け](./tickets/TKT-005-windows-android-export-error.md) で、generic configuration error を切り分ける
8. [TKT-006 iOS Export / Link Smoke 確認](./tickets/TKT-006-ios-export-link-smoke.md) で、CI 実結果を確認し、必要修正を入れる
9. [TKT-007 Release Package / 文書最終化](./tickets/TKT-007-release-finalization.md) で、`M6` `M7` の結果を package / 文書 / changelog に反映し、Asset Library 公開準備を閉じる

## ブロッカー / 未確定事項

- Web Phase 1 は `preview`、`CPU-only`、custom template 前提で進める方針を固定したが、Japanese text input / dictionary bootstrap を Phase 1 に含めるか、`TKT-010` でどの runtime scope を確定させるかは未確定です。
- macOS / Android / iOS の初回 CI 結果がまだ release 判定へ織り込まれていません。
- Windows local Android export の generic configuration error が Android 検証のノイズ源として残っています。

## 完了済みの要約

- runtime API、editor workflow、package assembly / validator の基礎実装は概ね完了しています。
- Windows packaged addon smoke と Linux headless strict CI は整備済みです。
- Android / iOS 向け export smoke script と CI job は追加済みで、残りは実結果の確定です。
- multilingual contract、capability matrix、matrix-first 検証、runtime capability/error API は完了済みで、成果は `tests/fixtures/` と `docs/generated/` に反映済みです。
