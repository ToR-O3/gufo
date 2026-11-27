# 今回のベンチマーク作業

> 履歴資料：本文の採用状況・起動状態・未完了事項は作成当時の記録です。現在の仕様は共通カスタム台帳を参照してください。測定値は再集計せず保持し、個人ホームパスを匿名化しています。生ログ・個人設定は公開対象に含めていません。

ユーザー指定：2026-11-02。Gufo→Strata Docker→Strata Windowsの順番。最後に元のGufoとGufo-ASRを起動。各条件3回、メモリ監視。通常設定は変更しない。

1. Gufo v0.28.1 / ROCm10.1の現行イメージでSC117内蔵→Huihui外部BF16→Huihui内蔵→Orca内蔵。公式bench.jsonそのまま、回数3。3回読み込み、単独72計測、同時189応答、メモリ2負荷×3。harness/controller.pyが実行中。成功時は通常Gufoを停止状態のままにする。
2. Gufoレポートは指定されたv0.25サンプルと同じ暗色・横比較形式。SC117過去/今回、Huihui過去/今回外部/今回内蔵、Orca過去/今回、公式の順。Prefill tok/s・TTFT秒・Decode tok/s。公式値の未公開TTFTは作らない。
3. Strata Docker0.2.9/Engine0.1.55をHuihuiで初回公式キャリブレーション→保存と再利用確認→公式standard-v1全項目（optional concurrency含む）→Gufoベンチを近い設定で実施。終了後停止。
4. Windows Terminal0.2.9/0.1.55でも別にキャリブレーション、同じ順。Windows起動は今回ユーザー明示許可。GUIでなくC:/Developments/Strata-Terminal。終了後停止。
5. Strataレポートは別のworkspace/Strata/docs/benchmarkへ。公式表Docker/Windows/公式。Gufo形式表Gufo参考/Docker/Windows/公式。派生モデル・設定差・測定不能は区別。
6. 最後に元のGufo（Huihui+BF16、2x128K、ROCm10.1）とGufo-ASRを起動、ready/設定一致確認。Strataおよび試験用コンテナは停止。不要な一時物のみ削除。

通常コンテナinspectと設定の原本はこのフォルダ内。Gufo-ASRは開始時点で既に停止。Gufoは稼働していた。Strataはcreated。ソースは編集禁止という指定ではないが今回はベンチなので本体に変更を加えない。Gufoのharnessは現在のworkspace/toolsを読む（稼働8f4a7e77との差分なし）。古いOrca/SC117設定のイメージは欠損だがモデル引数だけ再利用し現行イメージで測定。

Strata公式資料：docs/benchmark/runs/docker-windows-20261102/official。公式standard-v1は元Qwen向けdraft protocolなのでHuihuiは派生モデルとして明記。各公式suiteにはperformance3反復が内在。全suite3回はfreshengineで行い、重複キャッシュを混ぜない。公式値は対象ハード/モデル/版を表示。

## 進行記録（2026-11-02 03:43 JST更新）

