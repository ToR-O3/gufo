# Gufo v0.33.0のFlash-Next向けWSL修正

> 履歴資料：本文の採用状況・起動状態・未完了事項は作成当時の記録です。現在の仕様は共通カスタム台帳を参照してください。測定値は再集計せず保持し、個人ホームパスを匿名化しています。生ログ・個人設定は公開対象に含めていません。

本実装は `4910bc543efb30398042b13f63cc81ae2fd1288e`。修正差分（ローカル保管：production.patch）と再現用のGit bundle（ローカル保管：source.bundle）を保存した。

Flash-NextのROCm実行器で、既存のホスト識別APIからWSLを検出する。WSLのFlash-NextだけHIPグラフの作成と再生を省き、従来の直接発行経路を使う。⑤のDFlash2信頼度カットと⑱の巻き戻し統合を含む `e55f3b29` が土台。試験版の環境変数は本実装から外した。

ビルドはNixのリリース構成と拡張ロックを使い、Dockerは既存のROCm 10.1環境を使う。ビルドログ（ローカル保管：build.log） / イメージ情報（ローカル保管：image-version.txt） / バイナリのSHA-256（ローカル保管：engine.sha256）

## 検証

- [HIPグラフ有効・無効の同一バイナリ比較](../../../../benchmarks/benchmark/reports/wsl-hip-graphs-20261112/report.ja.html)
- [v0.33.0統合時の定例測定](../../../../benchmarks/benchmark/reports/v0330-toro3-20261112/report.ja.html)
- 本実装の定例測定ログ（ローカル保管：../../../benchmark/runs/v0330-wsl-eager-20261112/run.log）

## 定例ベンチの結果

内蔵PLE＋MTP、外部PLE Q8_0＋MTP、内蔵PLE＋DFlash2 Q8_0を各3回測定した。通常文を2件同時に生成する定例ベンチの実配信速度は、内蔵PLE＋MTPでHIPグラフ有効時の7.49 tok/sから44.98 tok/sに回復。v0.31.0の46.30 tok/sと同程度だった。外部PLE Q8_0＋MTPは7.51から46.03 tok/s、DFlash2は33.66から33.46 tok/s。中央値は定例の外れ値判定を適用した値。

単独は同じHTTP入力の216件すべてでHIPグラフ有効時と出力が一致。4件同時の内蔵PLE ARと外部PLE Q8_0 MTPは、初回にそれぞれ単独ARとの一致が11/12、独立した追試で両方12/12だった。外部PLEの不一致出力は、有効時の6件同時で出た応答と一致した。初回の数値と生ログも保存した。

[比較表](../../../../benchmarks/benchmark/reports/v0330-wsl-eager-20261112/report.ja.html) / [集計JSON](../../../../evidence/benchmark/runs/v0330-wsl-eager-20261112/summary.json) / [HIPグラフ有効時との出力照合](../../../../evidence/benchmark/runs/v0330-wsl-eager-20261112/graph-on-comparison.json) / 追試ログ（ローカル保管：../../../benchmark/runs/v0330-wsl-eager-20261112/recheck.log）

## 通常起動

`Gufo` は `gufo:v0.33.0-wsl-4910bc54-rocm101` で稼働。ToR-O3 UD-IQ4_XS、内蔵PLE、MTP、2×128K。通常のAPIモデル名は `Qwen3.8-FN-UD-IQ4_XS`。起動後の応答ではドラフト112トークンのうち75を採用し、2件同時の通常文は3回の実配信中央値が47.66 tok/s、単独ARとの出力は6/6件一致した。

廃止予定の `--prefill-chunk 512` を外した。生成中に8,139トークンの入力を処理する試験では、最大4,096トークンのパスを8グループに分け、16回生成へ処理を戻した。従来のコンテナと設定は切り戻し用に保存した。

起動・応答記録（ローカル保管：normal-restored.json） / 同時利用の検証（ローカル保管：normal-concurrency.json） / 採用確認（ローカル保管：adoption-readiness.json） / 以前の起動設定（ローカル保管：normal-settings.before.json）
