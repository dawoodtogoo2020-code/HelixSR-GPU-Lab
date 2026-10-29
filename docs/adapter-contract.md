# Adapter metadata contract

The contract is host-tested groundwork, not a game hook, GPU implementation, installer, or claim of PS5 compatibility. It checks caller-supplied metadata and attestations; it cannot prove resource contents, allocation bounds, actual synchronization, hardware features, backend correctness or recovery. Synthetic test data is not hardware evidence.

The caller supplies an exact device, title, build and executable hash, backend and model hashes, qualified adapter and backend, dimensions, formats, motion-vector conventions, depth, jitter and exposure, capability requirements, resource metadata, synchronization and lifetime attestations. The checker refuses unsupported or inconsistent input. On acceptance, honor the requested history reset; drain in-flight work when indicated. A refusal before dispatch permits the original path only if its resources remain valid. Post-dispatch failures need separately implemented synchronization and recovery. Update previous-frame state only for the actual result used.

An adapter needs per-build evidence for its live input bindings and recovery path. Neither a game mentioning FSR nor an entry in a registry establishes compatibility. The contract does not attach, execute shaders or submit GPU work. An independent activation policy is still needed before any integration.

Identity keys are nonempty ASCII letters, digits, dot, underscore, colon or hyphen, with bounded termination. Hashes are lowercase SHA-256 text. The previous activation-policy dependency was replaced with a local identity type and validation; this does not reproduce the activation policy itself.

The original metadata checker, benchmark and host tests were developed for the GPU Lab portability foundation. They have been adapted here to remove a local activation-policy dependency and private paths. They contain no upstream HelixSR runtime, game code, model weights or shaders. The benchmark compares supplied measurements and cannot verify their origin or accuracy.

Run `sh tests/run_native_tests.sh` with a C11 compiler. Run `python -m unittest discover -s tests -p test_benchmark.py` for the benchmark tests. Native build output is written to `.build/`.
