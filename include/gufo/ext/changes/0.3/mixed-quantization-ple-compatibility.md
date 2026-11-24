# Mixed quantization and external PLE compatibility

This fork retains the public Q2_0, IQ2_S, IQ3_XXS, IQ3_S, IQ4_XS and IQ4_NL
dequantization helpers and GGUF type additions used by mixed-quantization models.

TextRunnerOptions retains ple_model_path in its existing fork position before
provider_options. New callers should use named provider_options with ple-model;
that option takes precedence over the legacy field. The legacy positional
layout remains source-compatible with earlier fork releases in the 0.3 series,
but differs from upstream positional aggregate initialization. Modules must be
rebuilt against the installed fork SDK and its build identity.
