# TKT-019 Web 日本語 dictionary bootstrap / runtime

- 状態: `未着手`
- 主マイルストーン: [M10 Web Japanese Support / Pages Japanese Demo 完成](../milestones.md#m10)
- 関連マイルストーン: [M5 Quality Gate 完成](../milestones.md#m5)
- 関連要求: `FR-3` `FR-4` `FR-10` `NFR-2` `NFR-4` `NFR-5`
- 親チケット: [`TKT-018`](./TKT-018-web-japanese-support.md)
- 依存チケット: [`TKT-010`](./TKT-010-web-runtime-ort-adaptation.md) [`TKT-011`](./TKT-011-web-browser-smoke-ci-docs.md)
- 後続チケット: [`TKT-020`](./TKT-020-web-japanese-browser-smoke-ci.md) [`TKT-021`](./TKT-021-pages-japanese-demo-public-smoke.md)

## 進捗

- [ ] Web 向け `naist-jdic` asset policy を固定する
- [ ] export / Pages artifact に dictionary を staged する
- [ ] Web runtime contract から日本語 text input の exclusion を外す
- [ ] Japanese inspect / synthesize の success と failure を machine-readable にする
- [ ] `TKT-020` と `TKT-021` が使う fixture / sample / error surface を handoff する

## タスク目的とゴール

- Web で日本語 text input を成立させるための dictionary bootstrap と runtime contract を実装する。
- ゴールは、`naist-jdic` を Web export artifact と GitHub Pages artifact の両方へ再現可能に含め、日本語 text input / inspect / synthesize が browser 上で成立すること。

## 実装する内容の詳細

- `naist-jdic` を package には含めず、Web export / Pages demo 用 staged asset として扱う方針を固定する。
- `scripts/ci/prepare-pages-demo-assets.sh`、`test/prepare-assets.sh`、必要なら validator / manifest helper を更新し、日本語 dictionary の staging 条件を追加する。
- runtime 側で `dictionary_path` と Web resource load の両立を整理し、日本語 text input の contract を `get_runtime_contract()` と `get_last_error()` に反映する。
- `multilingual-test-medium` か日本語専用 model のどちらを smoke / demo の正本にするかを固定し、`language_code=ja` と sample text を deterministic に扱える形へ揃える。
- `openjtalk-native` を使わない Web path で、dictionary 欠落時の error と成功時の inspect / synthesize 結果を説明可能にする。

## 実装するために必要なエージェントチームの役割と人数

- `C++ runtime engineer` x1: runtime contract、dictionary load、error handling を担当する。
- `asset pipeline engineer` x1: staging script、manifest、validator を担当する。
- `QA engineer` x1: sample asset と deterministic test fixture を担当する。
- `review engineer` x1: package 境界と Web-only asset policy を確認する。

## 提供範囲

- Web 日本語 dictionary bootstrap。
- 日本語 text input / inspect / synthesize の runtime contract。
- `TKT-020` と `TKT-021` が再利用する fixture / asset 構成。

## テスト項目

- Web export artifact が日本語用 dictionary asset を含むこと。
- dictionary がある場合に日本語 inspect / synthesize が成立すること。
- dictionary がない場合に説明可能な error を返すこと。
- `get_runtime_contract()` と README が同じ日本語 support 条件を表すこと。

## 実装する unit テスト

- Web runtime contract で日本語 text input の可否と required asset を返す test を追加する。
- dictionary path / resource resolution の script-level または C++ test を追加する。
- dictionary 欠落時の `get_last_error()` を確認する test を追加する。

## 実装する e2e テスト

- `test/project` を使った Web export 後の日本語 inspect / synthesize smoke を実行する。
- `TKT-020` が使う browser smoke に渡す成功ログと error surface を確定する。

## 実装に関する懸念事項

- `naist-jdic` のサイズが大きく、Web 初回ロードと artifact サイズに影響する。
- dictionary の展開形式次第で FileAccess と export artifact の整合が崩れる可能性がある。
- model と dictionary の組み合わせ次第で Pages demo の初期化が遅くなる可能性がある。

## レビューする項目

- package に `naist-jdic` を混入させず、Web artifact にだけ載せる境界が守られているか。
- runtime contract と error surface が日本語 path を正しく表現しているか。
- English path の既存 smoke を壊していないか。

## 一から作り直すとしたらどうするか

- dictionary bootstrap を最初から `asset manifest + capability contract` で扱い、README と runtime が同じ metadata を参照するようにする。
- `dictionary_path` そのものではなく、Web 用は asset key ベースで解決する抽象層を先に切る。

## 後続タスクに連絡する内容

- `TKT-020` へ、日本語 smoke が見るべき sample text、成功ログ、失敗時 error を渡す。
- `TKT-021` へ、Pages demo が前提にすべき model / dictionary staging 条件を渡す。
- `TKT-007` へ、README と package 文書に書くべき必須 asset と既知制約を引き継ぐ。
