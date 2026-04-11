# マイルストーン管理

更新日: 2026-04-11

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
| 進行中 | M4 Packaging / Documentation 完成 | `FR-9` `NFR-6` | [TKT-002](./tickets/TKT-002-web-platform.md) [TKT-004](./tickets/TKT-004-android-export-runtime.md) [TKT-011](./tickets/TKT-011-web-browser-smoke-ci-docs.md) [TKT-007](./tickets/TKT-007-release-finalization.md) | package assembly / validator、addon 文書、Web preview 制約反映は整備済み。残りは Android の既知制約と release 向け最終文書反映 |
| 進行中 | M5 Quality Gate 完成 | `NFR-1` `NFR-2` `NFR-3` `NFR-4` `NFR-5` | [TKT-002](./tickets/TKT-002-web-platform.md) [TKT-003](./tickets/TKT-003-macos-packaged-smoke.md) [TKT-004](./tickets/TKT-004-android-export-runtime.md) [TKT-005](./tickets/TKT-005-windows-android-export-error.md) [TKT-006](./tickets/TKT-006-ios-export-link-smoke.md) [TKT-008](./tickets/TKT-008-web-template-toolchain-bootstrap.md) [TKT-009](./tickets/TKT-009-web-manifest-package-export-preset.md) [TKT-010](./tickets/TKT-010-web-runtime-ort-adaptation.md) [TKT-011](./tickets/TKT-011-web-browser-smoke-ci-docs.md) | C++ test、headless strict 化、package validator、multilingual matrix-first 検証、Web browser smoke、macOS packaged smoke、iOS export smoke までは確認済み。残りは Android runtime / local 再現性の gate 化 |
| 進行中 | M6 Platform Verification 完成 | サポート対象 platform と release 完了条件 | [TKT-003](./tickets/TKT-003-macos-packaged-smoke.md) [TKT-004](./tickets/TKT-004-android-export-runtime.md) [TKT-005](./tickets/TKT-005-windows-android-export-error.md) [TKT-006](./tickets/TKT-006-ios-export-link-smoke.md) | Windows / Linux / macOS / iOS は概ね確定。Android は CI export smoke 済みで、残りは runtime 可否と Windows local 差分の確定 |
| 完了 | M7 Web Support 完成 | `FR-10` | [TKT-002](./tickets/TKT-002-web-platform.md) [TKT-008](./tickets/TKT-008-web-template-toolchain-bootstrap.md) [TKT-009](./tickets/TKT-009-web-manifest-package-export-preset.md) [TKT-010](./tickets/TKT-010-web-runtime-ort-adaptation.md) [TKT-011](./tickets/TKT-011-web-browser-smoke-ci-docs.md) | 2026-04-10 の GitHub Actions run `24223195868` で `Build Web`、browser smoke、README 反映を含む Phase 1 preview support の受け入れ条件を確認済み |
| 進行中 | M8 Release / Asset Library 準備 | release 完了条件の最終集約 | [TKT-004](./tickets/TKT-004-android-export-runtime.md) [TKT-005](./tickets/TKT-005-windows-android-export-error.md) [TKT-007](./tickets/TKT-007-release-finalization.md) | Web preview、macOS packaged smoke、iOS export smoke の結果までは反映済み。残りは Android の最終判定と changelog / Asset Library 文書の最終化 |
| 進行中 | M9 GitHub Pages Public Demo / Deploy | post-preview Web public demo / GitHub Pages deployment | [TKT-012](./tickets/TKT-012-web-github-pages-deploy.md) [TKT-013](./tickets/m9-github-pages/TKT-013-github-pages-scope-asset-policy.md) [TKT-014](./tickets/m9-github-pages/TKT-014-github-pages-preset-public-entry.md) [TKT-015](./tickets/m9-github-pages/TKT-015-github-pages-deploy-workflow.md) [TKT-016](./tickets/m9-github-pages/TKT-016-github-pages-public-url-smoke.md) [TKT-017](./tickets/m9-github-pages/TKT-017-github-pages-docs-operational-notes.md) | `M7` 完了後の follow-up。`GP0` から `GP4` の ticket 分解が完了し、scope、preset/public entry、deploy workflow、public URL smoke、docs/ops finalization の責務が個別化されています |

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
- 現状: `.gdextension` manifest ベースの package assembly / validator、addon README / LICENSE / third-party notice、package 範囲の整理、multilingual contract の文書反映、Web preview 制約の README 反映までは実施済みです。
- 残作業:
  - Android export / runtime の最終判定と Windows local 制約を package / README / changelog へ反映する
  - Asset Library 提出向けの release 文書を最終整形する
