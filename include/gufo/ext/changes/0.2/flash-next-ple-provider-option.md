# Flash-Next PLE provider option

The Flash-Next provider reads `--ple-model` from
`TextRunnerOptions::provider_options`. Direct callers can still supply
`TextRunnerOptions::ple_model_path`. An explicit provider option takes
precedence over the legacy field.
