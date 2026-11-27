# OrcaRouter IQ4_XS

> 履歴資料：本文の採用状況・起動状態・未完了事項は作成当時の記録です。現在の仕様は共通カスタム台帳を参照してください。測定値は再集計せず保持し、個人ホームパスを匿名化しています。生ログ・個人設定は公開対象に含めていません。

This extension targets `orcarouter/Qwen3.8-Flash-Next-Uncensored-GGUF:IQ4_XS`.
The initial qualification used the Gufo v0.7.1 + SC117 branch. The adopted
extension is now integrated with official v0.23.0; this update has offline
build/CPU qualification only, not a fresh model/GPU qualification. Other
quantization variants of this repository are outside this qualification.

The inspected Hugging Face revision is
`e43d00f4e2b8b40b89f75e9adeb1045ac34c8acc`. Download all three files named
`Qwen3.8-Flash-Next-Uncensored-IQ4_XS-0000{1,2,3}-of-00003.gguf` and pass the
first shard to `--model`.

Use `--weight-compat off`. Weights remain in their packed GGUF formats on the
GPU. The file contains IQ4_XS expert gate/up and ordinary projections, IQ4_NL
expert/shared down and HC up projections, Q5_K attention projections, and a
Q6_K output head. Supporting those tensors is necessary to run this IQ4_XS
artifact; it does not qualify every quantization variant.

The existing `--weight-compat q8_0` retains its exact SC117 contract: it converts
only the specified 206 weights to Q8_0, not the whole model, and rejects this
OrcaRouter layout. Neither the SC117 allowlist nor its conversion algorithm is
changed. `--served-model-name` is an independent API identifier.

Native decode and speculative verification use FP32 activation row dots with
matching per-row arithmetic. Wide prefill projections use packed-weight WMMA
with temporary FP16 activation/weight fragments and FP32 accumulation. Small
projections (at most 512 output rows) keep FP32 row dots even during prefill,
including the long HC down and inject reductions. No converted GGUF is written
and no expanded full-model weight copy is kept.

## Current local deployment (October 25, 2026)

The stopped Docker container `Gufo` is prepared with the existing Orca-derived
`Qwen3.8-Flash-Next-Uncensored-MTP-draft-gufo-compat.gguf` sidecar, in the same
directory as the three target shards. It is not the historical shared Q8 sidecar
below, and is not interchangeable with the unmodified Orca draft file. This
update changes neither its bytes nor the metadata compatibility policy.

Normal settings remain context 131072, sessions 2, RAM cache 4294967296 bytes,
MTP with adaptive draft length 1–7, prefill chunk 512, thinking off, maximum
output 32768, weight compatibility off, and the existing F16 Vision projector.
These are the user's normal settings, not a new official benchmark prescription.
The complete saved command and verification scope are in
the v0.23 integration record（ローカル保管：artifacts/gufo-v0230-integration.json）.

The v0.23 small draft head keeps each packed output row's actual type. Its
prefix and request table therefore use the existing native Q6_K dense path.
New sampled and lone-greedy eligibility is retained from upstream. This source
inspection does not prove a measured speed gain, sampled quality, or resolution
of the prior C8 exact-output mismatch. Those require model tests.

## Historical initial qualification

Example with the previously qualified shared Q8 MTP sidecar (not the current
container command):

```sh
gufo serve --host 127.0.0.1 --port 8080 --sessions 1 llm \
  --model /models/OrcaRouter-Qwen3.8-Flash-Next-Uncensored-IQ4_XS/Qwen3.8-Flash-Next-Uncensored-IQ4_XS-00001-of-00003.gguf \
  --served-model-name orcarouter-iq4xs \
  --weight-compat off --context 262144 --cache-ram-bytes 1 \
  --think off --max-tokens -1 --speculative mtp --draft-tokens 7 \
  --mtp-model /models/MTP/mtp-Qwen3.8-Flash-Next-shared-Q8_0.gguf
```