- 完了条件:
  - package に含めるもの / 含めないものが最終状態で一致している
  - runtime API と配布手順の文書が最終実装と一致している
  - Asset Library 提出物に転記できる説明が揃っている

<a id="m5"></a>
### M5 Quality Gate 完成

- 対象要求: `NFR-1` `NFR-2` `NFR-3` `NFR-4` `NFR-5`
- 状態: `進行中`
- 関連チケット: [TKT-002 Web Platform 対応](./tickets/TKT-002-web-platform.md) [TKT-003 macOS Packaged Smoke 確認](./tickets/TKT-003-macos-packaged-smoke.md) [TKT-004 Android Export / Runtime 確認](./tickets/TKT-004-android-export-runtime.md) [TKT-005 Windows Local Android Export Error 切り分け](./tickets/TKT-005-windows-android-export-error.md) [TKT-006 iOS Export / Link Smoke 確認](./tickets/TKT-006-ios-export-link-smoke.md) [TKT-008 Web custom template / toolchain bootstrap](./tickets/TKT-008-web-template-toolchain-bootstrap.md) [TKT-009 Web manifest / package / export preset 整備](./tickets/TKT-009-web-manifest-package-export-preset.md) [TKT-010 Web runtime adaptation / ORT Web 対応](./tickets/TKT-010-web-runtime-ort-adaptation.md) [TKT-011 Web browser smoke / CI / 文書反映](./tickets/TKT-011-web-browser-smoke-ci-docs.md) [TKT-007 Release Package / 文書最終化](./tickets/TKT-007-release-finalization.md)
- 現状: `compatibility_minimum = 4.4`、オフライン runtime 前提、C++ unit test 継続実行、Godot headless strict 化、package validator による binary / dependency 検証、multilingual matrix-first の C++ / headless 検証、Web browser smoke、macOS packaged smoke、iOS export smoke までは整っています。
- 残作業:
  - Android export success を超えた runtime 可否を quality gate に組み込む
  - Windows local Android export の既知制約と CI 差分を説明可能にする
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
| macOS arm64 | `完了` | 2026-04-10 の run `24223195868` で build / C++ test / packaged addon smoke を確認済み | 既存結果を維持し、release 文書へ反映する |
| Android arm64-v8a | `進行中` | 2026-04-10 の run `24223195868` で build / package / export smoke は確認済み。残りは runtime 可否と Windows local generic configuration error の切り分け | runtime 可否を確定し、local 差分と既知制約を反映する |
| iOS arm64 | `完了` | 2026-04-10 の run `24223195868` で build / export / link smoke を確認済み | 既存結果を維持し、release 文書へ反映する |

- 残作業:
  - Android export smoke / runtime 可否の確定と必要修正
  - Windows local Android export の generic configuration error 切り分け
- 完了条件:
  - サポート対象の desktop / mobile platform の成否が文書付きで確定している
  - CI と local の差分が説明可能な状態になっている

<a id="m7"></a>
### M7 Web Support 完成

- 対象要求: `FR-10`
- 状態: `完了`
- 関連チケット: [TKT-002 Web Platform 対応](./tickets/TKT-002-web-platform.md) [TKT-008 Web custom template / toolchain bootstrap](./tickets/TKT-008-web-template-toolchain-bootstrap.md) [TKT-009 Web manifest / package / export preset 整備](./tickets/TKT-009-web-manifest-package-export-preset.md) [TKT-010 Web runtime adaptation / ORT Web 対応](./tickets/TKT-010-web-runtime-ort-adaptation.md) [TKT-011 Web browser smoke / CI / 文書反映](./tickets/TKT-011-web-browser-smoke-ci-docs.md)
- 現状: feasibility と設計メモは [docs/web-platform-research.md](./web-platform-research.md) に整理済みです。`W0` として Phase 1 の scope は `preview support`、`CPU-only`、custom Web export template 前提、`web.*` manifest、addon load と最小モデル synthesize を見る CI browser smoke、制約文書化までで固定済みです。`W1` から `W4` は完了しており、2026-04-10 の GitHub Actions run `24223195868` で `Build Web` が成功し、`threads` / `no-threads` の両 browser smoke で `RESULT total=9 pass=4 fail=0 skip=5` と `WEB_SMOKE status=pass` を確認しました。README と addon README には Web preview の前提と制約を反映済みです。
- 実装スコープ:
  - Phase 1: `preview support`。custom template、`web.*` manifest、Web 向け runtime 制約、browser smoke、README 反映までを release gate に含める
  - Phase 2: Japanese text input の dictionary bootstrap、multilingual parity 拡張、binary size 最適化などの広がりは preview 後に扱う
