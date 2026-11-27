# v0.36 / v0.37 対象履歴の時間比較

> この文書は各作業段階の記録です。最終判断（2026-11-27）はv0.37の採用保留・外部化見送り、通常利用はv0.36 main、キャッシュ上限4 GiBです。本文の当時の状態・計画・測定値は維持しています。ローカル生ログは同梱せず、公開用コピーでは利用者ホームパスを匿名化しています。

基準53a217cb、候補b0e2750b。固定raw入力、生成128 token、context32768、2セッション設定の単独要求。short 1548 / long 12370 / resume 1963 token（継続の再利用1548 token）。正式3回ベンチや2件同時性能の測定ではない。

初回base→head。遅延フラグが出たAR・外部PLE・DFlash2はhead→baseで再確認。5%かつ10msを超える差で判定。全生データを保持し、外れ値除外や異なる要求間の平均化はしていない。1組のみの改善は確定値にしない。

|構成|要求|区間|v0.36 ms（各回）|v0.37 ms（各回）|判定|
|---|---|---|---|---|---|
|ar|short|cache_restore_ms|0.00, 0.00|0.00, 0.00|no_slowdown_observed|
|ar|short|prompt_ms|1465.83, 1446.55|2252.72, 2442.72|slower|
|ar|short|predicted_ms|4444.50, 4401.68|4643.19, 4415.91|no_slowdown_observed|
|ar|short|cache_snapshot_ms|40.70, 36.16|39.75, 32.92|no_slowdown_observed|
|ar|long|cache_restore_ms|0.00, 0.00|0.00, 0.00|no_slowdown_observed|
|ar|long|prompt_ms|8928.51, 9001.16|10671.34, 8978.83|inconclusive|
|ar|long|predicted_ms|4511.70, 4406.26|4395.89, 4415.16|no_slowdown_observed|
|ar|long|cache_snapshot_ms|165.23, 168.87|160.26, 184.31|inconclusive|
|ar|resume|cache_restore_ms|58.25, 54.82|53.88, 61.02|no_slowdown_observed|
|ar|resume|prompt_ms|473.83, 465.21|460.88, 474.93|no_slowdown_observed|
|ar|resume|predicted_ms|4369.75, 4340.11|4356.26, 4369.22|no_slowdown_observed|
|ar|resume|cache_snapshot_ms|49.40, 47.97|45.88, 45.63|no_slowdown_observed|
|builtin|short|cache_restore_ms|0.00|0.00|no_slowdown_observed|
|builtin|short|prompt_ms|1511.30|1502.10|no_slowdown_observed|
|builtin|short|predicted_ms|2962.88|2755.74|faster|
|builtin|short|cache_snapshot_ms|42.16|38.27|no_slowdown_observed|
|builtin|long|cache_restore_ms|0.00|0.00|no_slowdown_observed|
|builtin|long|prompt_ms|9892.64|9879.26|no_slowdown_observed|
|builtin|long|predicted_ms|2831.76|2887.94|no_slowdown_observed|
|builtin|long|cache_snapshot_ms|181.03|185.36|no_slowdown_observed|
|builtin|resume|cache_restore_ms|59.85|60.68|no_slowdown_observed|
|builtin|resume|prompt_ms|489.81|487.63|no_slowdown_observed|
|builtin|resume|predicted_ms|2217.03|2269.25|no_slowdown_observed|
|builtin|resume|cache_snapshot_ms|46.06|50.06|no_slowdown_observed|
|external-q8|short|cache_restore_ms|0.00, 0.00|0.00, 0.00|no_slowdown_observed|
|external-q8|short|prompt_ms|1488.47, 1537.76|1503.49, 1521.13|no_slowdown_observed|
|external-q8|short|predicted_ms|2709.03, 2903.46|2878.37, 2888.66|inconclusive|
|external-q8|short|cache_snapshot_ms|42.35, 43.80|41.90, 42.80|no_slowdown_observed|
|external-q8|long|cache_restore_ms|0.00, 0.00|0.00, 0.00|no_slowdown_observed|
|external-q8|long|prompt_ms|9938.83, 9890.71|9839.60, 10136.80|no_slowdown_observed|
|external-q8|long|predicted_ms|3036.51, 2966.68|2965.49, 3006.41|no_slowdown_observed|
|external-q8|long|cache_snapshot_ms|215.14, 196.50|182.28, 180.71|faster|
|external-q8|resume|cache_restore_ms|62.83, 66.41|65.23, 66.30|no_slowdown_observed|
|external-q8|resume|prompt_ms|480.51, 478.01|476.14, 478.52|no_slowdown_observed|
|external-q8|resume|predicted_ms|2584.99, 2642.87|2552.47, 2601.97|no_slowdown_observed|
|external-q8|resume|cache_snapshot_ms|48.21, 51.97|51.26, 50.87|no_slowdown_observed|
|dflash2|short|cache_restore_ms|0.00, 0.00|0.00, 0.00|no_slowdown_observed|
|dflash2|short|prompt_ms|1544.59, 1570.41|1564.23, 1569.02|no_slowdown_observed|
|dflash2|short|predicted_ms|2899.94, 2645.59|2605.30, 2664.80|no_slowdown_observed|
|dflash2|short|cache_snapshot_ms|145.96, 117.24|93.51, 118.83|no_slowdown_observed|
|dflash2|long|cache_restore_ms|0.00, 0.00|0.00, 0.00|no_slowdown_observed|
|dflash2|long|prompt_ms|10932.35, 10211.30|9937.27, 10070.90|no_slowdown_observed|
|dflash2|long|predicted_ms|2409.68, 2325.62|2358.79, 2305.65|no_slowdown_observed|
|dflash2|long|cache_snapshot_ms|343.45, 412.92|363.36, 392.72|inconclusive|
|dflash2|resume|cache_restore_ms|79.38, 83.29|81.85, 92.12|no_slowdown_observed|
|dflash2|resume|prompt_ms|736.17, 601.27|598.99, 602.53|no_slowdown_observed|
|dflash2|resume|predicted_ms|2116.51, 2130.83|2129.43, 2120.13|no_slowdown_observed|
|dflash2|resume|cache_snapshot_ms|166.69, 164.09|179.06, 164.48|inconclusive|

inconclusiveは遅延方向が再現しなかった未確定の観測。no_slowdown_observedはこの範囲で閾値を超える遅延が観測されなかったもの。全体の速度保証ではない。

## AR追加切り分け

キャッシュ有効の新規入力、生成128 token。第1入力だけは上表shortと同一。第2・第3入力は異なる固定seedで新規入力を作り、新旧で各入力と処理量を一致させた。全件cached tokenは0、回答hashは新旧一致。

|入力|v0.36 Prefill ms|v0.37 Prefill ms|
|---|---:|---:|
|1|1423.99|1426.50|
|2|1382.00|1367.39|
|3|1333.97|1292.58|

最初の2組のAR初回遅延はこの追加確認で再現しなかった。上表のslowerは最初の2組だけの判定であり、最終判断は原因未確定・ばらつきあり。cache無効・生成32 tokenの別条件はar-warm/results.jsonに保存し、同一条件の集計には入れない。
