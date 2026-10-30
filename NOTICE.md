# Third-party notices

| Component | Where | Terms |
| --- | --- | --- |
| HelixSR (Lone Wolf) | the HelixSR kernels and plans inside the patches; `tools/helixsr_model_tables.py` (tables from HelixSR 1.8.1) | Lone Wolf's [HelixSR](https://github.com/lonewolf0622/HelixSR). This PS5 port is distributed with the author's permission. |
| NVIDIA DLSS network | not included | You download NVIDIA's `nvngx_dlss.dll` from [NVIDIA's GitHub](https://github.com/NVIDIA/DLSS) under [NVIDIA's license](https://github.com/NVIDIA/DLSS/blob/v310.7.0/LICENSE.txt). The network is built on your PC and never leaves it. The patches ship with zeros where the network goes. |
| Game files (Remedy, Sony/Bluepoint) | not included | Patches hold only HelixSR's changes and new code. Everything else comes from your own copy of the game. |
| GPU Lab app (`PPSA99272`), launch/inventory payloads, SHADOW OF THE COLOSSUS GNM hook | release zip | Built with [PS5_Vulkan](https://github.com/mihawk-99/PS5_Vulkan) and the [PS5 payload SDK](https://github.com/ps5-payload-dev/sdk) (GPL-3.0). The corresponding source is the `-source.zip` attached to the same release, under GPL-3.0-or-later. |
| `sce_module/libc.prx` | release zip | PS5_Vulkan's generated clean-room runtime module (GPL-3.0-or-later). Its source and rebuild instructions are in PS5_Vulkan. |
| Mesa RADV / ACO | inside the GPU Lab app and the shader builds | MIT. See [Mesa](https://docs.mesa3d.org/license.html). |
| Python (Windows, only when you have none) | downloaded by `tools/get_python.ps1` from python.org | [PSF License](https://docs.python.org/3/license.html) |

Original tools and documentation in this repository are MIT licensed ([LICENSE](LICENSE)). The MIT license does not cover the components above.