- 残作業:
  - なし。Phase 2 項目は別要求として扱います。
- follow-up:
  - GitHub Pages 向け public demo / deploy は release gate 外の post-preview task として [`M9 GitHub Pages Public Demo / Deploy`](#m9) と [`TKT-012`](./tickets/TKT-012-web-github-pages-deploy.md) で追跡します。前提整理は [`docs/web-github-pages-plan.md`](./web-github-pages-plan.md) にまとめます。
- 完了条件:
  - Web export 向け build / package / export 導線が成立する
  - `web.*` manifest、package validator、test project export preset が同じ artifact 契約を参照している
  - runtime 可否と制約が明文化される
  - CI 上の browser smoke で addon load と最小モデル synthesize の成否を確認でき、同じ script を local でも再実行でき、README と addon README に制約が反映される
- 実装マイルストーン:

| 状態 | ID | チケット | マイルストーン | 主な変更対象 | 完了条件 |
|---|---|---|---|---|---|
| 完了 | `W0` | [TKT-002](./tickets/TKT-002-web-platform.md) | feasibility / scope 固定 | `docs/web-platform-research.md`, `docs/milestones.md`, `docs/tickets/TKT-002-web-platform.md` | Phase 1 を `preview support`、`CPU-only`、custom template、addon load と最小モデル synthesize を見る CI browser smoke 前提で固定し、`W1` から `W4` の分割を確定している |
| 完了 | `W1` | [TKT-008](./tickets/TKT-008-web-template-toolchain-bootstrap.md) | custom template / toolchain bootstrap | `CMakeLists.txt`, `cmake/HTSEngine.cmake`, `scripts/ci/install-godot-export-templates.sh`, 必要なら Web template build script | `dlink_enabled=yes` 前提の custom Web export template と Emscripten build の入口が再現でき、thread / no-thread の binary 方針、artifact 名、出力配置が固定され、成功 run で成立確認済み |
| 完了 | `W2` | [TKT-009](./tickets/TKT-009-web-manifest-package-export-preset.md) | manifest / package / export preset 整備 | `addons/piper_plus/piper_plus.gdextension`, `test/project/addons/piper_plus/piper_plus.gdextension`, `test/project/export_presets.cfg`, `scripts/ci/package-addon.sh`, `scripts/ci/validate-addon-package.sh`, `test/prepare-assets.sh` | `W1` で固定した Web artifact matrix を `web.*` entry、package、validator、test project の Web export preset へ矛盾なく反映でき、成功 run で成立確認済み |
| 完了 | `W3` | [TKT-010](./tickets/TKT-010-web-runtime-ort-adaptation.md) | runtime adaptation と ORT Web 対応 | `cmake/FindOnnxRuntime.cmake`, `src/piper_core/piper.cpp`, `src/piper_tts.cpp`, 必要なら `src/piper_core/openjtalk_wrapper.c` | `libonnxruntime_webassembly.a` を link でき、model / config / `cmudict_data.json` を含む resource 読み込みが path 非依存になり、unsupported backend と Phase 1 除外機能が説明可能な error を返し、成功 run で最小 synthesize まで確認済み |
| 完了 | `W4` | [TKT-011](./tickets/TKT-011-web-browser-smoke-ci-docs.md) | browser smoke / CI / 文書反映 | `.github/workflows/build.yml`, `test/project`, `test/prepare-assets.sh`, smoke 用 script 群, `README.md`, `addons/piper_plus/README.md` | COOP / COEP 前提の browser smoke が既存 test fixture と package 成果物を使って CI 上で再現され、addon load と最小モデル synthesize の成否、Web の前提と制約が README と package 文書へ反映済み |

<a id="m8"></a>
### M8 Release / Asset Library 準備

- 対象要求: release 完了条件の最終集約
- 状態: `進行中`
- 依存: `M2` `M4` `M5` `M6` `M7`
- 関連チケット: [TKT-002 Web Platform 対応](./tickets/TKT-002-web-platform.md) [TKT-003 macOS Packaged Smoke 確認](./tickets/TKT-003-macos-packaged-smoke.md) [TKT-004 Android Export / Runtime 確認](./tickets/TKT-004-android-export-runtime.md) [TKT-006 iOS Export / Link Smoke 確認](./tickets/TKT-006-ios-export-link-smoke.md) [TKT-011 Web browser smoke / CI / 文書反映](./tickets/TKT-011-web-browser-smoke-ci-docs.md) [TKT-007 Release Package / 文書最終化](./tickets/TKT-007-release-finalization.md)
- 現状: package / validator / README 類の基礎、multilingual の反映、Web preview support の結果反映、macOS packaged smoke と iOS export smoke の確定までは完了しています。残るのは Android の最終判定と release 文書の最終化です。
- 残作業:
  - `M6` の Android 結果と Windows local 制約を package / README / license / changelog に反映する
  - Asset Library 提出時の説明、同梱範囲、注意事項を最終化する
- 完了条件:
  - `docs/requirements.md` の release 完了条件をすべて閉じている
  - Asset Library へ申請できる package 導線が整っている

<a id="m9"></a>
### M9 GitHub Pages Public Demo / Deploy

- 対象要求: post-preview Web public demo / GitHub Pages deployment
- 状態: `進行中`
- 依存: `M7`
- 関連チケット: [TKT-012 Web GitHub Pages deploy / public demo](./tickets/TKT-012-web-github-pages-deploy.md) [TKT-013 GitHub Pages scope / asset policy 固定](./tickets/m9-github-pages/TKT-013-github-pages-scope-asset-policy.md) [TKT-014 GitHub Pages preset / public entry 整備](./tickets/m9-github-pages/TKT-014-github-pages-preset-public-entry.md) [TKT-015 GitHub Pages deploy workflow 整備](./tickets/m9-github-pages/TKT-015-github-pages-deploy-workflow.md) [TKT-016 GitHub Pages public URL smoke 整備](./tickets/m9-github-pages/TKT-016-github-pages-public-url-smoke.md) [TKT-017 GitHub Pages docs / operational notes 最終化](./tickets/m9-github-pages/TKT-017-github-pages-docs-operational-notes.md)
- 現状: GitHub Pages 対応の overview は [docs/web-github-pages-plan.md](./web-github-pages-plan.md) に反映済みです。`docs/tickets/m9-github-pages/` で `GP0` から `GP4` を個別チケット化済みで、scope / asset policy の固定を [`TKT-013`](./tickets/m9-github-pages/TKT-013-github-pages-scope-asset-policy.md) で進めています。Pages 向け preset、`index.html` export、deploy workflow、public URL smoke、公開文書は未実装です。
- 実装スコープ:
  - `no-threads`
  - `CPU-only`
  - English minimal demo
  - `index.html` export
  - PWA と cross-origin isolation workaround を有効にした Pages 向け preset
  - GitHub Actions による Pages artifact upload / deploy
  - deploy 後の public URL smoke
- 残作業:
  - 公開デモの scope、model 配布方針、hosting 前提を固定する
  - Pages 向け export target と workflow を実装する
  - public URL に対する smoke と文書反映を完了させる
- 完了条件:
  - GitHub Actions で Pages 向け Web export が成功する
  - 成功時 artifact が GitHub Pages に deploy される
  - 公開 URL で addon load と最小 synthesize が成立する
  - scope が `no-threads` / `CPU-only` / English minimal demo として実装と文書で一致している
  - `M7 Web Support` の release gate を reopen せず、post-preview follow-up として閉じられる
- 実装マイルストーン:

| 状態 | ID | チケット | マイルストーン | 主な変更対象 | 完了条件 |
|---|---|---|---|---|---|
| 進行中 | `GP0` | [TKT-013](./tickets/m9-github-pages/TKT-013-github-pages-scope-asset-policy.md) | scope / asset policy 固定 | `docs/web-github-pages-plan.md`, `docs/milestones.md`, `docs/tickets/TKT-012-web-github-pages-deploy.md`, `docs/tickets/m9-github-pages/TKT-013-github-pages-scope-asset-policy.md` | `no-threads` / `CPU-only` / English minimal demo、`en_US-ljspeech-medium` 1 モデル同梱、runtime download なし、notice 同梱、PWA workaround 前提が文書で固定されている |
| 未着手 | `GP1` | [TKT-014](./tickets/m9-github-pages/TKT-014-github-pages-preset-public-entry.md) | Pages 向け preset / public entry 整備 | `test/project/export_presets.cfg`, 公開 demo 用 project または asset 一式, 必要なら export 補助 script | Pages 向け preset が `index.html`、`no-threads`、PWA 有効を満たし、公開入口が固定されている |
| 未着手 | `GP2` | [TKT-015](./tickets/m9-github-pages/TKT-015-github-pages-deploy-workflow.md) | Pages deploy workflow 整備 | `.github/workflows/build.yml` または Pages 専用 workflow, deploy 補助 script | `configure-pages`、artifact upload、deploy が CI 上で成立する |
| 未着手 | `GP3` | [TKT-016](./tickets/m9-github-pages/TKT-016-github-pages-public-url-smoke.md) | public URL smoke 整備 | smoke 用 script 群, 必要なら Playwright helper, workflow の deploy 後 step | deploy 後の `page_url` で addon load と最小 synthesize を確認できる |
| 未着手 | `GP4` | [TKT-017](./tickets/m9-github-pages/TKT-017-github-pages-docs-operational-notes.md) | 文書 / 運用メモ最終化 | `README.md`, `docs/web-github-pages-plan.md`, 関連 docs | 公開 URL の scope、既知制約、cache / service worker 注意点が文書へ反映されている |

## 直近の実行順

1. [TKT-004 Android arm64 export / runtime 確認](./tickets/TKT-004-android-export-runtime.md) で、export smoke 済みの状態から runtime 可否を確定する
2. [TKT-005 Windows Local Android Export Error 切り分け](./tickets/TKT-005-windows-android-export-error.md) で、generic configuration error を切り分ける
3. [TKT-007 Release Package / 文書最終化](./tickets/TKT-007-release-finalization.md) で、`M6` `M7` の結果を package / 文書 / changelog に反映し、Asset Library 公開準備を閉じる

## post-preview Web の実行順

1. [TKT-013 GitHub Pages scope / asset policy 固定](./tickets/m9-github-pages/TKT-013-github-pages-scope-asset-policy.md) で、公開 scope と model 配布方針を固定する
2. [TKT-014 GitHub Pages preset / public entry 整備](./tickets/m9-github-pages/TKT-014-github-pages-preset-public-entry.md) と [TKT-015 GitHub Pages deploy workflow 整備](./tickets/m9-github-pages/TKT-015-github-pages-deploy-workflow.md) で、Pages 向け preset / public entry と deploy workflow を揃える
3. [TKT-016 GitHub Pages public URL smoke 整備](./tickets/m9-github-pages/TKT-016-github-pages-public-url-smoke.md) と [TKT-017 GitHub Pages docs / operational notes 最終化](./tickets/m9-github-pages/TKT-017-github-pages-docs-operational-notes.md) で、public URL smoke と文書反映を閉じる

## ブロッカー / 未確定事項

- Web Phase 1 は `preview support` として確定済みで、Japanese text input / dictionary bootstrap は Phase 2 扱いです。
- Android は CI export smoke 成功後も runtime 可否が未確定で、release 判定へ残っています。
- Windows local Android export の generic configuration error が Android 検証のノイズ源として残っています。

## 完了済みの要約

- runtime API、editor workflow、package assembly / validator の基礎実装は概ね完了しています。
- Windows packaged addon smoke、Linux headless strict CI、macOS packaged addon smoke、iOS export smoke は整備と実結果確認が完了しています。
- 2026-04-10 の GitHub Actions run `24223195868` で Web preview の `Build Web` と browser smoke は `threads` / `no-threads` の両方で `WEB_SMOKE status=pass` を確認済みです。
- multilingual contract、capability matrix、matrix-first 検証、runtime capability/error API は完了済みで、成果は `tests/fixtures/` と `docs/generated/` に反映済みです。

## release gate 外の follow-up

- Web preview support 自体は完了済みです。GitHub Pages 上で動く public demo / deploy は独立マイルストーン [`M9 GitHub Pages Public Demo / Deploy`](#m9) と [`TKT-012`](./tickets/TKT-012-web-github-pages-deploy.md) で追跡し、技術整理は [`docs/web-github-pages-plan.md`](./web-github-pages-plan.md) にまとめます。
