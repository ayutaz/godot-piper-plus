# TKT-017 GitHub Pages docs / operational notes 最終化

- 状態: `進行中`
- フェーズ: `GP4`
- 主マイルストーン: [M9 GitHub Pages Public Demo / Deploy](../../milestones.md#m9)
- 関連マイルストーン: [M7 Web Support 完成](../../milestones.md#m7)
- 親チケット: [TKT-012 Web GitHub Pages deploy / public demo](../TKT-012-web-github-pages-deploy.md)
- 依存チケット: [`TKT-013`](./TKT-013-github-pages-scope-asset-policy.md) [`TKT-014`](./TKT-014-github-pages-preset-public-entry.md) [`TKT-015`](./TKT-015-github-pages-deploy-workflow.md) [`TKT-016`](./TKT-016-github-pages-public-url-smoke.md)
- 後続チケット: なし
- 関連メモ: [docs/web-github-pages-plan.md](../../web-github-pages-plan.md) [M9 GitHub Pages 個別チケット](./README.md)

## 進捗

- [x] README と関連 docs に public demo の scope と known limitations を反映する
- [x] deploy workflow、public URL smoke、cache troubleshooting の運用メモを反映する
- [ ] `M9`、`TKT-012`、個別チケットの完了時反映手順を整理する
- [ ] `GP0` で固定した公開 URL の案内条件を README と運用メモへ反映する

## タスク目的とゴール

- GitHub Pages 対応で出来上がった scope、preset、deploy、smoke の結果を、ユーザー向け文書と運用メモへ矛盾なく反映する。
- `GP0` から `GP3` で固定された公開範囲、案内条件、cache 切り分け、更新手順を文書化し、後から見ても運用可能な状態にする。

## 実装する内容の詳細

- `README.md`、`docs/web-github-pages-plan.md`、`docs/milestones.md`、`docs/tickets/README.md`、必要なら addon README に Pages 公開の情報を反映する。
- public demo の scope、非対象範囲、known limitations、`CPU-only` / `no-threads` / English minimal demo の前提を明記する。
- deploy workflow の trigger 条件、workflow / job 名、environment、artifact 名、Pages 公開手順、失敗時の rerun / redeploy、cache troubleshooting を運用メモとして整理する。
- `GP0` で固定した公開 URL の開示条件を README へ反映し、過大表現を避ける。
- `GP2` と `GP3` が渡す `page_url`、workflow / job 名、failure artifact 名、screenshot / console / log の参照先を運用メモへ落とす。
- `M9` と `TKT-012` の完了時反映手順を整え、完了時にどこを閉じるかを固定する。

## 実装するために必要なエージェントチームの役割と人数

- `docs engineer` x1: ユーザー向け文書と内部メモを更新する。
- `release / operations engineer` x1: deploy / rollback / troubleshooting の運用手順を整理する。
- `QA representative` x1: smoke と failure artifact に基づく診断フローをレビューする。
- `technical editor` x1: preview support と public demo の表現境界を整える。

## 提供範囲

- README と関連 docs への Pages 公開情報の反映。
- deploy / smoke / cache troubleshooting を含む運用メモ。
- milestone / ticket の完了時反映手順。
- `GP0` で固定した public URL の案内条件と表現方針の反映。

## テスト項目

- 文書の scope、known limitations、公開手順が `GP0` から `GP3` の結果と一致している。
- README から Pages 公開情報へ辿れる。
- cache / rerun / redeploy の手順が failure artifact と整合している。
- `M7` の preview support と `M9` の public demo が混線していない。

## 実装する unit テスト

- docs 内リンク、ticket 参照、milestone 参照の整合を確認する軽量チェックを追加する。
- 可能であれば README と運用メモで使う固定値や URL の整合を確認する script-level check を追加する。

## 実装する e2e テスト

- `TKT-016` の public URL smoke が成功した状態で、README と運用メモの手順どおりに再確認できることを確認する。
- failure 手順を実施する必要がある場合は、rollback / cache troubleshooting の文書が現実の挙動と合うかを spot check する。

## 実装に関する懸念事項

- 実装より先に文書だけ更新すると public URL や制約がすぐ古くなる。
- preview support と public demo の差を曖昧に書くと期待値を誤らせる。
- deploy 条件や cache troubleshooting を短く書きすぎると、障害時に運用できなくなる。

## レビューする項目

- README と運用メモが `GP0` から `GP3` の結果を過不足なく反映しているか。
- public URL の表現が `GP0` で固定した開示条件を超えていないか。
- cache / rerun / redeploy の手順が failure artifact と結び付いているか。
- `M9` と子チケットの完了条件が曖昧になっていないか。

## 一から作り直すとしたらどうするか

- deploy metadata、smoke 結果、known limitations を 1 つの generated docs source にまとめ、README や運用メモをそこから生成する。
- milestone / ticket 状態と user-facing docs を別レイヤーに分け、公開 URL や制約だけを deployment manifest から埋め込む。
- troubleshooting は FAQ ではなく runbook として最初から構造化し、症状別の導線を持たせる。

## 後続のタスクに連絡する内容

- public demo を Phase 2 へ広げる場合は、`M9` を延命せずに別 milestone / ticket 群として切り出す。
- Japanese text input、thread build、binary size 最適化はこのチケットで抱えず、別要求として扱う。
