# Mixed quantization and external PLE runner options

Add the optional `TextRunnerOptions::ple_model_path` for text providers that
load an external PLE n-gram table. An empty path retains the embedded-table
behavior. Model-specific providers validate the sidecar and bind it to durable
continuation identities. Add Q2_0 tensor storage and the canonical packed
Q2_0, IQ2_S and IQ3_XXS block layouts to the forwarded quantization surface.
Existing tensor values and default runner behavior remain unchanged.
