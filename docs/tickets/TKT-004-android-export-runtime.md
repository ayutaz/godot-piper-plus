# TKT-004 Android arm64 export / runtime 確認

- 状態: `進行中`
- 主マイルストーン: [M6 Platform Verification 完成](../milestones.md#m6)
- 関連マイルストーン: [M5 Quality Gate 完成](../milestones.md#m5), [M8 Release / Asset Library 準備](../milestones.md#m8)
- 関連要求: サポート対象 `Android arm64-v8a`, release 完了条件の Android export smoke / runtime 可否
- 依存チケット: なし
- 後続チケット: [`TKT-005`](TKT-005-windows-android-export-error.md), [`TKT-007`](TKT-007-release-finalization.md)

## 進捗

- [x] Android export smoke の CI 結果を確定する
- [x] export 条件と package bin の不足を修正する
- [ ] runtime 可否を確認する
- [x] CI export smoke の結果と未確定な runtime 条件を文書へ反映する

## タスク目的とゴール

- Android arm64-v8a 向け export smoke と runtime 可否を確定し、release gate を閉じる。
- APK 生成だけではなく、native library 同梱と最小 runtime 成立を確認する。

## 実装メモ

- 2026-04-10 の GitHub Actions run `24223195868` で `Android Export Smoke` は `success` でした。package、export preset、SDK/bootstrap 導線、APK 生成までは CI で成立しています。
- 残件は、export 成功を超えた runtime 可否の最終確認です。Windows local の generic configuration error は [`TKT-005`](TKT-005-windows-android-export-error.md) に切り分け済みです。

## 実装する内容の詳細

- `scripts/ci/export-android-smoke.sh` と `scripts/ci/export-android-smoke.ps1` の SDK / keystore / editor settings 解決を点検する。
- `test/project/export_presets.cfg` の Android preset が CI と local の両方で成立するかを確認する。
- package に `libpiper_plus.android.template_*.so` と `libonnxruntime.android.arm64.so` が揃うようにする。
- `test/prepare-assets.sh` 経由の asset 展開と `PIPER_ADDON_SRC` / `PIPER_ADDON_BIN_SRC` 注入を検証する。
- 実機または emulator で runtime まで見る場合は、最低限 `initialize()` と class load の成否を確認する。

## 実装するために必要なエージェントチームの役割と人数

- `Android export engineer` x1: export preset、SDK、templates、keystore の成立条件を担当する。
- `package / native runtime engineer` x1: APK 内の native library 同梱と runtime load を担当する。
- `QA / automation engineer` x1: smoke script と runtime 確認手順の自動化を担当する。
- `docs / release engineer` x1: 結果の文書反映を担当する。

## 提供範囲

- script: `scripts/ci/export-android-smoke.sh`, `scripts/ci/export-android-smoke.ps1`, `scripts/ci/install-godot-export-templates.sh`
- 設定: `test/project/export_presets.cfg`
- package / manifest: `addons/piper_plus/piper_plus.gdextension`, package bin
- 文書: `README.md`, `docs/milestones.md`, 必要なら `CHANGELOG.md`

## テスト項目

- headless Android export が CI で APK を生成できる。
- APK に `libpiper_plus` と `onnxruntime` が含まれる。
- export 前提の SDK / keystore / editor settings が再現可能である。
- runtime で `PiperTTS` class load または `initialize()` が成立する。
- NNAPI 不可環境でも CPU fallback の説明が可能である。

## 実装する unit テスト

- script レベルで SDK path、keystore path、editor settings 更新の欠落を検出するチェックを追加する。
- package validator で Android binary と `libonnxruntime.android.arm64.so` の存在を検証する。
- 必要なら GDScript に Android provider / fallback の最小検証を追加する。

## 実装する e2e テスト

- `scripts/ci/export-android-smoke.sh` により APK を生成し、`unzip -l` で native library 同梱を確認する。
- 可能なら emulator / device 上で exported app を起動し、addon load と最小初期化を確認する。
- Windows local と CI の差分がある場合は同じ project / preset / package で再現比較する。

## 実装に関する懸念事項

- Godot export templates、Android SDK、Java、keystore の組み合わせで環境差が出やすい。
- export が通っても runtime load で missing `.so` が出る可能性がある。
- CI で emulator runtime まで回せない場合、runtime 可否の確度が下がる。

## レビューをする項目

- APK 生成成功を runtime 成功と誤認していないか。
- `.sh` と `.ps1` の条件差が無視されていないか。
- Android 向け sidecar のコピー元が不安定な探索に依存しすぎていないか。
- README の Android サポート表記が実測結果と一致しているか。

## ここまでのタスクで一から作り直すとしたらどうするか

- Android export smoke を package validator と同じく artifact 入力前提で早期に作る。
- global editor settings を直接触らず、project-local な export 設定注入を優先する。
- runtime 確認用の最小 Android scene / log probe を最初から用意する。

## 後続のタスクに連絡する内容

- `TKT-005` には Windows local の再現条件と CI との差分を渡す。
- `TKT-007` には export 成否、runtime 可否、既知制約、必要な README 注記を渡す。
