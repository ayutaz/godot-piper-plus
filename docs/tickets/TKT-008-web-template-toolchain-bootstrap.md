# TKT-008 Web custom template / toolchain bootstrap

- 状態: `進行中`
- 主マイルストーン: [M7 Web Support 完成](../milestones.md#m7)
- 関連マイルストーン: [M5 Quality Gate 完成](../milestones.md#m5)
- 関連要求: `FR-10` `NFR-5`
- 親チケット: [`TKT-002`](TKT-002-web-platform.md)
- 依存チケット: なし
- 後続チケット: [`TKT-009`](TKT-009-web-manifest-package-export-preset.md) [`TKT-010`](TKT-010-web-runtime-ort-adaptation.md)

## 進捗

- [x] custom Web export template の build 前提と採用する Godot / Emscripten 条件を固定する
- [x] `dlink_enabled=yes` 前提の build bootstrap を再現可能な形にする
- [x] addon 側の thread / no-thread binary 方針と命名規約を固定する
- [x] custom template artifact の出力配置と後続チケットが参照するパス契約を固定する
- [x] 後続チケットへ渡す toolchain 前提と artifact 規約を文書化する

## タスク目的とゴール

- custom Web export template と addon side module build の入口を作り、Web 実装を実際に着手できる土台を整える。
- ゴールは、`dlink_enabled=yes` 前提の custom template と Emscripten build の再現条件が固定され、後続チケットがその前提で実装を進められること。

## 実装メモ

- `build-web` workflow では Godot 4.4 系に合わせて `EMSDK_VERSION=3.1.62` を固定しました。
- `scripts/ci/build-godot-web-templates.sh`、`scripts/ci/build-web-side-module.sh`、`scripts/ci/install-godot-export-templates.sh`、README の local smoke 手順で、custom template artifact 名と配置契約を後続チケットへ共有しています。

## 実装する内容の詳細

- `CMakeLists.txt` に Web 向け toolchain と外部依存への引き渡し方針を追加する。
- `cmake/HTSEngine.cmake` で Web を CMake build 系へ含め、autotools 側へ落ちないように整理する。
- custom Web export template の build 入口を定義し、`dlink_enabled=yes` と thread / no-thread の扱いを固定する。
- 既存の `scripts/ci/install-godot-export-templates.sh` を拡張するか、別の Web template build script を追加し、local / CI 共通の template 配置先を固定する。
- `godot-cpp` / Emscripten / Godot template の version 前提を文書化し、後続チケットの前提にする。
- `TKT-009` と `TKT-011` が参照する artifact 名、出力 directory、thread / no-thread matrix を handoff 用に固定する。

## 実装するために必要なエージェントチームの役割と人数

- `build / toolchain engineer` x1: Godot、Emscripten、CMake の接続を担当する。
- `dependency engineer` x1: 外部依存の cross compile 条件と命名規約を担当する。
- `review engineer` x1: 非 Web platform への副作用と再現性を確認する。

## 提供範囲

- Web custom template build の前提条件と再現手順。
- addon 側 Web build の入口。
- thread / no-thread binary 方針、artifact 命名規約、template 配置規約。

## テスト項目

- Web 向け CMake configure が成立し、外部依存へ toolchain 情報を渡せること。
- custom Web export template の build 手順が再現できること。
- thread / no-thread の binary 方針、artifact 名、template 配置先が後続の manifest 設計に使える形で固定されていること。

## 実装する unit テスト

- Web 向け CMake option と external dependency 引き渡しを確認する configure smoke を追加または更新する。
- `scripts/ci/install-godot-export-templates.sh` または追加する bootstrap script が、入力不足や未設定環境を説明可能に fail することを確認する。

## 実装する e2e テスト

- local または CI で custom Web export template build を 1 回通し、artifact が期待した場所へ出ることを確認する。
- addon 側の Web build を最小構成で走らせ、thread / no-thread 方針に沿った artifact 名と出力配置になることを確認する。

## 実装に関する懸念事項

- Emscripten の dynamic linking と pthreads の組み合わせが不安定な可能性がある。
- HTS Engine や他依存が Web cross compile 前提を満たさない可能性がある。
- Godot 側 template build 条件が version 依存で変わると再現性が落ちる。

## レビューする項目

- Web 向けフラグや toolchain 分岐が既存 desktop / mobile build を壊していないか。
- custom template の前提条件が文書と script で一致しているか。
- 後続の manifest / package 設計に必要な artifact 命名規約が十分に固定されているか。

## 一から作り直すとしたらどうするか

- 最初から Web build を専用 preset と wrapper script で分離し、既存 platform 分岐へ後付けしない構成にする。
- template build と addon build の出力 metadata を 1 つの source に集約し、命名規約の手作業同期を避ける。

## 後続のタスクに連絡する内容

- `TKT-009` へ、採用した artifact 名、thread / no-thread 方針、template 前提を渡す。
- `TKT-011` へ、CI が取得・配布する custom template artifact 名、配置先、install 手順を渡す。
- `TKT-010` へ、toolchain 引き渡し方法、Web で使える external dependency 条件、避けるべき linker 前提を渡す。
- `TKT-002` へ、Phase 1 の build bootstrap が成立した時点で M7 の進捗更新を返す。
