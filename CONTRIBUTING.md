# Contributing

Please start with an issue describing the exact game title/build, the proposed change and the evidence you can share. A detected FSR string does not qualify a game for activation.

Keep changes independently reviewable. Run the Python tests and native host tests described in the README. A new game adapter needs exact identity, observed resource conventions, synchronized lifetimes, history reset, original-path recovery and live validation before it can be called supported. Host fixtures alone are insufficient.

Contributions must be original or have a clearly documented compatible license. Include provenance and preserve third-party notices. Do not submit game assets, proprietary SDK files, translated model bodies, driver-derived weights, credentials or private machine details. The MIT license applies to the original code in this repository; it cannot relicense third-party material.

For benchmarks, provide matched resolution, model, timing boundaries, calibration and raw individual observations. Separate GPU intervals from frame time, FPS and end-to-end input latency. Report failed runs and scene differences.
