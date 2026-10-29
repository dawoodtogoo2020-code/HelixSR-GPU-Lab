# Validation record

This public summary records the lab's bounded results as of 2026-10-29. Private recordings, game data and device-specific logs are not included. These results describe the tested lab integration, not a reproducible public PS5 installation.

## CONTROL rendering work

- Exact title/build: PPSA34547, 01.300.300.
- Performance: 1505 × 847 input to 2560 × 1440 output.
- Cinematic/Quality: 2258 × 1270 input to 3840 × 2160 output.
- An experimental combined resource graph added 82 missing shader-resource references and passed bounded Quality/Performance switching, including Q/P/Q/P. Each phase included eight dimension samples; eight diagnostic frames across the experiment were accepted.
- This supports startup and mode-switching observations only. It does not establish all-frame correctness, a complete visual-quality audit, history-reset/fallback correctness, controlled traversal performance or locked 60 FPS.
- That combined experiment was restored afterward. The retained baseline remains r17. A previous launch-hang cause remains unresolved.

## Selective k5f experiment

| Measurement | Control | Candidate | Interpretation |
| --- | ---: | ---: | --- |
| Isolated k5f median, raw ticks | 60,140 | 55,800 | 7.22% lower in one synthetic paired experiment |
| Sum of instrumented kernel medians, raw ticks | 290,738 | 276,954 | 4.74% lower; excludes host gaps |
| Live full-span median, raw ticks | 388,608 | 387,072 | 0.395% lower descriptively; inconclusive |

The isolated pair used 100 launches per arm and six frames. All six final images passed the existing numerical thresholds (relative L2 below 0.001 and PSNR at least 45 dB). The candidate's largest relative L2 was 0.0009576; minimum PSNR was 65.706 dB. The tested arms both included the enc1 change and were not bare r17.

The live pair compared enc1 against enc1+k5f. The scene routes, camera contact, dwell and execution order differed, so the median difference cannot be attributed to the kernel change. A candidate stream stall required reconnection; its onset and cause were not established. The median admission period was unchanged. No candidate was promoted, and r17 was restored and hash-checked.

Raw ticks are not calibrated milliseconds, gameplay FPS or input latency. No matched direct stock-FSR2 timing or parity result has been established.

## Public tooling tests

The CI workflow validates the bundle verifier, benchmark comparator, standalone C metadata contract and public site/repository consistency. Synthetic host tests establish those tools' behavior only. They do not validate GPU submission, live resources, PS5 firmware compatibility or console installation.
