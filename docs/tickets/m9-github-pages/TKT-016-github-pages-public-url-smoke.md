# TKT-016 GitHub Pages public URL smoke 整備

- 状態: `未着手`
- フェーズ: `GP3`
- 主マイルストーン: [M9 GitHub Pages Public Demo / Deploy](../../milestones.md#m9)
- 関連マイルストーン: [M7 Web Support 完成](../../milestones.md#m7)
- 親チケット: [TKT-012 Web GitHub Pages deploy / public demo](../TKT-012-web-github-pages-deploy.md)
- 依存チケット: [`TKT-014`](./TKT-014-github-pages-preset-public-entry.md) [`TKT-015`](./TKT-015-github-pages-deploy-workflow.md)
- 後続チケット: `TKT-017`
- 関連メモ: [docs/web-github-pages-plan.md](../../web-github-pages-plan.md) [M9 GitHub Pages 個別チケット](./README.md)

## 進捗

- [ ] deploy 後の `page_url` に対する smoke 観点を固定する
- [ ] addon load と最小 synthesize を確認する browser automation を整備する
- [ ] service worker cache / stale deploy を切り分ける診断方法を固定する
- [ ] failure 時の screenshot / console / log artifact を固定する

## タスク目的とゴール

- GitHub Pages に deploy された公開 URL を対象に、addon load と最小 synthesize の成否を自動で判定できるようにする。
- deploy 成功だけでは見えない runtime failure、cache 問題、public URL 固有の不具合を `GP3` で早期に検知できるようにする。

## 実装する内容の詳細

- `TKT-015` が出力する `page_url` を受け取り、headless browser でアクセスする smoke を設計する。
- addon load、最小 synthesize、必要 resource の解決、エラーメッセージの検出条件を固定する。
- cache-busting、service worker 更新反映、deploy 直後の待ち合わせなど、GitHub Pages 固有の揺れを吸収する。
- failure 時に screenshot、console log、network / app log などの診断情報を artifact として残す。
- local server 向け smoke と public URL 向け smoke の共通化余地を整理する。

## 実装するために必要なエージェントチームの役割と人数

- `QA / browser automation engineer` x1: headless browser smoke を設計する。
- `web runtime engineer` x1: addon load と最小 synthesize の観測点を定義する。
- `CI engineer` x1: deploy 後 job との接続と artifact 回収を整える。
- `debugging / observability engineer` x1: failure artifact と cache 問題の切り分け手順を整える。

## 提供範囲

- public URL を対象にした smoke script / job。
- addon load と最小 synthesize の pass / fail 判定条件。
- cache / service worker 問題を含む診断手順。
- failure 時の screenshot / console / log artifact。

## テスト項目

- deploy 後の `page_url` に headless browser で到達できる。
- public URL 上で addon load と最小 synthesize が成立する。
- stale cache や deploy 反映待ちが原因の false negative を抑制できる。
- failure 時に必要な診断 artifact が保存される。

## 実装する unit テスト

- `page_url` の正規化、cache-busting、待ち合わせ判定を行う helper の軽量チェックを追加する。
- smoke 判定で使う log / DOM / state の抽出 helper を単体で検証できるようにする。

## 実装する e2e テスト

- Pages deploy 完了後に public URL smoke を実行し、addon load と最小 synthesize の成否を確認する。
- failure を意図的に起こせる場合は、diagnostic artifact が期待どおり残ることを確認する。

## 実装に関する懸念事項

- service worker と GitHub Pages の反映タイミングが絡むため、テストが flaky になりやすい。
- smoke の待ち条件を緩くしすぎると false positive が増え、厳しすぎると false negative が増える。
- local server smoke と public URL smoke のコードを分けすぎると保守が重くなる。

## レビューする項目

- `page_url` と deploy metadata の受け取り方が `GP2` と一致しているか。
- smoke の pass 条件が `GP1` の minimal demo / artifact 契約と一致しているか。
- cache / service worker 問題に対して診断情報が十分残るか。
- false positive / false negative を増やす待ち条件になっていないか。

## 一から作り直すとしたらどうするか

- local / remote を同じ harness で叩ける smoke runner を最初から作り、URL と mode だけ差し替える。
- app 側に smoke 用 telemetry endpoint を用意し、DOM や console parsing への依存を減らす。
- deploy metadata、smoke 結果、diagnostic artifact を 1 つの結果 manifest にまとめる。

## 後続のタスクに連絡する内容

- `TKT-017` には、運用メモと README がこのチケットの smoke 導線、failure artifact、cache 切り分け手順と一致するよう引き継ぐ。
