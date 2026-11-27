# GufoのAR／MTP不一致調査：保全・引き継ぎ

> 履歴資料：本文の採用状況・起動状態・未完了事項は作成当時の記録です。現在の仕様は共通カスタム台帳を参照してください。測定値は再集計せず保持し、個人ホームパスを匿名化しています。生ログ・個人設定は公開対象に含めていません。

2026-11-02（日本時間）。ユーザー指示により、利用枠のリセット後まで原因調査を保留する。単独AR／MTPの回答一致率が以前の記録から大きく低下しているため、同時利用の不一致と併せて追跡する。今回の保全作業では追加試験・実装・設定変更を行わない。

## 調査対象と保全範囲

- 原記録：`/home/user/workspace/Gufo/docs/benchmark/runs/all-models-v0281-20261102-021203/`
- 今回のレポート：`/home/user/workspace/Gufo/docs/benchmark/reports/all-models-v0281-20261102-021203/report.ja.html`
- v0.25比較元：`/home/user/workspace/Gufo/docs/benchmark/reports/gufo-v0250-all-models-20261027/report.ja.html`
- ソースとGit履歴：`/home/user/workspace/Gufo/`
- 通常起動設定：`/home/user/Apps/Gufo/normal-settings.json`
- 共通モデル：`/home/user/Models/`
- 保全ファイル一覧・ハッシュ・設定控え・Git状態：この原記録配下の `preservation/`

SC117 IQ3_S内蔵n-gram、Huihui UD-IQ4_XS外部BF16、Huihui UD-IQ4_XS内蔵n-gram、Orca IQ4_XS内蔵n-gramの4条件をすべて残す。一致した応答だけを選んで残すことや、不一致の原記録を削除することはしない。

各条件の `http/`、`observations.jsonl`、`artifacts/`、`corpus-reports/`、`single-ar-mtp-input-audit.json`、`measurement-provenance.json`、`logs/`、`effective-config.json`、`loaded-runtime.json`、`fingerprint.json`、`manifest.json`を保持する。集計だけでなく、実入力・応答・キャッシュ／prefill量・draft・起動条件を確認できる原記録が重要。

ルートの `harness/`、`model-config.json`、`official-bench.json`、`official-artifacts/`、`baseline-v025.json`、`runtime-comparison.json`、`summary.json`、`test-outcomes.json`、コンテナ作成設定・イメージ識別情報も保持する。旧版のレポートから抽出した値と今回の実測を混同しない。

## 現時点の限界

- ARとMTPの出力一致は、正答率やモデル能力を直接測る指標ではない。ただし、同じ入力・設定で大きく一致率が変化した理由は確認する必要がある。
- 数値誤差、カーネル、MTPの検証・採択、キャッシュ、量子化拡張、試験ハーネスのいずれが原因かは未確定。保全時点では原因を追加検証していない。
- 単独AR／MTPの比較と、同時利用での単独AR参照との比較を分ける。これらは参照条件が異なるため、合格数だけで同じ現象と決めつけない。
- 旧版との間にはGufo・カスタム・ROCm等の条件差がある。v0.25の値との単純比較だけで一つの変更を原因と断定しない。
- 全試験の終了時点の停止状態と、その後の現在の稼働状態は別。保全時の実状態を `preservation/manifest.json` に記録する。稼働中のGufo／ASRは操作しない。
- 以前の配置先 `/home/user/Apps/Gufo/deployments/v0281-attention-boundary-20261101-163558/source` は保全時に存在しなかった。移動・削除の経緯は未調査。現在のworkspaceのGit状態と実行イメージの記録から追う。
- 試験用 `Gufo-benchmark-20261102` コンテナはStrata側の保全時点で不在だった。今回の保存済み作成設定・実行ログを参照し、コンテナが今も存在すると仮定しない。

## リセット後の調査順序

まず保存済みの一致率集計と原応答を照合し、入力・生成設定・実際のdraft・キャッシュ量が一致しているか確認する。次に単独AR／MTPで最初に異なるトークンと条件の傾向を調べ、実行版・量子化経路・キャッシュ等の切り分け方針を立てる。追加実行は必要な条件だけに絞り、調査再開だけをフルベンチ再実行やモデル起動の許可と解釈しない。

原記録、Git履歴・変更、Dockerイメージ、実行形式、モデルを調査再開まで削除・更新・pruneしない。大きなモデルとイメージは現物を保持し、全量コピーは作らない。この保全は同一ディスク内の現状保持であり、別媒体への完全バックアップではない。

再開時は「GufoのAR／MTP不一致を、Gufoの`PRESERVATION.ja.md`から調査再開」と指定する。Strataの所要時間調査の保全メモは `/home/user/workspace/Strata/docs/benchmark/runs/docker-windows-20261102/PRESERVATION.ja.md`。
