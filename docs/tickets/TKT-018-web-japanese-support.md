# TKT-018 Web 日本語対応

- 状態: `進行中`
- 主マイルストーン: [M10 Web Japanese Support / Pages Japanese Demo 完成](../milestones.md#m10)
- 関連マイルストーン: [M4 Packaging / Documentation 完成](../milestones.md#m4) [M5 Quality Gate 完成](../milestones.md#m5) [M8 Release / Asset Library 準備](../milestones.md#m8)
- 関連要求: `FR-3` `FR-4` `FR-10` `NFR-2` `NFR-5` `NFR-6`
- 子チケット: [`TKT-019`](./TKT-019-web-japanese-dictionary-bootstrap.md) [`TKT-020`](./TKT-020-web-japanese-browser-smoke-ci.md) [`TKT-021`](./TKT-021-pages-japanese-demo-public-smoke.md)
- 依存チケット: [`TKT-010`](./TKT-010-web-runtime-ort-adaptation.md) [`TKT-011`](./TKT-011-web-browser-smoke-ci-docs.md)
- 後続チケット: [`TKT-007`](./TKT-007-release-finalization.md)

## 進捗

- [x] 日本語 Web 対応を must follow-up として `M10` へ昇格する
- [x] `TKT-019` から `TKT-021` の責務を分割する
- [ ] Web 日本語 runtime の asset policy と acceptance を固定する
- [ ] Japanese browser smoke / CI gate を閉じる
- [ ] GitHub Pages 日本語 demo / public smoke を閉じる
- [ ] `TKT-007` へ release 文書化条件を handoff する

## タスク目的とゴール

- Web preview と English-only Pages demo の上に、日本語 text input と合成を must requirement として成立させる。
- ゴールは、Web export と GitHub Pages の両方で `naist-jdic` を前提に日本語 text input / inspect / synthesize を再現可能にし、CI と文書でそれを保証すること。

## 実装する内容の詳細

- `TKT-019` で `naist-jdic` の staging、bootstrap、runtime contract、失敗時 error、asset 境界を固定する。
- `TKT-020` で local / CI browser smoke に Japanese scenario を追加し、既存 English smoke と同じ entrypoint / log 採取を維持する。
- `TKT-021` で GitHub Pages demo を English-only から日本語入力を含む UI へ拡張し、public URL smoke を日本語シナリオまで広げる。
- `TKT-007` が release 文書で迷わないように、必須 asset、既知制約、scope 表現、英語最小構成からの差分を明文化する。
- `M7` と `M9` の完了実績は残しつつ、post-preview の mandatory track として `M10` へ進捗を集約する。

## エージェントチームの役割と人数

| 役割 | 人数 | 主責務 |
|---|---:|---|
| tech lead / coordinator | 1 | `TKT-019` から `TKT-021` の依存、acceptance、handoff を管理する |
| runtime engineer | 1 | dictionary bootstrap と Web runtime contract を管理する |
| QA / CI engineer | 1 | Japanese browser smoke と CI gate を管理する |
| demo / UX engineer | 1 | Pages 日本語 demo の UI と public smoke を管理する |
| review lead | 1 | must requirement の表現、asset 境界、release handoff を確認する |

## 提供範囲

- Web 日本語対応の責務分解と acceptance。
- `naist-jdic` を含む Web asset policy。
- Japanese browser smoke、Pages public smoke、release 文書反映までの handoff。

## テスト項目

- Web export で日本語用 dictionary asset を再現可能な形で扱えること。
- browser 上で日本語 text input の inspect / synthesize が成立すること。
- local / CI / public URL の 3 つで同じ pass 条件を使えること。
- README と release 文書が English-only scope のまま残らないこと。

## 実装する unit テスト

- `TKT-019` の runtime / asset resolution test、`TKT-020` の smoke helper test、`TKT-021` の artifact validator を acceptance に含める。
- この umbrella ticket 自体では専用 unit test を追加せず、child ticket の pass 条件を集約する。

## 実装する e2e テスト

- `TKT-020` の Japanese browser smoke。
- `TKT-021` の GitHub Pages public URL smoke。
- `TKT-007` 前の package / docs 最終確認。

## 実装に関する懸念事項

- `naist-jdic` の payload と bootstrap 時間が Web 初回起動を重くする可能性がある。
- browser cache / service worker の影響で dictionary 更新が stale 化しやすい。
- Web 上の日本語 IME 入力と backend の日本語 text path は別問題なので、UI と synthesize の両方を見ないと false positive になる。

## レビューする項目

- 日本語 Web 対応が「候補」ではなく must requirement として表現されているか。
- `TKT-019` から `TKT-021` の責務境界が重複していないか。
- release 文書が English-only Pages demo を最終状態として誤認させないか。

## 一から作り直すとしたらどうするか

- 最初から Web を English track と Japanese track に分けた capability contract で管理し、asset bootstrap と smoke を別々に設計する。
- Pages demo も最初から multi-locale UI と smoke selector を持つ構成にして、後付け拡張を減らす。

## 後続タスクに連絡する内容

- `TKT-019` へ、dictionary bootstrap と runtime contract を最優先で閉じることを渡す。
- `TKT-020` と `TKT-021` へ、Japanese scenario は local / CI / public URL の 3 面で同じ sample と判定を使う方針を渡す。
- `TKT-007` へ、Web 日本語対応が release 文書の必須入力になったことを引き継ぐ。
