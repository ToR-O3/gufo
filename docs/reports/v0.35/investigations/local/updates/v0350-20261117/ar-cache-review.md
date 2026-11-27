# AR cache comparison review

> 履歴資料：本文の採用状況・起動状態・未完了事項は作成当時の記録です。現在の仕様は共通カスタム台帳を参照してください。測定値は再集計せず保持し、個人ホームパスを匿名化しています。生ログ・個人設定は公開対象に含めていません。

Read-only investigation of `functional/ar-baseline` and `functional/ar-candidate`, 2026-11-17. No builds, inference, runtime changes, or harness edits were performed by this reviewer. This note is the only written artifact.

## Verdict

Both runs passed their functional assertions. The comparison remains failed: eight work/coverage differences and 107 of 634 timing measurements exceed the joint 5%/10 ms thresholds. The eight work/coverage differences have concrete upstream policy provenance. Timing is still unqualified; one observation does not prove or disprove a regression.

## Changed disk reuse is explained by prompt demotion

| Restarted request | Baseline cached / prefill | Candidate cached / prefill | Candidate seed publication |
| --- | --- | --- | --- |
| text-cancel-disk request 0 | 172 / 14 | 176 / 10 | `server.log:398`: tokens=176 checkpoint_id=745 purpose=0 spacing_exempt=1 |
| thinking-tool-cancel-disk request 0 | 672 / 37 | 709 / 0 | `server.log:372`: tokens=709 checkpoint_id=811 purpose=0 spacing_exempt=1 |
| legacy-tool-cancel-disk request 0 | 668 / 43 | 711 / 0 | `server.log:408`: tokens=711 checkpoint_id=926 purpose=0 spacing_exempt=1 |

All seed-log references above are under `functional/ar-candidate`. These are prompt checkpoints, not direct generated-frontier publications. Earlier attempts to persist 709 and 711 were refused for `min_step`; subsequent RAM pressure demoted these retained checkpoints and bypassed spacing. The restarted acquisitions select exactly those boundaries (`server-restarted.log:15,35,54`). Baseline selects 172,672,668 (`server-restarted.log:12,28,43`).

