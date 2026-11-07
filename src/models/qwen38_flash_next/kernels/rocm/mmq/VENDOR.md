# Quantized HIP kernels

Adapted from `ggml-org/llama.cpp` at
`5c0e9468378eba6bf3cc1989ff5d62fbbe4d9e3a` (MIT; see `LICENSE`).
The upstream source directory is `ggml/src/ggml-cuda`; Gufo builds these
sources with HIP for AMD Strix Halo (`gfx1151`).

`qfn_mmq.h` is the public adapter. It supplies Q8_0 dense projections and
Q4_K, Q5_K, Q5_0, Q5_1, Q8_0, IQ2_S, IQ3_XXS, IQ3_S, IQ4_XS, IQ4_NL
and Q2_0 routed expert projections. Paired matrix gate/up projections share
input preparation for Q4_K, Q5_K, Q8_0, IQ2_S, IQ3_XXS, IQ3_S and IQ4_XS;
other formats retain their vector fallback.
`qfn_ggml_stubs.*` supplies the minimal context and scratch pool without the
ggml graph runtime. Gufo owns scheduling and HIP graphs.

The kernels use native HIP runtime, hipBLAS and HIP numeric types. The
upstream runtime aliases, NVIDIA capability dispatch and PTX branches have
been removed; retained matrix fragments use AMD WMMA/MFMA. Headers use the
`.hpp` suffix. Gufo owns scheduling and HIP graphs.

## Native mixed-format support and local corrections

The five IQ formats enable the vector dot products and tiled matrix kernels
already present in the pinned import above. The import as a whole has not
been rebased. Their storage layouts and scalar decoding were independently
checked against llama.cpp commit
`11fe02151f79c41d0d4af7da708755d73b9c0da6`:

- [ggml-common.h](https://github.com/ggml-org/llama.cpp/blob/11fe02151f79c41d0d4af7da708755d73b9c0da6/ggml/src/ggml-common.h)
  defines the packed blocks and IQ codebooks.
- [ggml-quants.c](https://github.com/ggml-org/llama.cpp/blob/11fe02151f79c41d0d4af7da708755d73b9c0da6/ggml/src/ggml-quants.c)
  supplies the scalar decoding and Q8 reference conversion rules.

Local changes in `common.hpp`, `qfn_ggml_stubs.h` and `mmvq.hip.cpp` add
Q2_0 (GGML type 42): one FP16 scale and 16 packed bytes encode 64 values,
with contiguous two-bit codes representing `{-1, 0, 1, 2}`. Its vector dot
product uses both 32-value Q8 activation scales. Q2_0 currently uses this
packed vector implementation in the MMQ fallback at every batch width.
The native routed WMMA prefill path has a separate Q2_0 specialization.

Two local corrections in `vecdotq.hpp` preserve fractional dot products
that the imported IQ2_S and IQ3_XXS integer expressions truncated. IQ2_S
forms `(2*ls0+1)*sumi0 + (2*ls1+1)*sumi1` and applies `1/8` in floating
point; IQ3_XXS forms `(2*ls+1)*sumi` and applies `1/4` in floating point.
Exact regression cases require IQ2_S outputs `+/-5.375` and IQ3_XXS outputs
`+/-15.5`; the previous integer divisions returned `+/-5` and `+/-15`.
The newly enabled formats preserve nonfinite results rather than using the
legacy Q4/Q5/Q8 vector result sanitization.

The adapter retains the actual format-specific row and expert strides.
IQ4_NL tiled loads sweep 256 values even for a partial final K tile. Their
activation padding is zero, and the weight allocation must have at least
126 readable zero bytes after the whole tensor. `DeviceModel` supplies a
4096-byte zero tail. At the down-projection width K=640, each row is
20 blocks (360 bytes); the last tile reads 72 additional bytes. Rows and
experts are not padded or expanded.

`native_expert_ops_test.cpp` checks the six IQ/Q2_0 vector/prefill paths
against values generated directly by the pinned upstream scalar decoders at
K=2560/640. Q5_0 additionally uses directly encoded five-bit codes and an
independent signed-scale reference, including the full down-projection shape. It covers 512 physical expert strides, the last expert, inactive
slots, ragged batches and the exact fractional-dot goldens above. Exact dyadic
activations make Q8 input quantization lossless; FP64 reference dots use the
ordinary FP32 forward-error bound without hiding activation error.
`routed_wmma_ops_test.cpp` retains the independent F64 checks for the F16
prefill route and packed MMQ comparisons, including production shapes and
paired projections. Scalar fixture inputs, outputs and source hashes live in
`tests/models/qwen38_flash_next/fixtures/native_quant.{bin,json}`.
Operator validation does not replace full-model AR/MTP or continuation checks.

When importing an upstream fix, preserve the Gufo namespace, adapter and
HIP-only dispatch, update the pin, and run the Flash-Next operator tests
under `tests/models/qwen38_flash_next/` plus the AR/MTP session replay.
The routed GEMM test compares supported formats against a floating-point
reference and checks the paired adapter against two separate projections.
Build and validate through Nix as described in `AGENTS.md`.
