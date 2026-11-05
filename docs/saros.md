# SAROS: HelixSR v27 performance preset

SAROS **PPSA07631 01.000.000** on a **regular PS5, firmware 11.60** reconstructs a full **3840×2160** output using HelixSR Model E. GPU Lab's controller ON/OFF switch and Triangle launch were tested in both modes, with the actual mounted executable checked byte for byte. OFF restores the original executable and visual settings.

This is reconstructed 4K, with dynamic input commonly **1536×864**. The performance preset disables **dynamic shadows and volumetric fog**, and sets the minimum dynamic render percentage to 40% for that launch. General fog, SSGI, ambient occlusion and depth of field keep their default values. The changes do not modify saved settings. The lighting is visibly different from stock.

## Measurements and scope

Four calibrated PS5 CPU render-callback captures reached the nominal **59.94 Hz** cadence:

| Capture | Consecutive intervals | Mean FPS | P99 interval | Maximum |
| --- | ---: | ---: | ---: | ---: |
| Stationary | 1,662 | 59.933 | 17.57 ms | 17.97 ms |
| Camera / movement 1 | 1,660 | 59.934 | 17.82 ms | 18.56 ms |
| Camera / movement 2 | 1,662 | 59.935 | 17.71 ms | 18.36 ms |
| Fresh GPU Lab launch, settled gameplay | 1,663 | 59.935 | 17.53 ms | 17.81 ms |

All four traces had no missing callback ticks, stock TSR calls or withheld publications, and used 1536×864 input with 3840×2160 output. A separate startup capture includes loading pauses and resolution changes; it is not counted as steady gameplay. Timing uses PS5 TSC calibrated against PS5 CLOCK_MONOTONIC. It does **not** measure GPU duration, HDMI refresh delivery, Remote Play/network FPS or every part of the game. Combat, every area, and comprehensive temporal image quality remain outside this qualification.

Native full-model comparison validated the qualified scheduling changes, and full-output readback validated all 8,294,400 output pixels against the format conversion reference. Walking, camera movement, a fall/recovery and cold launch were visually exercised. Supported frames bypass the stock TSR call entirely. Unsupported shapes retain stock fallback, with a history reset before returning to HelixSR; a separate native fallback/re-entry test verified that path. Input widths below 1536 are not enabled. The private feature arena is 464,790,784 bytes (about 443 MiB), plus a 1,923,840-byte parameter buffer and engine output/history resources.

GPU Lab's frame-rate screen displays imported, verified callback logs and labels their timing basis. These logs are examples from the developer console; the SAROS build does not provide an in-game FPS HUD or a new live recorder. See the `examples/SAROS-v27-*-frame-log.txt` files in the download.

## Install and use

Download and extract **HelixSR-GPU-Lab-v0.3.0-saros-beta.zip** from [Releases](https://github.com/dawoodtogoo2020-code/HelixSR-GPU-Lab/releases). From its directory:

```sh
python tools/gpulab.py install --game saros-native-v1
```

Use Python 3.10 or newer. Have ftpsrv, pldmgr and ShadowMount+ running. Follow the existing installer prompts, including NVIDIA's license decision. The installer reads the exact native executable and a bounded shader-archive range from your own running game, reconstructs the patched executable and sidecar on your PC, checks their hashes, then asks you to close applications before installation. A different title version or input is refused.

Open GPU Lab, select SAROS, press **Cross** to enter its settings, then **Cross** on HelixSR to choose ON/OFF. **Triangle** applies the selection and launches the game. **Square** opens available frame logs. Resolution examples describe observed dynamic inputs; this release does not offer an arbitrary resolution override.

For an existing matching v27 installation, `install --game saros-native-v1 --adopt` verifies its executable and sidecar, updates the app/metadata and records the existing mode. It does not accept an older SAROS trial. Preserve the installer's `my-ps5` directory for rollback. The existing `on`, `off`, `status` and `uninstall` commands remain available. Adoption uninstall restores the adopted mode and any app/metadata it replaced; it does not delete pre-existing game files.

## Reproduction and identities

The release contains only patches, application components and source; it contains no retail game executable, private native capture or NVIDIA model weights. HXD patches rebuild from your files, with a model-weight hole filled locally. The native executable patch is 7,210 bytes; the sidecar patch is 80,997 bytes. Both reconstructed results were compared with the installed, tested files.

| Component | SHA-256 |
| --- | --- |
| SAROS patched SELF | `68a69da47a55e10aa524f778e2bfd6a9a4fa83ecdc73c97f29b7f64cdd0c6c22` |
| Model sidecar | `e37aefda5023a8e782df15dd4d0a7e0248b647d7acb6d8ecd34fe3a69f91edfd` |
| GPU Lab app | `36bb48d73d5fb2d88b6193fd58635d608e281fd4b854b5cbf6a28904e49d9d61` |
| Launch payload | `3b462f3124ba2a84d5a4541051acb5ec447d908cfffdf2f9bd2de758fe34f297` |

See [native SAROS source](../native/saros/README.md) and [GPU Lab build instructions](../native/gpulab/README.md). The delivered portable build scripts reproduced the qualified hook, app and payload hashes. Reproducibility with the pinned dependencies is distinct from a fresh toolchain bootstrap or testing another console.
