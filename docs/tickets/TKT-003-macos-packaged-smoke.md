# TKT-003 macOS arm64 packaged addon smoke 確認

- 状態: `要確認`
- 主マイルストーン: [M6 Platform Verification 完成](../milestones.md#m6)
- 関連マイルストーン: [M5 Quality Gate 完成](../milestones.md#m5), [M8 Release / Asset Library 準備](../milestones.md#m8)
- 関連要求: サポート対象 `macOS arm64`, release 完了条件の packaged addon smoke
- 依存チケット: なし
- 後続チケット: [`TKT-007`](TKT-007-release-finalization.md)

## 進捗

- [ ] macOS CI の packaged smoke 結果を確認する
- [ ] 必要なら dylib / dependency / package 差分を修正する
- [ ] 結果を文書へ反映する

## タスク目的とゴール

- macOS arm64 で packaged addon が実際にロードできるかを確定し、release gate を閉じる。
- build 成功だけではなく、package からの runtime load を smoke で通す。

## 実装する内容の詳細

- macOS runner で package assembly、validator、packaged smoke を順に実行し、失敗箇所を切り分ける。
- `addons/piper_plus/piper_plus.gdextension` の macOS dylib 参照と `libonnxruntime.macos.arm64.dylib` の同梱状態を確認する。
- 必要なら `scripts/ci/package-addon.sh` と `scripts/ci/smoke-test-packaged-addon.sh` の macOS 前提を補強する。
- dylib の load path、install name、runtime dependency の不足があれば package または build を修正する。
- 結果を `README.md` と `docs/milestones.md` に反映する。

## 実装するために必要なエージェントチームの役割と人数

- `macOS CI / runtime engineer` x1: CI 実行結果の解析と runtime load の切り分けを担当する。
- `package / binary engineer` x1: dylib 同梱、dependency、manifest 差分を担当する。
- `QA / docs engineer` x1: smoke 条件の確定と文書反映を担当する。

## 提供範囲

- CI / script: `scripts/ci/package-addon.sh`, `scripts/ci/smoke-test-packaged-addon.sh`
- manifest / package: `addons/piper_plus/piper_plus.gdextension`, package bin
- 文書: `README.md`, `docs/milestones.md`, 必要なら `CHANGELOG.md`

## テスト項目

- package された macOS addon が headless でロードできる。
- `libpiper_plus.macos.*.dylib` と `libonnxruntime.macos.arm64.dylib` が package と manifest で一致する。
- packaged smoke が false positive ではなく、`PiperTTS` 利用不可時に failure する。

## 実装する unit テスト

- package validator で macOS 向け dylib と dependency の存在を検証する。
- 必要なら macOS 専用に `otool -L` または同等チェックを追加し、依存解決を事前検出する。

## 実装する e2e テスト

- macOS runner で package 組み立て後に `scripts/ci/smoke-test-packaged-addon.sh` を実行する。
- `test/run-tests.sh` の failure 条件が package 状態でも有効であることを確認する。

## 実装に関する懸念事項

- dylib の install name や rpath が CI と local でずれる可能性がある。
- ONNX Runtime sidecar の命名差やコピー漏れが packaged runtime failure を起こしやすい。

## レビューをする項目

- macOS 特有の path 修正を他 platform の package assembly に混ぜて壊していないか。
- smoke test が単なる file existence ではなく runtime load を見ているか。
- 文書上の status 更新が CI 結果と一致しているか。

## ここまでのタスクで一から作り直すとしたらどうするか

- macOS build 完了時点で packaged smoke を必須にし、source build と package 検証を分離して管理する。
- package validator に dynamic library 依存確認を最初から入れる。

## 後続のタスクに連絡する内容

- `TKT-007` には CI run の成否、必要なら回避条件、最終的な macOS サポート表記を渡す。
- もし CI だけ失敗するなら、local と CI の差分条件を `M6` へ残す。
