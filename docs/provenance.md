# Provenance and distribution boundaries

## What ships

| Item | Origin |
| --- | --- |
| Installer, patch tool, weights builder, console access, tests, docs | Written for this project (MIT) |
| HXD1 patches (`games/*/patches`, release zip) | HelixSR's changes to each game file. They hold HelixSR kernels, plans, math tables and our own code, plus copy instructions into your own game files. The NVIDIA network range ships as zeros. |
| `tools/helixsr_model_tables.py` | Tables from Lone Wolf's HelixSR 1.8.1: where each network tensor sits in NVIDIA's DLL, and the layout the kernels read. Distributed with the author's permission. It holds no NVIDIA data. |
| GPU Lab app, launch/inventory payloads, SHADOW OF THE COLOSSUS GNM hook (release zip) | Built from this project's sources with PS5_Vulkan, the PS5 payload SDK and Mesa. GPL-3.0 source zip attached to the release. |
| `libc.prx` (release zip) | PS5_Vulkan's generated clean-room runtime module (GPL-3.0-or-later) |

HelixSR's PS5 ports (the kernels compiled for the PS5 and PS4 GPUs, the plans and the integrations) are distributed with **Lone Wolf's permission**. Upstream: [lonewolf0622/HelixSR](https://github.com/lonewolf0622/HelixSR).

## What never ships

- **NVIDIA's DLSS DLL and anything built from it.** Your PC downloads the DLL from NVIDIA after you accept NVIDIA's license, builds the network and deletes the DLL. The network stays in `my-ps5/`.
- **Game executables, archives, effects or shaders** from Remedy, Sony or Bluepoint. Patches copy those bytes from your own installed game. The one exception: a patch may carry changed bytes where HelixSR modified game code or effects (for example, a call to HelixSR, or a shader variant that gates FSR2).
- **Sony SDK headers or libraries, console keys, game dumps, private recordings, and the lab's console addresses or receipts.**

## How this was checked

- **Patches rebuild the tested files.** Each one was made from stock game files and verified to rebuild, byte for byte, the files that ran on the lab consoles.
- **The network is left out.** Every copy of it inside a package is a hole. No 256-byte fragment of it is found elsewhere.
- **No game shader records ship.** For CONTROL's shader library (585 records), the 9 stock records are copied from your file and the rest are HelixSR's. For Alan Wake 2, a 1.3 KB shader-record header the grafted kernels reuse is copied from your own FSR2 effect file.
- **The installer was tested end to end in simulation.** It ran against a simulated PS5 loaded with stock files: install, ON/OFF, a second install, and an uninstall that restored the console byte for byte.

## Upstream references

- [Lone Wolf's HelixSR](https://github.com/lonewolf0622/HelixSR): the upstream PC upscaler
- [PS5 payload SDK](https://github.com/ps5-payload-dev/sdk), [PS5_Vulkan](https://github.com/mihawk-99/PS5_Vulkan): toolchains for the console binaries
- [NVIDIA DLSS](https://github.com/NVIDIA/DLSS): source of the network the installer downloads under NVIDIA's license

Each project keeps its own license. Naming a project does not imply affiliation or endorsement.
