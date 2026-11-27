# v0.37 fork review disposition

The v0.37 integration remains on its review branch. Production stays on the
v0.36 main revision, 53a217cb, pending a later adoption decision.

## Performance investigation

The reviewed execution code is b0e2750b. Repeated end-to-end measurements
showed slower results in several workloads than the v0.36 baseline.

- The inspected Stages paths retained the existing optimized kernels.
- The pending-token correction did not execute in the inspected normal
  Flash-Next DFlash2 path; retain the upstream correction.
- The repaired Q4_K dense draft-head path matched the previous routed path
  bit for bit in focused synthetic checks. It was faster at four and eight
  rows. These operator timings do not establish an end-to-end MTP gain.
- Intermittent waits remain unresolved. Host-side profiling showed HSA waits
  in both revisions, but did not isolate GPU execution, driver behavior or
  external load. Do not attribute the regression to any one of these causes.

Retain the integration and its correctness fixes for future review. No further
performance changes are selected from this investigation.

## Extension extraction

Defer extraction of the customizations into external extensions. WeightSource
can separate external PLE loading from execution, but that separation does not
itself improve inference speed. Current Stages contracts do not expose all
prepared Q8_1/F16 inputs and HC/PLE operations needed by the existing fast paths.
Supplying external Stages also disables some intermediate-elision paths to
preserve input correctness. Do not remove those guards merely to enable an
external extension.

Revisit extraction when the public contracts preserve activation reuse without
additional conversion or copying. Shared cache-retention and page-pool changes
remain core concerns. Keep machine-specific settings and raw operational
records outside the published source tree.
