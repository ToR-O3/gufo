# External BF16 PLE support for Huihui UD-IQ4_XS

> 履歴資料：本文の採用状況・起動状態・未完了事項は作成当時の記録です。現在の仕様は共通カスタム台帳を参照してください。測定値は再集計せず保持し、個人ホームパスを匿名化しています。生ログ・個人設定は公開対象に含めていません。

Status: implementation, download, build, CPU tests and minimal real-model validation complete. The user explicitly authorized startup. **Gufo is running Huihui UD-IQ4_XS with external BF16 PLE under the normal settings.**

`gufo serve llm --ple-model PATH` replaces only the embedded PLE table. The trunk GGUF, GPU kernels, BF16 row decoder, bounded host cache and downstream inference path are unchanged. The loader checks architecture, PLE metadata and exact tensor dimensions. Persistent continuation identities include the bound external PLE artifact fingerprint. Omitting the option retains the embedded table.

## Artifact

- Path: `/home/user/Models/PLE/Qwen3.8-Flash-Next-PLE-BF16.gguf`
- Source: https://huggingface.co/Delgon/Qwen3.8-Flash-Next-PLE-GGUF/resolve/5de86c89b5b78fc2711744475b4da50ddd9acb72/Qwen3.8-Flash-Next-PLE-BF16.gguf
- Size: 102400492480 bytes (95.37 GiB)
- Verified SHA-256: `0ac215cf0c5767ca20c463ad467eb31341e0ce0ca6cc8eb4f934d4b5f6d1932d`
- Original trunk shards remain required and unchanged.

## Validation

- Formatting: 695 C++ files passed.
- CPU tests: n-gram sidecar binding/reads/rejections, model configuration, GGUF reader passed.
- Actual GGUF metadata: all 11 required entries match in both type and value.
- ROCm 10.1 build passed. New missing-initializer warnings were resolved with explicit empty defaults; other compiler warnings are retained in the build logs.
- New image passed `--help` and `serve llm --help`, without a model mount or GPU device.
- Two minimal real-model requests passed: a 33-token short prompt and a 256-token repeated prompt. Both completed with HTTP 200 and accepted MTP drafts.
- No full benchmark or long-context stress test was run; the user requested minimum validation.

## Prepared deployment

Image: `gufo:v0.27.0-ud-iq4xs-ple-bf16-rocm101-20261031`

Binary SHA-256: `5699a8c70fed2b2c36ab8733100168ae71069a057ea08ada6a87ffd8dd9d43db`

Source base: `2a6beddcca7160406694a52517d5fdbf9abbae93`; branch: `feat/qwen38-flash-next-ple-model`.

Source: `/home/user/Apps/Gufo/deployments/bf16-ple-v0270-20261031/source`

Review patch: `/home/user/Apps/Gufo/deployments/bf16-ple-v0270-20261031/checks/implementation.patch`

Candidate arguments: `/home/user/Apps/Gufo/deployments/bf16-ple-v0270-20261031/normal/docker-create.json`

The candidate preserves the served model name, two 131072-token sessions, 4 GiB RAM cache, MTP, vision sidecar, ROCm 10.1 and `AMD_SERIALIZE_COPY=2`. Normal settings now reference the new image and external PLE. The previous container is retained stopped as `Gufo-pre-ple-bf16-20261031`. The builder is stopped.

Detailed machine-readable evidence: `/home/user/Apps/Gufo/deployments/bf16-ple-v0270-20261031/checks/validation.json`.

Runtime evidence: `/home/user/Apps/Gufo/deployments/bf16-ple-v0270-20261031/normal/summary.json`. The post-validation Windows free physical memory snapshot was 5.03 GiB. The smallest sampled WSL MemAvailable was 16.82 GiB; these scopes must not be added. Windows peak memory was not measured.