For AR, use `--speculative off` and omit `--mtp-model` / `--draft-tokens`.
The shared MTP SHA256 is
`5ff54097406a905cf3a724c709124ceb0e3e10235ee862298969e91c96fa96e6`.
The repository's separate `MTP-draft.gguf` has all-zero attention compression
metadata, including the trunk attention layers. It is not interchangeable with
this shared sidecar; the existing architecture validation remains enforced.

Focused numerical tests:

```sh
cmake --build --preset gpu-test --target qwen38_flash_next_native_iq4_ops_test
ctest --preset gpu-full -R '^qwen38_flash_next.native_iq4_ops$' --output-on-failure
cmake --build --preset cpu-test --target qwen38_flash_next_iq4xs_bind_probe
build/cpu-test/tests/models/qwen38_flash_next/qwen38_flash_next_iq4xs_bind_probe "$MODEL" "$MTP"
```

The operator test compares GPU results with independently decoded CPU weights
and double-precision dot products. It covers real reduction widths, partial
output tiles, exact allocation boundaries, packed IQ4_XS embedding lookup, and
identical arithmetic across 1/2/7/8/9/32/33/129/256/257 token rows. Model-level and memory
results are recorded separately from these operator checks.

FP32 hyper-connection prefill keeps the same normalization reduction for every
chunk width, including 1–15 token tails. Decode keeps its existing arithmetic.
The HC operator test checks bitwise residual/norm equality for chunks of
1/8/9/15/16 tokens; the session test checks split prompts and sampled MTP replay.

## Prefill execution paths

The inspected Huihui Q4_K_XL file uses Q8_0 for most ordinary projections,
Q4_K/Q5_K for expert gate/up, and Q5_1/Q8_0 for expert down. OrcaRouter's
corresponding expert tensors use IQ4_XS and IQ4_NL. The filename alone does
not select a kernel; dispatch uses each bound tensor's actual GGUF type.

The IQ4 expert prefill now enters the same `Executor::MoeExperts` WMMA route
as these Q4 experts: `RoutedCompact`, token tile planning, packed LDS staging,
`RoutedF16GEMMKernel`, whole-K FP32 matrix-core accumulation, paired gate/up
SwiGLU, and the F16 down/mixture epilogue are shared implementations. Only
packed weight fetch and decode are specialized by format. Decode and MTP
verification retain the existing vector/MMQ paths.

IQ4 uses a nonlinear 16-entry codebook, whereas Q4_K and Q5 formats use an
affine scale/bias. IQ4_XS additionally has a signed six-bit sub-block scale.
Its scale stays FP32 until the decoded weight rounds to F16; using Q4's
half2 scale/bias arithmetic would add a rounding step. The codebook decoder
in `iq4_half.hpp` is shared by the ordinary IQ4 prefill and routed experts.
Only matrix fragments expand in registers, after loading the original packed
bytes. There is no new persistent weight allocation or model conversion.

Wide ordinary projections and the routed experts also use Q4/Q8's
`PrepareHalfInput` to prepare and reuse the same temporary F16 activation
rows. It does not change the weights or the amount of persistent storage.

Small IQ4 projections retain their original FP32 dot-product specialization.
Following Q4's small-projection launch organization, eight-token groups now
share one launch grid, with the existing launcher handling a short remainder.
Their reduction order stays unchanged. Replacing these long reductions with
Q4's F16 or int8 arithmetic is not valid under their existing numerical
contract. Wide ordinary projections already share the generic
`HalfPrefillGemmKernel` template with Q4_K/Q5_K/Q6_K/Q8_0; Q4_K_XL's separate
Q8_0-specialized dense and fused SSM paths remain selected by their actual
weight types and shape conditions.

The routed operator test retains the Q4 checks and error limits, adding IQ4
real shapes, ragged tails, tile-width equivalence, paired SwiGLU, and an
independent scalar codebook / FP64 oracle. Throughput and model-level
qualification are recorded in BENCHMARKS.md and QUALITY.md after validation.

For the text-only artifact, the independent MTP probe accepts
`--mtp-model "$MTP" --mtp-audit-text-only`. The default `--mtp-audit`
still requires the image sidecar and includes its image cases. The full
vocabulary check reads the actual Q6_K output head rather than interpreting
its bytes as Q8_0; candidate IDs/logits still require exact equality.