Upstream commit `48aeeee992a754f06ccf6ffe93995e5de446c184` (#1183) adds coordinated RAM/disk retention and demotion that bypasses prompt spacing. `docs/internal/cache.md:620` describes this explicitly. Generated-frontier captures add other records and pressure, but the directly selected records here are the demoted prompt checkpoints. The upstream qualification note also records intentional deeper disk restores and explicitly leaves disk timing inconclusive (`docs/internal/cache-1130-1136.md:198-220`). Its measurements are historical upstream evidence, not results for this machine.

The cancellation histories are matched: each run has the same six disconnected requests, the client cancels after eight nonempty deltas, initial resumed cached/prefill counts match across revisions, and all three resumed and follow-up output hashes match. The comparator reports no request-history, output, prompt-token, or completion-token changes. The differences only appear after restart, consistent with changed publication/retention rather than different client cancellation histories.

Zero prefill removes `prefill_ms_per_token`; that accounts for the two metric-coverage differences. Do not invent a zero per-token measurement. The existing generic comparator intentionally requires equal cached/prefill work and therefore cannot turn this policy-changing pair green merely by repeating it. Preserve its failed result and attach this specific explanation; do not loosen the global check.

## Timing findings that remain open

| Request / phase | Baseline ms | Candidate ms | Work comparison |
| --- | ---: | ---: | --- |
| legacy-tool-cancel-disk request 1 snapshot | 24.268 | 153.885 | Same 274044960 captured bytes, two captures, 24 prefill tokens, one completion token, no restore |
| same request queue | 1.421 | 12.636 | Same submitted request; candidate background frontier work can delay admission |
| cache-persist-disk request 0 restore | 175.492 | 232.158 | Same 203299024 restored bytes, 244 pieces, 2828 cached tokens |
| same request snapshot | 107.849 | 157.820 | Same 335983036 captured bytes, seven prefill tokens, two completion tokens |
| cache-persist-disk request 1 duration | 2190.6 | 2341.8 | Same 2836 prefill tokens, two completion tokens, 467625568 captured bytes and 203299024 queued bytes |
| same request queue | 25.181 | 117.531 | Same foreground work; prior optional background work differs |
| same request snapshot | 37.756 | 49.728 | Same captured bytes |

The cache-persist-disk request 0 restore diagnostic has filesystem time 68.450 -> 68.396 ms and checksum time 11.241 -> 11.389 ms, but stream wait 51.045 -> 77.744 ms. This identifies a device/transfer-side investigation lead, not proof of the cause. Candidate startup logs confirm three native transfer streams and 37 event leases. Upstream #1195 changes this transfer mechanism.

The new idle frontier task and admission barrier are documented and present in `ComponentTextCache::Lease::Commit` and `Acquire`. Extra durability work is intentional, and queue waits can include it. This does not excuse a user-visible timing regression. In particular, the 129.617 ms snapshot increase above moves the same foreground bytes; it cannot be explained as a larger requested snapshot. Memory pressure, backing growth and interference are hypotheses requiring controls. Phase timings may overlap and must not be summed as disjoint latency components.

## Smallest existing-harness controls

1. Keep the same two production binaries, harness hash, build inputs, model, AR mode, sessions=2, context=8192, 4 GiB RAM cache, sampling preset, `tier=pr`, timeout and disk settings. Use fresh output/disk directories and retain all logs.
2. Alternate one fresh baseline run and one fresh candidate run using the original selected history: `--suite long-context --suite cache`. This is not a four-profile/full-suite matrix. It preserves the exact preceding history that causes pressure demotion and the restarted request sequence.
3. The current runner rejects `--through-case cache:...` (`run.py:337-342`). Selecting only `cache` loses the preceding long-context history; invoking `cache_persist.py` alone loses preceding cancellations and disk-spacing pressure. Those are diagnostic experiments, not matched qualification controls for this pair. The original selected AR history is therefore the smallest supported matched control for these disk flags without modifying the harness.
4. Add a fresh unchanged-baseline control with the same selected history if the second pair still flags the phases above. Use `compare.py --pair ORIGINAL_BASE ORIGINAL_CAND --pair REPEAT_BASE REPEAT_CAND --control REPEAT_BASE BASE_CONTROL --output NEW_FILE`. Keep the eight explained work differences visible; inspect same-work per-request timing classifications rather than presenting the aggregate failed status as a timing result.
5. Compare the same-work snapshot/restore/queue phases individually. Repeated candidate separation from stable baseline confirms regression; overlap or variable controls remain inconclusive. Do not average requests, offset slow requests with faster restores, widen margins, or skip failed samples.
6. For an isolated long-context timing flag only, the existing `--through-case long-context:CASE` can stop after that case while retaining preceding SDK cases. It cannot resolve the cache-disk flags and should not trigger another profile matrix.

The observed selected runs took approximately 207 s (baseline) and 197 s (candidate), including startup/restart; these are elapsed evidence from the report timestamps, not a guarantee for controls.

## Follow-up source review while matched controls run

No obvious merge-specific cache/transfer defect was found. The integrated transfer pool, component cache backing, streaming restore, host allocation implementation and serving cache are unchanged from v0.35.0. The custom runner correctly passes the actual session count to `PlanComponentTransfers`; sessions=2 produces three native streams and 37 event leases. The model adapter retained the upstream lease-level `TransferStream::Synchronize()` in `CopyNativeKeys`. Its remaining whole-stream synchronizations refer to the execution stream at reset/restore/validation boundaries, not accidentally retained native-pool waits. DFlash-only additions are inactive for the observed AR/MTP profiles.

The relevant upstream behavioral changes and limits of the evidence are:

- Transfer leases now share FIFO native streams by `index % native_streams`. A completed lease waits for its own event rather than all later work on that stream. It must still wait for earlier work on that lane. This changes concurrency and possible interference; increased observed stream waiting does not by itself demonstrate a bug.
- `ComponentTextCache::Lease::Commit` can start an optional idle-frontier capture, and `Acquire` waits until such work settles. This is an intentional admission barrier. It creates additional preceding work even when the next request's own byte/token counters match. Existing logs identify publications, but do not measure the barrier separately, so the precise queue-time attribution remains unproven.
- `TextModelRunner::FinishPromptSnapshot` measures wall time from capture scheduling until future settlement. Boundary capture timing also includes reservation and publication. `cache_snapshot_ms` is therefore not a pure copy-throughput measurement, and phase timings can overlap.
- The candidate restarted log reports `backing_prepared committed_bytes=2229338112 long_term_pinned_bytes=0` and no `backing_grown` event. This does not support an assertion of successful long-term pinning. Absence of that event does not exclude background `GrowAhead`, because the event is emitted by the allocation hook when it observes segment growth, whereas the background growth thread commits independently.
- Upstream `HostAllocations` now tries anonymous-page pinning followed by HIP registration, falling back to HIP allocation if pinning fails. Linux pinning uses io_uring buffer registration. The backing-pool pin counter does not establish the branch taken by every separate scratch/staging allocation. No driver-regression conclusion follows from this source change or the current timings.

Existing focused checks are available if repeated evidence points to their contracts: `cache_transfer_pool_test` covers shared lease fences, cached native handles, concurrent draining, capacity bounds, cancellation and failures using controlled fake HIP completions; `component_text_cache_test` includes `IdleFrontierCaptureReturnsBeforeCopyAndAdmissionWaits` and setup-refusal preservation; `qwen38_flash_next.snapshot_copy` checks actual registered-memory transfers and peer capture. These are correctness checks, not substitutes for the matched HTTP timing controls. `cache_frontier.py` retains a dedicated real-model interleaved frontier workflow with raw trace output, but its one-session/2 GiB scenario is a separate diagnostic and does not reproduce the current two-session pressure history.

No additional tests, builds, runtime actions, code changes, main-branch operations or remote operations were performed for this source review. Repeat results remain pending.
