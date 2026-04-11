# TKT-002 Web Platform 対応

- 状態: `完了`
- 主マイルストーン: [M7 Web Support 完成](../milestones.md#m7)
- 関連マイルストーン: [M4 Packaging / Documentation 完成](../milestones.md#m4) [M5 Quality Gate 完成](../milestones.md#m5) [M8 Release / Asset Library 準備](../milestones.md#m8)
- 関連要求: `FR-10` `FR-9` `NFR-5` `NFR-6`
- 子チケット: [`TKT-008`](TKT-008-web-template-toolchain-bootstrap.md) [`TKT-009`](TKT-009-web-manifest-package-export-preset.md) [`TKT-010`](TKT-010-web-runtime-ort-adaptation.md) [`TKT-011`](TKT-011-web-browser-smoke-ci-docs.md)
- 後続チケット: [TKT-007](./TKT-007-release-finalization.md)

## 進捗

- [x] Phase 1 のスコープを `preview`、`CPU-only`、custom template 前提で固定する
- [x] Web backend と制約の feasibility を確定する
- [x] `W0` として調査結果、受け入れ条件、子チケット分割を `docs/` へ固定する
- [x] [`TKT-008`](TKT-008-web-template-toolchain-bootstrap.md) として `W1` custom template / toolchain bootstrap の実装と handoff を反映する
- [x] [`TKT-009`](TKT-009-web-manifest-package-export-preset.md) として `W2` `web.*` manifest / package / export preset の実装と handoff を反映する
- [x] [`TKT-010`](TKT-010-web-runtime-ort-adaptation.md) として `W3` ONNX Runtime Web static-lib と runtime I/O refactor の実装と runtime scope を反映する
- [x] [`TKT-011`](TKT-011-web-browser-smoke-ci-docs.md) として、重い Web build / export / browser smoke の CI 結果と local 再現 entrypoint を確定する

## 技術調査

- `W0` の調査結果と判断は、このチケット本文と [docs/milestones.md](../milestones.md) の `M7` セクションに反映しています。
- 現時点の推奨方針は、`preview` 扱いの Web 対応として custom dlink template、CPU-only backend、addon load と最小モデル synthesize を見る browser smoke を先に閉じることです。
- このチケットは umbrella と受け入れ条件の管理が責務で、実装本体は `TKT-008` から `TKT-011` が担当します。
- 2026-04-10 の GitHub Actions run `24223195868` で `Build Web` が成功し、`threads` / `no-threads` の両 browser smoke で `RESULT total=9 pass=4 fail=0 skip=5` と `WEB_SMOKE status=pass` を確認しました。

## 実装マイルストーン

| ID | チケット | 状態 | 目的 | 主な変更対象 | 完了条件 | 依存 |
|---|---|---|---|---|---|---|
| `W0` | [`TKT-002`](TKT-002-web-platform.md) | `完了` | feasibility と Phase 1 scope を固定し、分割チケットの境界を確定する | `docs/milestones.md`, `docs/tickets/TKT-002-web-platform.md` | `preview`、`CPU-only`、custom template、addon load と最小モデル synthesize を見る CI browser smoke 前提を固定し、`W1` から `W4` の責務分担が定義済み | - |
| `W1` | [`TKT-008`](TKT-008-web-template-toolchain-bootstrap.md) | `完了` | custom Web export template と Emscripten build の入口を作る | `CMakeLists.txt`, `cmake/HTSEngine.cmake`, `scripts/ci/install-godot-export-templates.sh`, 必要なら Web template build script | `dlink_enabled=yes` 前提の custom template が再現でき、thread / no-thread の binary 方針、artifact 名、出力配置が固定され、`W4` の成功 run で成立確認済み | - |
| `W2` | [`TKT-009`](TKT-009-web-manifest-package-export-preset.md) | `完了` | `web.*` manifest、package、validator、export preset を揃える | `addons/piper_plus/piper_plus.gdextension`, `test/project/addons/piper_plus/piper_plus.gdextension`, `test/project/export_presets.cfg`, `scripts/ci/package-addon.sh`, `scripts/ci/validate-addon-package.sh`, `test/prepare-assets.sh` | `W1` で固定した Web artifact matrix を manifest、package、validator、export preset へ反映でき、`W4` の成功 run で成立確認済み | `W1` |
| `W3` | [`TKT-010`](TKT-010-web-runtime-ort-adaptation.md) | `完了` | Web 向け runtime backend と resource I/O を成立させる | `cmake/FindOnnxRuntime.cmake`, `src/piper_core/piper.cpp`, `src/piper_tts.cpp`, 必要なら `src/piper_core/openjtalk_wrapper.c` | `libonnxruntime_webassembly.a` を link でき、model / config / `cmudict_data.json` を含む path 非依存 load と unsupported backend error が実装され、`W4` の成功 run で最小 synthesize まで確認済み | `W1` `W2` |
| `W4` | [`TKT-011`](TKT-011-web-browser-smoke-ci-docs.md) | `完了` | browser smoke と CI、README 反映で preview support を閉じる | `.github/workflows/build.yml`, `test/project`, `test/prepare-assets.sh`, smoke script 群, `README.md`, `addons/piper_plus/README.md` | browser smoke が既存 test fixture と package 成果物で CI 上再現され、addon load と最小モデル synthesize の成否、Web の前提と制約が利用者向け文書に反映済み | `W2` `W3` |

## タスク目的とゴール

- Web Phase 1 の受け入れ条件と子チケット間の handoff を固定し、`M7` と `W0` から `W4` の進捗を一貫して追跡できる状態にする。
- ゴールは、Web で何が動くか、何が制約か、どの子チケットが何を閉じるかを実装計画と文書で矛盾なく管理すること。

## 実装する内容の詳細

- `TKT-008` から `TKT-011` の完了条件、依存、handoff 内容を同期し、責務が重複しないように保つ。
- `docs/milestones.md` の `M7` と `W0` から `W4` の進捗を child ticket と同時更新できる状態にする。
- Phase 1 の `preview`、`CPU-only`、custom template、CI browser smoke、文書化という受け入れ条件を固定し続ける。
- `W1` の artifact 契約、`W3` の runtime scope / resource 契約、`W4` の smoke / 文書条件を release gate として接続する。
- `TKT-007` と `M4` `M5` `M8` に引き渡す release gate、既知制約、README 反映範囲を管理する。
- 実装本体の詳細は `TKT-008` から `TKT-011` に委譲し、このチケットでは cross-ticket decision と acceptance を扱う。

## エージェントチームの役割と人数

| 役割 | 人数 | 主責務 |
|---|---:|---|
| tech lead / coordinator | 1 | `W0` から `W4` の依存、acceptance、進捗同期を管理する |
| build track lead | 1 | `TKT-008` と `TKT-009` の handoff と artifact 契約を管理する |
| runtime track lead | 1 | `TKT-010` の runtime scope と unsupported 条件を管理する |
| QA / docs lead | 1 | `TKT-011` と `TKT-007` への引き渡し条件を管理する |
| review lead | 1 | release gate と preview 表記の妥当性を確認する |

## 提供範囲

- `M7` の進捗と `W0` から `W4` の責務境界。
- Phase 1 preview support の受け入れ条件と既知制約。
- `M4` `M5` `M8` `TKT-007` へ渡す release gate 情報。

## テスト項目

- `TKT-008` から `TKT-011` の完了条件が `FR-10` の acceptance を過不足なく覆っていること。
- child ticket 間の artifact 契約、runtime signal、README 反映条件が矛盾していないこと。
- CI browser smoke と local 再現手順が同じ判定基準を共有していること。
- `TKT-007` に渡す preview scope と既知制約が確定していること。

## 実装する unit テスト

- この umbrella ticket 自体でコード向け unit test は追加しない。
- 代わりに `TKT-009` の manifest / validator test、`TKT-010` の runtime unit test、`TKT-011` の smoke helper test が `FR-10` の acceptance を満たすかを確認対象にする。

## 実装する e2e テスト

- `TKT-008` から `TKT-011` が追加する export、runtime、browser smoke の e2e が順に接続され、`W4` 完了時に最終 CI smoke と local 再現が同じ判定で通ることを確認する。
- この umbrella ticket 自体では専用の e2e を増やさず、child ticket が追加した e2e の受け入れ結果を集約する。

## 実装に関する懸念事項

- GDExtension と ONNX Runtime の Web 対応境界が不明確な可能性がある。
- 実行可能でも性能要件を満たさない可能性がある。
- browser 制約により desktop/mobile と同じ API 保証が難しい可能性がある。

## レビューする項目

- Web Phase 1 が `preview support` として一貫して表現されているか。
- unsupported 機能が明示され、黙って失敗しないか。
- package / validator / README の 3 者が一致しているか。
- CI と local が同じ browser smoke 条件を見ているか。

## 一から作り直すとしたらどうするか

- 最初に Web を別 delivery target として切り分け、desktop/mobile と同じ binary/package 前提を持ち込まない。
- runtime 機能も feature flag で段階開放し、load-only、inspect-only、synthesis-enabled を明確に分ける。

## 後続タスクに連絡する内容

- `TKT-007` へ、Web Phase 1 は `preview support` で固定し、custom template 前提、`CPU-only`、addon load と最小モデル synthesize を pass 条件にすることを渡す。
- `M5` 向けに Web smoke の pass 条件と失敗時ログ採取方法を共有する。