- SC117は全23項目の測定完了。単独72測定、同時189応答、読み込み3回、メモリ2負荷×3回。測定記録の整合性チェックは全項目true。単独AR/MTP文章は同一入力24組中6組一致・18組不一致。同時利用は15条件全て一致。
- Huihui外部BF16も全23項目を実施し、同じ記録整合性チェックは全項目true。単独AR/MTPは同一入力24組中4組一致・20組不一致。同時AR C2/C6、同時MTP文章C4で各1応答不一致（合計3）。不一致を除外した平均や差し替えはしていない。
- 注意：途中の進捗報告で、結果ファイルを確認せず同時利用合格と誤報した。job-outcomes.jsonで上記3条件の不一致を確認した時点で訂正・謝罪済み。以後はprogress.jsonだけで合格判定せず、job-outcomes.jsonとcorpus-reportsを必ず確認する。
- 現在Huihui内蔵n-gramの単独AR。後続のOrcaまでharness/controller.pyが順次自動実行。PIDはcontroller-pid.json。Gufo-benchmark-20261102のみ稼働。通常Gufo・ASR停止、Strata未起動。
- analyze.py→report.pyを実データで実行済み。SC117とHuihui外部BF16を反映した途中レポートは docs/benchmark/reports/all-models-v0281-20261102-021203/report.ja.html。未完了条件は空欄。8リンクにローカル参照切れなし。partial-report-verification.json更新済み。
- Gufo全条件終了後にharness/stopped-memory.pyを実行しgufo-stopped-memory.jsonを作成。その後再集計・レポート検証。通常Gufoはそのまま停止を維持してStrataへ進む。
- Strataの準備はread-only調査とローカル試験スクリプト作成のみ。workspace/Strata/docs/benchmark/runs/docker-windows-20261102/harness に環境別セットアップ・公式キャリブレーション・公式標準3回・旧来公式速度試験（--runs 3 --clients 4 --long）・Gufo方式・メモリ監視・集計・レポートのスクリプトを作成。構文確認のみで実機実行はまだ。harness/host_stage.py docker をGufo全試験終了後に開始し、Docker終了・内容確認後に windows を開始する。
- Strata公式標準の参考値を発見・保存済み。公式repo bench/results/2026-10-20-0.2.1-bench/p2003-rtx3060/run3-with-fixes。RTX3060、IQ3_XXS、Engine0.1.46であり今回のHuihui/AMDとは違う。Gufo方式の右端はGufo公式値。
- v0.25の公式公開値10ファイルは現在の公式ソースのJSONとSHA-256一致。v0.25保存設定はrocm-runtime-7.2.3、今回は10.1+AMD_SERIALIZE_COPY=2。速度差はエンジンだけの差とは扱わない。
- 全レポート完了後、元のGufo（Huihui+BF16、2x128K、ROCm10.1）とGufo-ASRを復帰。Strataは停止状態にする。最終復帰はまだ実施しない。

## 追加進捗：Huihui内蔵n-gram
- MTP文章128Kまでの3回測定が完了。Prefill平均928.04446 tok/s、TTFT平均2.221382秒、Decode平均34.27670 tok/s。
- 単独AR／MTP文章は同一入力24/24組、全文一致3/24組。interim-input-output-audit.jsonに保存。出力を差し替えず参考値として扱う。
- 単独の反復文も128Kまで3回測定完了。同時利用を実行中。
- AR・4要求で12応答中11一致、3組中2組一致。不一致を保持して次条件へ進めている。
- Strataは未起動。Docker→Windowsの順に実行するためのハーネスとレポート雛形のみ準備。実行時の検証はこれから。

## 04:20 JST：Gufoの3条件が完了、Orca開始

SC117・Huihui外部BF16・Huihui内蔵の全測定を集計し、3条件とも回数・HTTP原記録・出力token数・入力パラメータ・メモリ記録の検査が全て通過。回答一致の未達は別に保持。Huihui内蔵は単独3/24組一致、同時利用186/189応答一致（AR C4、MTP文章C2/C6が各1応答不一致）。反復文の同時利用は全て一致。

Orcaの読み込み測定から実行中。Strata未起動。途中版HTMLを3条件の値へ更新し、8リンクの存在と不一致の回数別集計を検証済み。

## 05:03 JST：Gufo完了、停止確認済み

4条件すべて各72単独測定・189同時応答・読み込み3回・メモリ2負荷×3回を取得。全条件で原記録との照合と測定回数検査が通過。Orcaは単独AR/MTPが7/24一致、同時利用は189/189一致。回答不一致は保持したままレポートへ掲載。停止後はWindows利用可能37.743 GiB、HIP空き110.918 GiB。Gufo/ASR/測定用コンテナ/Strataの停止を確認。Gufoレポートを4条件で再生成し、全リンクと不一致数を検証済み。続いてStrata Dockerの段階へ進む。

- All requested measurements are complete. Gufo, Gufo-ASR, Strata Docker, and test containers are stopped; Strata Windows processes are absent. Final startup was cancelled at the user's request before a PC reboot. Normal Gufo and ASR settings are unchanged. Final evidence: final-completion.json.
