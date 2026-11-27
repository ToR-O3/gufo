# finish-checklist.ja

> 履歴資料：本文の採用状況・起動状態・未完了事項は作成当時の記録です。現在の仕様は共通カスタム台帳を参照してください。測定値は再集計せず保持し、個人ホームパスを匿名化しています。生ログ・個人設定は公開対象に含めていません。

> 2026-11-08：下記最終処理は完了。通常Gufoは内蔵PLEで稼働中。実施結果と未解決の回答一致項目はREADME.ja.md、completion.json、正式レポートを正本とする。以下は実施時の手順記録であり、再実行指示ではない。

# 最終処理の残作業

- 本体の実装・コミットは完了。HEAD `96f1aa8c`、全15コミットは目的別の英語。通常コンテナGufoは停止中。ユーザーは起動・停止を委任済み。
- 通常反映イメージ `gufo:v0.31.0-quant-refactor-96f1aa8c-rocm101` 作成済み。バイナリSHAとイメージからのversion確認済み。`final-image.json`を参照。
- ベンチ正本: `docs/benchmark/runs/quant-refactor-20261108`。外部Q8_0 PLEは全23項目・各3回の測定が完走。内蔵PLEは実行中。現在の進捗は`python3 local/v0310/show_benchmark_progress.py builtin`。
- 外部PLEのAR/MTP文章一致は8/24。旧正式ベンチと改修前d1比較にも同種の差が存在し、再利用境界が1token異なる。全条件合格とはしない。
- 外部PLE MTP文章C4は正式12応答中1件不一致。旧d1、少量expert変更直前、最終候補を各3回再測定すると全12/12一致。入力payload全一致、変更直前と最終候補はAR回答も一致。原因未確定で初回セルを未合格のまま保持。`concurrency-quality-review.json`を参照。初回completionの非ゼロは`completion.initial.json`等へ保全。
- 内蔵PLEを動かす現在のラッパーは`continue_formal_after_quality.py`。全23項目が実行され、回答一致以外の実行失敗がない場合だけ測定完了にする。回答不一致を合格へ変更しない。追加不一致は即時報告し、必要条件だけ確認する。
- 全測定後、通常復帰: `python3 local/v0310/restore_normal.py gufo:v0.31.0-quant-refactor-96f1aa8c-rocm101 local/v0300/gpu-build/gufo`。旧Gufoを停止したrollback名へ保全、同じDocker引数でimageだけ変更。通常内蔵PLE・thinking auto・MTP7・context131072・2session等を保持する。ready、version、短い応答、実draftとreasoningを検証して設定を更新する。
- 復帰後 `python3 local/v0310/snapshot_memory.py normal-memory-after`。前スナップショットは取得済み。監視は最後の測定・復帰後データが取れるまで継続。
- 集計: `python3 docs/benchmark/runs/quant-refactor-20261108/harness/analyze.py`。measurement_completeを確認し、回答一致の未合格セルを別記。続いて`harness/report_benchmark.py`。出力は`docs/benchmark/reports/quant-refactor-20261108/report.ja.html`。リンク・数値・回数を照合。
- 監視終了はベンチ根の`stop-telemetry`を作成。所有するテスト/ビルダーだけ停止し、通常Gufoを起動状態に保つ。ASR/Strataは操作しない。元イメージ・rollback・失敗原記録を保持する。
- 進捗表を最終状態へ更新し、Hermetica inboxへ結果を1行追記。公開コードやコミットへ私的な呼称・環境情報を含めない。最終回答は通常起動確認とレポートへのリンク、未解決の回答一致・他モデル未検証範囲を明示する。
