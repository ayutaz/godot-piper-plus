# TKT-025 Web 6-language runtime / Pages demo / smoke

- 状態: `未着手`
- 主マイルストーン: [M11 Windows / Web 6-Language Text Input / Template UX 完成](../milestones.md#m11)
- 関連マイルストーン: [M5 Quality Gate 完成](../milestones.md#m5) [M8 Release / Asset Library 準備](../milestones.md#m8) [M10 Web Japanese Support / Pages Japanese Demo 完成](../milestones.md#m10)
- 関連要求: `FR-3` `FR-10` `FR-11` `NFR-2` `NFR-5` `NFR-6`
- 親チケット: [`TKT-022`](./TKT-022-windows-web-six-language-support.md)
- 依存チケット: [`TKT-023`](./TKT-023-six-language-capability-template-contract.md) [`TKT-021`](./TKT-021-pages-japanese-demo-public-smoke.md)
- 後続チケット: [`TKT-026`](./TKT-026-six-language-docs-release-sync.md) [`TKT-007`](./TKT-007-release-finalization.md)

## 進捗

- [ ] Web export で 6 言語 text input / inspect / synthesize の acceptance を固定する
- [ ] browser smoke を 6 言語 sample set に拡張する
- [ ] Pages demo に 6 言語 selector と template text catalog を追加する
- [ ] public smoke と CI gate の実行戦略を 6 言語向けに固定する

## タスク目的とゴール

- Web export と GitHub Pages demo で、最低限 `ja/en/zh/es/fr/pt` の 6 言語 text input / inspect / synthesize を成立させる。
- ゴールは、Pages demo の言語 selector から各言語の template text を呼び出し、そのまま browser 上で合成確認できること。

## 実装する内容の詳細

- `M10` の `ja/en` baseline を拡張し、`pages_demo` と browser smoke が `TKT-023` の sample text catalog を読むようにする。
- `Web` preset の no-threads 側を 6 言語 synthesize gate の正本とし、`Web Threads` preset は引き続き non-blocking core regression を監視する。
- `zh` text input 用の asset / frontend 契約、`ja` の `naist-jdic` staging、`en` の `cmudict_data.json`、`es/fr/pt` rule-based path を Web artifact で再現できるようにする。
- `pages_demo` の selector は `ja/en/zh/es/fr/pt` を持ち、選択時に template text と required asset / status summary を更新できるようにする。
- CI は PR で最低限の 6 言語 gate を回し、`workflow_dispatch` / `main` deploy では public smoke まで含めた full matrix を通せる構成を目標にする。
- public smoke は `WEB_SMOKE summary=<json>` 相当の machine-readable summary を維持し、各言語 sample の pass / fail を artifact に残す。

## 提供範囲

- Web export と Pages demo の 6 言語 selector / template text UX。
- 6 言語 browser smoke と public smoke。
- Web 用 required asset と runtime summary の整理。

## テスト項目

- browser 上で 6 言語の text input / inspect / synthesize が成立すること。
- Pages demo の selector と template text が sample text catalog と一致すること。
- local / CI / public URL の 3 面で同じ sample text set を使えること。
- `ja/en` だけ通って `zh/es/fr/pt` が smoke 外に逃げていないこと。

## 実装する unit テスト

- Web smoke helper の scenario selection と 6 言語 summary 判定 test。
- Pages demo の language selector / template text binding test。

## 実装する e2e テスト

- `scripts/ci/export-web-smoke.sh` を使った 6 言語 browser smoke。
- `scripts/ci/run-pages-demo-smoke.mjs` を使った Pages demo の 6 言語 local / public smoke。

## 実装に関する懸念事項

- 6 言語 full matrix は PR CI の所要時間と artifact サイズを増やす。
- Web 上の `zh` text path は asset と frontend の切り方次第で日本語より難度が上がる可能性がある。
- Pages は cache / service worker の影響で sample text catalog や asset 更新が stale 化しやすい。

## レビューする項目

- 6 言語対応が Pages demo の UI 表示だけでなく browser synthesize gate に乗っているか。
- sample text catalog と public smoke の判定が drift していないか。
- PR / dispatch / main deploy で実行する matrix が曖昧なままになっていないか。

## 一から作り直すとしたらどうするか

- `pages_demo` を最初から 6 言語 locale-aware UI と sample text catalog 前提で作り、English-only / `ja/en` 追加の段階的拡張を避ける。

## 後続タスクに連絡する内容

- `TKT-026` へ、Web / Pages の 6 言語 sample text、required asset、CI / public smoke 手順を渡す。
- `TKT-007` へ、Web support 表記、Pages 公開 scope、known issue の最終文言を引き継ぐ。
