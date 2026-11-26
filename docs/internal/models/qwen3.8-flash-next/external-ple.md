# Flash-Next external PLE in this fork

When you construct `TextRunnerOptions` in C++, assign Flash-Next's external PLE
path by name:

```cpp
gufo::ext::TextRunnerOptions options{};
options.model_path = model_path;
options.provider_options["--ple-model"] = ple_path;  // std::string
```

In API series 0.3, this fork retains the legacy `ple_model_path` field before
`provider_options`. The upstream ten-argument positional initializer therefore
needs the named assignment above. Existing fork callers can keep using the
legacy field. If you set both, the provider option takes precedence. Migrate
to the named option before the next upstream compatibility series.

