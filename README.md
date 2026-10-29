# HelixSR GPU Lab

**An experimental PS5 upscaling project, starting with CONTROL Resonant.**

[Project website](https://dawoodtogoo2020-code.github.io/HelixSR-GPU-Lab/) · [Developer preview](https://github.com/dawoodtogoo2020-code/HelixSR-GPU-Lab/releases) · [Compatibility](docs/compatibility.md) · [Installation status](docs/installation.md) · [Roadmap](docs/roadmap.md)

[Join Blackheart 909 Gaming on Discord](https://discord.gg/k4UWq8DqK) · [Support my work on Ko-fi](https://ko-fi.com/blackheart909)

GPU Lab is working toward a simple way to discover compatible games, select a qualified upscaler, and return to the original rendering path. The lab has demonstrated a game-specific HelixSR integration in CONTROL Resonant. Extending that work to more PS5 games requires an adapter and validation for each exact game build.

> **Release status: developer preview.** This repository contains host tools and a metadata contract. It does not include a PS5 application, injection payload, neural model, or installable game patch. There is currently no public end-user upscaler bundle. Downloading this preview does not enable HelixSR on a PS5.

## Compatibility today

| Game | Evidence | Public installation |
| --- | --- | --- |
| CONTROL Resonant, PPSA34547, 01.300.300 | Bounded live integration in the private lab. Experimental Performance and Cinematic mode work is documented below. | Not released |
| SAROS | Candidate for investigation; historical static discovery only. | Unsupported |
| Other PS5 games using FSR | Long-term target. No qualified adapter in this release. | Unsupported |

FSR detection is a starting point, not compatibility proof. Temporal upscaling needs the right color, depth, motion, exposure, jitter, history and output resources, with correct synchronization. A PC upscaler DLL cannot simply be dropped into a PS5 title. See the [adapter contract](docs/adapter-contract.md).

## What you can use now

- **Bundle verifier:** validates a locally supplied support manifest and prints a dry-run installation plan. It never connects to a console. There is no approved bundle yet.
- **Temporal adapter contract:** a standalone C gate for checking caller-supplied identity, resource and lifecycle metadata. It does not hook a game or submit GPU work.
- **Benchmark comparator:** refuses mismatched workloads and reports descriptive comparisons from supplied measurements. [Input format and limits](docs/benchmark.md).
- **Compatibility, evidence and release requirements:** a public record of what works in the lab and what still needs validation.

Download the developer preview from [Releases](https://github.com/dawoodtogoo2020-code/HelixSR-GPU-Lab/releases), extract it on your computer, and use Python 3.10 or newer:

```sh
python tools/install_support.py --help
python tools/benchmark.py --help
python -m unittest discover -s tests -p 'test_*.py'
```

The C contract can be tested on a Linux host with a C compiler:

```sh
bash tests/run_native_tests.sh
```

Start with [installation status](docs/installation.md) before using any bundle. Do not send the source archive to a payload loader.

## What the lab results establish

The CONTROL integration is specific to the recorded title and build. Performance mode has used 1505 × 847 input at 2560 × 1440 output. An experimental combined resource fix passed bounded Performance/Cinematic switching, with Cinematic using 2258 × 1270 input at 3840 × 2160 output. That experiment is separate from the retained r17 baseline.

An isolated k5f experiment reduced its measured kernel interval by 7.22%, but the live enc1 versus enc1+k5f comparison was inconclusive. Its roughly 0.40% lower measured interval occurred across different scene paths. No candidate was promoted from that test, and no locked-60-FPS, input-latency or stock-FSR parity claim is supported. [Evidence and limits](docs/validation.md).

## Toward a simple PS5 install

The intended flow is **check compatibility → install GPU Lab → choose a qualified game → apply → verify → restore when needed**. A future payload can automate delivery, but cannot supply missing game adapters or redistribution rights.

Before that download can ship, we need a reproducible native build, distributable runtime/model assets, per-build packages that do not bundle game files, and a clean installation and recovery test on a separately configured supported console. [Release roadmap](docs/roadmap.md).

## Contribute

See [CONTRIBUTING.md](CONTRIBUTING.md). Useful contributions include independently licensed adapters, resource/lifecycle validation, portable packaging and reproducible matched benchmarks. Report only non-sensitive metadata. Do not upload game dumps, SDKs, model weights, keys, private device addresses or raw recordings containing personal information.

## Community and support

Report bugs, ask questions and share feedback in the [Blackheart 909 Gaming Server](https://discord.gg/k4UWq8DqK). Join the server first, then choose the relevant channel under **HelixSR**:

- [HelixSR Plugin](https://discord.com/channels/1555240126139867169/1565267596863864852): Decky plugin questions and issues.
- [HelixSR PlayStation 5](https://discord.com/channels/1555240126139867169/1565267777135312987): GPU Lab and PS5 research questions and issues.
- [HelixSR Armada](https://discord.com/channels/1555240126139867169/1565267979422138409): Armada Linux plugin questions and issues.

Include your device, relevant versions, reproduction steps, expected result and actual result. Remove private information from logs and screenshots. You can also use [GitHub Issues](https://github.com/dawoodtogoo2020-code/HelixSR-GPU-Lab/issues) for reproducible GPU Lab bugs. [Reporting guide](docs/community.md).

If you would like to support my development and testing, visit [Blackheart909 on Ko-fi](https://ko-fi.com/blackheart909). Support is optional; reporting an issue and using public releases do not require a donation.

## Credits and licensing

This is an independent PS5 research project maintained by Dawood. [Lone Wolf's HelixSR](https://github.com/lonewolf0622/HelixSR) is the upstream PC project. This repository is not an official Sony, AMD, NVIDIA or upstream HelixSR release.

The original tooling and documentation included here are [MIT licensed](LICENSE). That license does not grant rights to upstream HelixSR, game code, SDKs, models or generated shaders, none of which are bundled. [Provenance and third-party boundaries](docs/provenance.md).
