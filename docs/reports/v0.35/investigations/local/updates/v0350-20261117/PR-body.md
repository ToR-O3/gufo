# PR-body

> 履歴資料：本文の採用状況・起動状態・未完了事項は作成当時の記録です。現在の仕様は共通カスタム台帳を参照してください。測定値は再集計せず保持し、個人ホームパスを匿名化しています。生ログ・個人設定は公開対象に含めていません。

Integrate upstream Gufo v0.35.0 into the v0.34-based fork while retaining the existing quantized weight formats, external PLE, DFlash2, and ASR checkpoint support.

The new Q4_K draft-head path now applies only to the separately requantized draft prefix. Ordinary Q4_K projections keep native dense dispatch, including batches larger than eight rows. DFlash no longer reports MTP-calibrated continuation cost estimates. The extension API guide now names the active 0.3 series.

Validation completed so far:

- Canonical `nix flake check`: all checks passed, including 199 CPU tests.
- Focused GPU operators and synthetic wide verification passed.
- New dense dispatch regression passed; both incorrect dispatch mutations failed for the intended numerical reason, followed by a clean restored pass.
- Flash-Next text session checks passed across AR/MTP cohorts, mixed draft heads, rollback, and sampled replay.
- In-pass continuation and slot reuse passed for AR and MTP, including constrained device headroom and cancellation.
- The production release package built successfully.

HTTP validation is in progress. The PR remains a draft until those checks have been reviewed. Image and NPU inference are outside this validation scope.
