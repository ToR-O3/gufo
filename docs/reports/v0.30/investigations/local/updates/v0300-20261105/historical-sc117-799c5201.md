# Historical SC117 implementation record

> 履歴資料：本文の採用状況・起動状態・未完了事項は作成当時の記録です。現在の仕様は共通カスタム台帳を参照してください。測定値は再集計せず保持し、個人ホームパスを匿名化しています。生ログ・個人設定は公開対象に含めていません。

Source: git object `799c5201:docs/models/qwen3.8-flash-next/SC117.md`. Preserved as historical evidence; not a new measurement.

# SC117 GSQ-RCO IQ3_S compatibility

This custom branch adds the packed routed-expert types IQ2_S, IQ3_XXS,
IQ3_S, IQ4_XS, IQ4_NL and Q2_0. Q2_0 is GGML type 42 (64 values/18 bytes),
distinct from Q2_K. Each tensor retains its own storage type and expert stride.

The inspected SC117 revision is `eec4e10a2c29b440a2ae85591fa11569ce752963`.
Its two original GGUF shards remain unchanged. The 144 expert tensors use
50,292,326,400 packed bytes; the 28,800,138,240-byte IQ4_NL PLE table keeps
the existing bounded disk-row reader.

## Explicit dense compatibility mode

Pass `--weight-compat q8_0` to `serve llm`, `prompt`, `chat`, or `bench`.
The only accepted values are `off` (the default) and `q8_0`. The former
`sc117-iq3s-q8` option value has been removed and is not an alias.

This mode additionally quantizes only the specified 206 ordinary weight
tensors to Q8_0. It does not convert the whole model and is not lossless.
The supported model scope is unchanged: it requires the inspected SC117
artifact above, not arbitrary Qwen or GGUF models. Before GPU allocation,
the mode validates each selected tensor's exact name, original type and
shape. Unexpected layouts are rejected. Routed experts and the PLE table
retain their original packed formats. Source GGUF offsets/types stay distinct
from the device execution type.

The selected ordinary matrices change from 2,336,343,040 to 3,556,802,560
payload bytes (+1,220,459,520). The conversion reads bounded chunks, decodes
one F32 row, rounds Q8 codes using the pinned llama.cpp reference rules,
and transfers the bytes through one reusable pinned buffer. It logs the
source/runtime type, payload sizes, SHA256 of each converted tensor,
elapsed conversion time and scratch high-water mark. The conversion pool
is separate from Gufo's existing raw weight-upload staging pool; neither
payload arithmetic nor scratch accounting is a total RAM/VRAM measurement.

BF16 router and SSM alpha/beta stacks convert to the existing F16 execution
representation. F16 PLE convolution coefficients expand to F32. Nonfinite
conversion input and FP16 overflow are rejected.

## Initial local serving command

```sh
gufo serve --host 127.0.0.1 --port 18080 --sessions 1 llm \
  --model /models/Qwen3.8-Flash-Next-GSQ-RCO-abliterated-GGUF/IQ3_S/Qwen3.8-Flash-Next-GSQ-RCO-abliterated-IQ3_S-00001-of-00002.gguf \
  --served-model-name sc117-iq3s-q8 \
  --weight-compat q8_0 --context 4096 --cache-ram-bytes 1 \
  --think off
```

Qualify short AR first, then longer inputs, then MTP. Do not enable the disk
conversation cache for these comparisons. A context capacity setting alone
does not demonstrate successful long-input inference.

For this Windows/WSL RocDXG environment, preserve the verified `/dev/dxg`,
RocDXG and `libdxcore.so` mounts and set `HSA_ENABLE_DXG_DETECTION=1` plus
`LD_LIBRARY_PATH=/usr/lib:/opt/rocm/lib`. Container ports must bind only to
host loopback. The standalone binary's host binding inside a published-port
container must be `0.0.0.0`; the Docker publish binding remains `127.0.0.1`.

## MTP artifact choice

