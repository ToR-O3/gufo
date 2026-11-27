# v0.37 レビュー資料

v0.37の取り込み、定例ベンチ、性能調査、外部化検討の記録です。

**最終判断（2026-11-27）：v0.37はレビュー用ブランチで保留し、通常利用はv0.36 mainを継続。外部化も当面見送ります。**

実行コードはb0e2750b、比較対象はv0.36の53a217cbです。通常設定は内蔵PLE＋DFlash2、128k、同時接続2、RAMキャッシュ最大4 GiBです。

各資料は作成時点の記録です。「未実施」「v0.37起動中」「8 GiB採用」等は当時の状態で、最新の判断ではありません。8 GiB採用は後に撤回しています。

## レポート

- [定例ベンチ比較表（HTML）](benchmark/reports/v0370-review-20261127/report.ja.html)
- [v0.36からの変化（HTML）](benchmark/reports/v0370-review-20261127/previous-v0360.ja.html)

HTMLはダウンロードしてブラウザーで開くと表として表示できます。数値と表の内容は元のレポートを維持しています。

## 作業文書

- [v0.37 カスタム外部化の調査（2026-11-27）](local/updates/v0370-20261126/externalization-review.ja.md)
- [v0.36 / v0.37 対象履歴の時間比較](local/updates/v0370-20261126/timing-comparison.ja.md)
- [Gufo v0.37取り込み・カスタム棚卸し](local/updates/v0370-20261126/README.ja.md)
- [v0.37保留とv0.36通常復帰](local/updates/v0370-defer-v0360-restore-20261127/README.ja.md)
- [v0.37速度低下の一次切り分け（保存済みデータ）](benchmark/runs/v0370-review-20261127/slowdown-triage.ja.md)
- [v0.37 定例ベンチ・通常設定の確認](benchmark/runs/v0370-review-20261127/README.ja.md)
- [v0.37 定例ベンチ実施計画](benchmark/runs/v0370-review-20261127/plan.ja.md)
- [通常起動設定の評価](benchmark/runs/v0370-review-20261127/settings-plan.ja.md)
- [Stages移行後の高速経路と速度低下の調査](benchmark/runs/v0370-stages-investigation-20261127/report.ja.md)
- [Stages移行後の速度調査](benchmark/runs/v0370-stages-investigation-20261127/plan.ja.md)
- [複数トークン生成前の状態修正（#1369）の調査](benchmark/runs/v0370-wait-investigation-20261127/deferred-token-investigation.ja.md)
- [v0.37の断続的な待機増加の調査（2026-11-27）](benchmark/runs/v0370-wait-investigation-20261127/report.ja.md)
- [Q4_K draft headの丸め統一とMTP速度への影響](local/updates/v0370-20261126/q4-investigation/report.ja.md)

## 保存範囲

今回作成したMarkdownとレポート2本、参照される集計JSON3本を収録しました。ダウンロードしたEXL3の公式README・解説、ビルド生成物内の複製は今回の作成文書ではないため含めていません。

測定値・失敗・未確定事項は保持し、ホームディレクトリのユーザー名を匿名化しました。全生ログ、認証情報、通常起動設定JSONは含めていません。同梱していない証拠のファイル名は、ローカル記録への参照です。

元ファイルと公開コピーの対応・SHA-256は[manifest.json](manifest.json)を参照してください。
