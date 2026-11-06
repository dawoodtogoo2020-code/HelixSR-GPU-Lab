# HelixSR GPU Lab

**Neural upscaling for PS5 games: Lone Wolf's HelixSR running inside SAROS, CONTROL Resonant, Alan Wake 2 and SHADOW OF THE COLOSSUS.**

[Download](https://github.com/dawoodtogoo2020-code/HelixSR-GPU-Lab/releases) · [Install guide](docs/install-ps5.md) · [Compatibility](docs/compatibility.md) · [Results](docs/validation.md) · [Project website](https://dawoodtogoo2020-code.github.io/HelixSR-GPU-Lab/)

[Join Blackheart 909 Gaming on Discord](https://discord.gg/k4UWq8DqK) · [Support my work on Ko-fi](https://ko-fi.com/blackheart909)

> **Public beta (v0.3.5-lite-a53-beta).** The HelixSR builds it installs ran on the developer's own regular PS5 and PS5 Pro. SAROS effects-on gameplay was tested on a regular PS5, firmware 11.60. Other consoles and areas remain untested. Everything it does is checked and can be undone with `uninstall`. Please [report your results](docs/install-ps5.md#help).

## Install in three steps

1. **Download** `HelixSR-GPU-Lab-v0.3.5-lite-a53-beta.zip` from [Releases](https://github.com/dawoodtogoo2020-code/HelixSR-GPU-Lab/releases) and unzip it on your PC.
2. **Run** `install.bat` (Windows: double-click) or `sh install.sh` (Mac / Linux).
3. **Answer its questions:**
   - your PS5's IP address
   - whether it is a PS5 Pro
   - **yes** to NVIDIA's DLSS license
   - then start, and later close, the game when it asks

For SAROS only, run `python tools/gpulab.py install --game saros-native-v1`.

**One installer includes Lite + A53 on regular PS5 firmware 11.60.** The installer detects firmware automatically from the console. On a regular PS5 running **11.60**, it installs GPU Lab, the selected game integrations and the bundled combined Local payload in the same run. There is no firmware-entry prompt. No second installer, Test5 download or build is needed. After restarting: jailbreak → ShadowMount → the combined payload once → GPU Lab. PS5 Pro and other firmware keep their existing startup setup. The separate `setup-kstuff-lite.*` launchers remain available for payload-only installation. [Details](docs/lite-1160-integrated.md). Game/app and combined-payload binaries are unchanged.

Then open **HelixSR GPU Lab** on the PS5. **Cross** switches HelixSR on or off, and **Triangle** starts the game.

**You need** a jailbroken PS5 running ftpsrv (port 2121), pldmgr (port 8084) and ShadowMount+, plus one of these games at exactly this version:

| Game | Version | Console | HelixSR |
| --- | --- | --- | --- |
| SAROS (PPSA07631) | 01.000.000 | regular PS5 | Stock dynamic input → 1440p reconstruction → 4K; measured 59.93 FPS, shadows and volumetric fog on ([details](docs/saros.md)) |
| CONTROL Resonant (PPSA34547) | 01.300.300 | regular PS5 | Performance 1505×847 → 1440p (60 FPS target) · Quality 2258×1270 → 4K (30 FPS) |
| Alan Wake 2 (PPSA02572) | 01.200.007 | regular PS5 | Performance 1505×847 → 1440p, 60 FPS (mean 59.95, minimum 59) |
| SHADOW OF THE COLOSSUS (CUSA08809) | 01.00 | PS5 Pro | Cinematic 1440p → 4K, about 38 FPS |

All the details are in the [install guide](docs/install-ps5.md): what each question means, ON/OFF, uninstall and help.

## How it works without shipping anyone's files

- **No game files ship.** Each game's HelixSR files are built on your PC from your own copy of the game, which the installer reads from your PS5 while the game runs. The download holds only small patches.
- **No NVIDIA files ship.** HelixSR's network comes from NVIDIA's DLSS DLL. As in HelixSR's own setup, the installer downloads it from NVIDIA after you accept NVIDIA's license, builds the network on your PC (`tools/helixsr_weights.py`) and deletes the DLL.
- **Everything is checked.** Every built file must match the exact build that was tested, byte for byte. If your game version or a file differs, nothing is written.
- **Everything can be undone.** Each change on the PS5 is journalled on your PC. Anything replaced is backed up first, and `uninstall` restores it.

On the PS5, HelixSR replaces the game's own upscaler:
- **SAROS:** Model E reconstructs at 1440p and scales to 4K, with shadows and fog on, stock dynamic input, and no TSR fallback.
- **CONTROL and Alan Wake 2:** it replaces FSR2, inside the game's own render graph.
- **SHADOW OF THE COLOSSUS** (a PS4 title on the Pro): it hooks the game's GNM graphics driver.

## In this repository

| Path | What |
| --- | --- |
| `install.bat`, `install.sh`, `uninstall.*`, `gpulab.*`, `alan-wake-2-*.bat` | Launchers for the installer |
| `tools/gpulab.py` | The installer: install, status, on/off, uninstall (Python standard library only) |
| `tools/helixsr_weights.py`, `tools/helixsr_model_tables.py` | Builds HelixSR's network on your PC from NVIDIA's DLSS DLL (no NVIDIA data inside) |
| `tools/helixsr_patch.py` | HXD1 patches: your game file in, the tested HelixSR file out |
| `tools/ps5_console.py` | FTP and process-list access to the PS5, with a "nothing running" check before every write |
| `games/*/game.json`, `console/*/console.json` | What is installed where, with the SHA-256 of every tested file |
| `tests/` | Installer tests against a simulated PS5, plus the earlier host tools' tests |
| `docs/` | Install guide, compatibility, results, provenance, roadmap |

The patches, the GPU Lab app and payloads, and the SHADOW OF THE COLOSSUS hook ship in the release zip. Their source code is in the release's source zip.

Earlier host tools are still here: the bundle verifier, the benchmark comparator and the temporal adapter contract (`tools/install_support.py`, `tools/benchmark.py`, `native/`).

Run the tests with `python -m unittest discover -s tests -p 'test_*.py'`.

## Credits and licensing

- **HelixSR** is [Lone Wolf's](https://github.com/lonewolf0622/HelixSR) upscaler, which runs NVIDIA's DLSS network as standard compute shaders. GPU Lab's PS5 ports of HelixSR are distributed with Lone Wolf's permission.
- **The PS5 integration, installer and GPU Lab** are by Dawood (Blackheart909).
- **Licenses:**
  - Original tools and docs here are [MIT licensed](LICENSE).
  - The GPU Lab app, payloads and hook binaries are built with GPL-3.0 toolchains, and their source is published with each release.
  - Third-party terms are listed in [NOTICE.md](NOTICE.md) and [docs/provenance.md](docs/provenance.md).

This is an independent project. It is not affiliated with or endorsed by Sony, NVIDIA, AMD, Remedy, Bluepoint or the upstream HelixSR project. Use it at your own risk, on single-player games only.

