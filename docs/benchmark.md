# Benchmark comparison

Run `python tools/benchmark.py baseline.json candidate.json`. Use a separate copy of [the blank template](../examples/benchmark-template.json) for each arm and enter the actual individual samples and observed metadata. The blank template intentionally refuses comparison.

Required metadata describes the game/version, scene, model hash, precision, actual input/output sizes, dynamic-resolution policy, GPU or CPU timing type and measured interval boundary. Record device, driver, compiler, build flags, shader-graph hash and backend version as well.

Samples may be calibrated milliseconds or ticks. Tick records additionally require a positive `tick_frequency_hz`. Calibration must be verified with a stated method and nonnegative uncertainty in milliseconds. These are caller-supplied attestations; the tool does not establish that calibration happened. Never convert raw ticks into milliseconds using an assumed frequency.

Unknown values and mismatched workload metadata refuse comparison. Differences in device, compiler, driver, compilation information and graph are disclosed in `environment_differences`; they do not automatically refuse a descriptive comparison. Therefore `comparable: true` does not establish a controlled optimization experiment or a causal speedup. Read every difference and verify the underlying scenes and measurements yourself.

Output includes sample counts, descriptive percentiles, median ratio and declared calibration uncertainty. Those bounds are not statistical confidence intervals. Summed kernel intervals, total GPU time, frame time, FPS and input latency are different measurements.

Exit codes: 0 for comparable supplied records, 1 for refused metadata, and 2 for usage, parse or file errors. Tests use synthetic records only.
