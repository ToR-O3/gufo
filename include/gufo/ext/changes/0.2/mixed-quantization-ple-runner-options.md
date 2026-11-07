# Mixed quantization and external PLE runner options

Retain the optional `TextRunnerOptions::ple_model_path` in the component-cache
API series. An empty path uses the embedded PLE table; providers validate an
external table and include its artifact fingerprint in the component-cache
identity. Retain the forwarded Q2_0 tensor format and packed Q2_0, IQ2_S and
IQ3_XXS block declarations from the previous compatibility series. These
additions do not change default provider behavior.
