# Gufo v0.34.0の実機検証

> 履歴資料：本文の採用状況・起動状態・未完了事項は作成当時の記録です。現在の仕様は共通カスタム台帳を参照してください。測定値は再集計せず保持し、個人ホームパスを匿名化しています。生ログ・個人設定は公開対象に含めていません。

ToR-O3 UD-IQ4_XSを使用。内蔵PLE＋MTP、外部PLE Q8_0＋MTP、内蔵PLE＋DFlash2 Q8_0を、公式v0.34.0のベンチ条件で各3回測定した。単独利用、同時利用、冷間ロード、GPU・ホストメモリを記録した。

[比較レポート](../../../../benchmarks/benchmark/reports/v0340-validation-20261112/report.ja.html) · [集計と外れ値](../../../../evidence/benchmark/runs/v0340-validation-20261112/summary.json) · 測定ログ（ローカル保管：../../../benchmark/runs/v0340-validation-20261112/run.log）

## ビルドと出力照合

- v0.34.0統合ツリーの `a66b1a2c` と既存の作業差分から、ロックされた拡張構成で `319fcdde` をビルドした。リリースログ（ローカル保管：build.log） · ソースのバンドル（ローカル保管：source.bundle） · ビルド時の差分（ローカル保管：candidate.patch）
- IQ2_S・IQ3_XXS・IQ4_XSの `gate/up` とSwiGLU後のGPU出力は、9・16・33・64行で従来経路とバイト単位で一致した。演算試験ログ（ローカル保管：iq-gate-ops.log）
- v0.33.0との単独照合は216/216件。外部PLE Q8_0＋MTPの4件同時・通常文では初回11/12件が単独ARと一致した。[出力の照合](../../../../evidence/benchmark/runs/v0340-validation-20261112/comparison-v0330.json)

## 追加対照

- 外部PLE Q8_0＋MTPの4件同時を追加測定。IQ変更あり・なしで24/24件ずつ単独ARと一致し、初回の出力差は再現しなかった。全体スループット中央値は変更あり60.81、変更なし63.02 tok/s。対照の生値（ローカル保管：../../../benchmark/runs/v0340-validation-20261112/c4-control.json）
- 内蔵PLE＋MTPの冷間ロードは、同じ時間帯に交互に測定するとv0.33.0が36.98秒、v0.34.0が41.23秒。履歴なしの通常文では、Prefillが1,118.50から1,136.41 tok/s、TTFTが1.916秒から1.860秒になった。冷間ロード（ローカル保管：../../../benchmark/runs/v0340-validation-20261112/loading-control.json） · 単独Prefill（ローカル保管：../../../benchmark/runs/v0340-validation-20261112/prefill-control.json）

## 通常起動

2026-11-13 01:28 JSTに v0.34.0（`319fcdd`）への切り替えを確認。内蔵PLE＋MTP Q8_0、128K×2、thinking auto、RAMキャッシュ4 GiB。旧ドラフト引数を外し、モデル名 `Qwen3.8-FN-UD-IQ4_XS` でHTTP待ち受けと生成を確認した。

通常設定の2件同時・通常文は48.68 tok/s、6/6件が単独ARと一致。先行要求の生成中に8,139トークンの新規プリフィルが実行され、層の区切りで16回、先行要求の生成が進んだ。旧v0.33.0は `Gufo-v0330-before-v0340` に保存した。

起動検証（ローカル保管：normal-restored.json） · 2件同時の生データ（ローカル保管：normal-concurrency.json） · 採用判断（ローカル保管：adoption-readiness.json） · 通常起動設定（ローカル保管：/home/user/Apps/Gufo/normal-settings.json）

## 公開PR

[ToR-O3/gufo #5](https://github.com/ToR-O3/gufo/pull/5) を `main` 宛てに作成。公開コミット `dba1fb64` のソースは実機ビルド `319fcdde` と一致。英語の説明に3構成の計測、4件同時の出力差と追試、IQ変更の速度対照を掲載した。拡張API差分の検査とC++ 981ファイルの書式検査に合格。

PR #5は `48974e3d` でマージ済み。通常設定のソース参照を v0.34.0のmain作業場（ローカル保管：../../worktrees/v0340-main-48974e3d） に更新した。稼働中の `319fcdd` とマージ後mainのソースツリー `78cd5ade` は一致。既存の通常コンテナで `/ready`、モデル名、128K×2、実起動引数を照合し、コンテナの起動時刻も維持されていることを確認した。