The inspected standard SC117 Q8 MTP file uses a different head layout from
Gufo's shared sidecar. A previously downloaded official Unsloth shared
Q8_0 sidecar has been independently rechecked against the SC117 main model:
its architecture/inference constants pass `MtpMatches` and `MtpWeights::Bind`.
Its SHA256 is `5ff54097406a905cf3a724c709124ceb0e3e10235ee862298969e91c96fa96e6`,
size 2,786,568,256 bytes, Unsloth revision
`38bb39ee97821de2c9009abb7e93950eec396e66`.
This is a separate original-model draft; the SC117 target performs acceptance
verification and supplies its converted embedding/output. Artifact validation
alone is not a successful AR/MTP equivalence or acceptance-rate test.

## Numerical checks

The model tests contain independent upstream CPU fixtures, encoded Q8 golden
bytes, exact real-width GPU expert cases, and multi-chunk conversion/transfer
checks. The `gpu_probe` and `reference_probe` accept the same explicit mode.
The CPU reference re-encodes each marked row to exactly the runtime Q8 bytes
before using it. The reference probe's `--original-weights-ref` compares
against original SC117 source weights without creating another GGUF.

Record three comparisons separately: source CPU/GPU expert arithmetic;
same converted weights CPU/GPU inference; original/converted model quality.
Retain performance/memory results and all unresolved differences alongside
the exact source commit, binary/image identities and invocation.

## Shared Q4 prefill execution (October 6, 2026)

The artifact label IQ3_S does not describe every expert matrix. Header
inspection finds gate/up in 20 IQ2_S, 17 IQ3_XXS, 10 IQ3_S and one IQ4_XS
layers; down is IQ4_NL in 39 layers and Q2_0 in nine. The Huihui Q4_K_XL
control has 47 Q4_K and one Q5_K gate/up layers, with 43 Q5_1 and five Q8_0
down layers. Dispatch uses these tensor types, not either filename.

Loading still follows `ModelWeights::Bind` and the existing device loader.
The selective conversion allowlist and loader are unchanged. In prefill,
`Executor::MoeExperts` now routes all four existing SC117 gate/up formats
and both down formats through Q4's `RoutedGatedF16Gemm` / `RoutedF16Gemm`
and the same `RoutedF16GEMMKernel` template. They share token grouping,
compact routing, activation preparation, LDS layout, WMMA FP32 accumulation,
paired SwiGLU and the half-output down epilogue. Previously only layer 47
used that path; other layers used MMQ, including a vector Q2_0 fallback.

`iq_prefill.hpp` contains only the additional packed-format fetch/decode.
IQ2/IQ3 require their existing nonlinear grids, signs, high bits and fractional
FP32 subscales; Q4's affine decode cannot represent those weights. Q2_0
uses its original 64-value/18-byte layout. The decoder reuses pinned MMQ
grid definitions, stages compact codes, and rounds each decoded operand to
F16 only in registers before WMMA. IQ2_S needs one extra 1 KiB scale plane
in block-local LDS. There is no persistent expanded matrix, second device
weight copy, new loader conversion, or extra runtime option.

The ordinary converted Q8 projections already use the shared dense paths.
SC117's BF16 HC and PLE projections now also use the existing dense WMMA
template, with original BF16 operands and a fixed accumulation order.
The previous hipBLASLt path changed sums with a token's position in a batch;
this reproduced on the saved pre-change binary and broke split-prefill
logit equality. The fix is limited to the five actual HC/PLE matrix shapes.
Q4's existing rotated-order BF16 indexer specialization is unchanged.

Single-token AR and non-prefill MTP verification retain their existing
MMQ/vector arithmetic. The PLE table retains its bounded packed disk-row
reader. These operations are outside the large-batch routed prefill change.
Numerical/replay qualification is recorded in QUALITY.md（ローカル保管：QUALITY.md）,
and bounded timing results in BENCHMARKS.md（ローカル保管：BENCHMARKS.md）. Neither is a
substitute for renewed 32K/128K/256K or Windows physical-RAM measurements.
