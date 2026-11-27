# quality-review-draft

> 履歴資料：本文の採用状況・起動状態・未完了事項は作成当時の記録です。現在の仕様は共通カスタム台帳を参照してください。測定値は再集計せず保持し、個人ホームパスを匿名化しています。生ログ・個人設定は公開対象に含めていません。

## Native quantized paths (November 8, 2026)

The Q5_0 routed F16 path is checked against independently decoded packed
weights, MMQ and an FP64 dot product. Generic 16/48/128-token tiles retain
split-pass rounding. The specialized 128-token candidate failed an exact F16
comparison and was rejected without relaxing the contract.

Input reuse and the grouped Q6_K projection preserve the candidate's numerical
results: all 159 fixed-history logit rows are byte-identical before and after
each change. Q6_K additionally matches the previous FP32 launcher exactly for
distinct rows at widths 9, 129, 257, 2034, 2049 and 2057. Independent CPU checks
and the existing allocation-boundary checks pass.

A separate CPU FP32 oracle evaluates the actual mixed IQ4_XS/IQ3_XXS/Q5_0
weights over English, Japanese, prose, code and JSON. Using the unchanged
reference scorer, the 159-position comparison is:

| Metric | Before routed Q5_0 F16 | Candidate |
| --- | ---: | ---: |
| Mean KL lower bound (top 256 plus tail) | 0.0136270 | 0.0113673 |
| Maximum KL lower bound | 0.2149023 | 0.1215895 |
| Top-1 agreement | 147/159 | 143/159 |
| Mean next-token NLL (158 positions) | 2.3192346 | 2.3292876 |

The paired mean KL change is -0.0022597 (95% bootstrap interval
[-0.0049904, 0.0003327]); NLL changes by +0.0100530
([-0.0188430, 0.0389105]). Both intervals include zero, while top-1 agreement
falls. This small sample does not establish an overall quality improvement or
qualify other weight sets and architectures. The new prefill arithmetic can
change generated prose and MTP acceptance; those trajectories are recorded
separately from exact-output timing controls.

