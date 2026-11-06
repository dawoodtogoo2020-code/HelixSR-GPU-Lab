# One-load K-Stuff Lite + A53 setup for firmware 11.60

GPU Lab can build and install one combined **Kstuff-Lite-v1.11-A53-11.60.elf**. Its A53 initialization and original Lite kernel run in one process from one Payload Manager entry. The separate bridge is no longer part of this recipe. This is a GPU Lab integration of exact existing inputs, not an official upstream Lite release.

## Set it up once

1. Download and unzip **HelixSR-GPU-Lab-v0.3.2-lite-a53-beta.zip** from [Releases](https://github.com/dawoodtogoo2020-code/HelixSR-GPU-Lab/releases). Keep the extracted folder intact.
2. On the PS5, close games and run the usual jailbreak, Payload Manager and ftpsrv. Setup only copies files; it does not inject a kernel payload.
3. Double-click **setup-kstuff-lite.bat** on Windows, or run **sh setup-kstuff-lite.sh** on Mac/Linux. Enter the PS5's IP address.
4. Setup finds the matching original Lite and Test5 A53 reference in Payload Manager. If a file is absent, it asks you to drag the original ELF into the terminal or paste its full path. It checks complete hashes automatically and stops on different builds.
5. After **Installed and verified**, restart normally. Run the usual jailbreak, load **ShadowMount**, then select **Kstuff-Lite-v1.11-A53-11.60.elf** in Payload Manager's Local list **once**. Open GPU Lab and launch SAROS normally.

Use the combined entry instead of plain Lite, full Test5 and the earlier A53 bridge. Do not add it to an already configured K-Stuff boot. Setup preserves existing payloads, favorites and autoload settings. It journals its own file changes, and GPU Lab's normal `uninstall` removes them or restores anything it replaced.

Windows uses GPU Lab's existing Python launcher, including its pinned official Python download when needed. Mac/Linux require Python 3.10+. The setup and binary integration use only Python's standard library; users do not need an SDK, compiler or Unicorn.

Optional command-line inputs:

```sh
python3 tools/gpulab.py setup-lite --ps5 YOUR_PS5_IP --test5 /path/to/Test5.elf --lite /path/to/Lite-v1.11.elf
```

If you do not already have the matching Test5 reference, obtain that exact build from its maintainer, [Drakmor](https://github.com/drakmor), before setup. The download does not bundle the reference or a prebuilt combined ELF. Setup does not substitute another similarly named version or download an unverified build.

## Supported input files

| Input | Bytes | SHA-256 |
| --- | ---: | --- |
| `FPKG-11.60-Kstuff-test5.elf` | 1,802,280 | `829b45fe871dd64fbd53874ae4558076c2f1e1f203fe1ab52db5510d5fc45923` |
| Original `kstuff-lite_v1.11.elf` | 1,737,080 | `ab9a6cb4d3b1daf139d4d646e402b1cf569071acd64599c936d7a3a6164dc779` |

The combined output is **3,592,568 bytes**, SHA-256 **`d02d0cd18d422ad63af0e573a040f8a26284320ce6a6f259bd230f8383de40a7`**. Setup verifies this hash locally and after transfer. It keeps the final ELF and receipt in `my-ps5/<console>/lite1160/`, and creates the console entry `local/gpulab-kstuff-lite-a53-1160`.

## How the integration works

The [binary linker](../tools/lite1160/integrate.py) preserves Lite's original main, kernel image and load-segment contents except for its CRT's main-call hook. The hook invokes the embedded A53 runtime, checks its result, then invokes the original Lite main only on success. The embedded runtime retains the exact Test5 A53 initializer and its private SDK state. Its original K-Stuff main is removed and its entire 434,088-byte embedded Test5 kernel image is zeroed.

The module is placed at a separate virtual offset. Additional ELF load segments and 1,475 loader relocations establish its private namespace; both original dynamic tables remain intact. Its runtime returns status to Lite instead of terminating the process or finalizing Lite's shared dependencies. No second ELF is injected, no child process is created, and the integration makes no Payload Manager load request. The outer Lite runtime owns process exit.

This is reproducible binary integration using owner-supplied files. It does not rebuild the reference's A53 implementation from source. The integrated helper refuses firmware outside 11.60 and stops before Lite's kernel when A53 initialization fails.

## Verification and limits

Offline qualification passed the original 24 firmware/result cases plus linker checks for both images' relocation targets, original Lite preservation, embedded Test5 kernel erasure, wrapper result/argument propagation, stack/register preservation and nested-runtime return behavior. Installer regressions cover input discovery, hash refusal, running-game refusal, repeat setup, interrupted upload, uninstall and preservation of later user edits.

On 6 November 2026, a fresh jailbreak-only regular PS5 boot on firmware 11.60 loaded ShadowMount and this combined ELF once each, with no separate bridge, plain Lite or full Test5. All 72 A53 PPR words passed readback, GET_MMU was restored, and Native/PLAINTEXT_NOAUTH readback succeeded before the same process continued into Lite. SAROS PPSA07631 01.000.000 reached responsive gameplay through GPU Lab with HelixSR On, closed normally, relaunched into responsive gameplay and closed normally a second time. FTP and Payload Manager remained reachable; the complete mounted native SELF still matched the unchanged effects-on candidate SHA-256 `6797e08fe14354bb21b8c0de39e27b8222eeaf712fa4f61c5f170598c389ef11`. **Rest-mode/resume, other firmware, other games and long-session stability remain unqualified.** This release changes host setup and startup compatibility; it introduces no new game rendering binary or FPS claim. The earlier [separate-bridge result](lite-1160-compat.md) remains a historical control.
