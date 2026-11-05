# SAROS: shadows and fog on, 60 FPS target

SAROS **PPSA07631 01.000.000** on a **regular PS5, firmware 11.60** uses HelixSR Model E at **2560×1440**, followed by scaling to **3840×2160**. This is 1440p neural reconstruction presented at 4K. **Dynamic shadows and volumetric fog remain on. There is no TSR fallback.** Stock dynamic-resolution settings remain unchanged: 41.666% minimum, 60% maximum, 16.67 ms budget. Probes observed 1600×900 and 2304×1296 inputs; the game selects its input each frame.

## Measurements and scope

A clean 90-second gameplay capture using **ps5-exporter v0.3.1's display-controller flip counter** averaged **59.933 FPS**. All **90 samples were 59–60 FPS**, with the SAROS title, running state and FPS collector valid throughout. A separate stationary capture averaged 59.95 FPS over 60 valid samples. These are once-per-second counter readings, not per-frame frametimes, HDMI capture or a guarantee for every scene.

Three native traces during camera movement, walking and a ledge-return sequence recorded no missing callback ticks, zero stock TSR calls and zero withheld outputs. The final trace covered 1,662 consecutive intervals: mean 16.686 ms, P99 17.599 ms, maximum 18.039 ms. Callback timing uses PS5 TSC calibrated against PS5 CLOCK_MONOTONIC; it does not measure GPU duration. A separate mixed capture included Remote Play reconnection and a pause menu and is excluded from the headline gameplay result.

Read-only probes verified ShadowQuality=5, virtual shadows=1, general fog=1, volumetric fog=1 and fog grid size=10. Native full-frame scaler readbacks passed independent FP16 reference comparisons with one packed-channel code-step tolerance: 8,294,400 primary pixels, 518,400 quarter-output pixels, and 8,294,400 larger-input primary pixels. No invalid/non-finite pixels were found. These establish scaler correctness in captured frames, not neural fidelity or comprehensive temporal image quality.

Coverage is limited to the tested room, camera motion, walking, startup and a ledge-return/postprocess sequence. Combat, other areas and full death/respawn coverage remain unqualified. Unsupported render contracts stop execution rather than invoking TSR. Earlier network outages remain unexplained, including one before SAROS launched; the successful run remained reachable through normal game closure. GPU Lab controller OFF and ON launches passed: full mounted hashes matched stock and the effects-on candidate. Both reached gameplay and closed normally. The fresh ON launch then averaged 59.917 FPS over 60 valid display-counter samples (59–60 FPS); a 1,661-interval native trace again recorded zero TSR calls, withheld outputs or tick gaps.

## Install and use

Download **HelixSR-GPU-Lab-v0.3.1-saros-effects-beta.zip** from [Releases](https://github.com/dawoodtogoo2020-code/HelixSR-GPU-Lab/releases). With Python 3.10+, ftpsrv, pldmgr and ShadowMount+ running:

```sh
python tools/gpulab.py install --game saros-native-v1
```

Follow the installer prompts, including NVIDIA's license decision. It reads the exact executable and a bounded shader range from your own game, reconstructs the package locally, and checks every hash. No game executable or NVIDIA weights ship.

In GPU Lab, select SAROS, press **Cross** to enter details and **Cross** on HelixSR to choose ON/OFF. **Triangle** applies the choice and launches. OFF restores the original executable. **Square** opens imported callback logs; their timing basis is labelled. There is no built-in SAROS FPS HUD or arbitrary resolution override.

For an earlier v0.3.0/v27 installation, use that release's installer and retained `my-ps5` journal to uninstall before installing this release. `--adopt` accepts only the exact matching effects-on package; it cannot upgrade an older trial. Preserve installer journals needed for rollback.

## Reproduction and identities

HXD recipes reconstruct the exact tested executable and asset from owner-provided inputs and locally generated weights. Wrong executable, shader archive and model weights are rejected. See [native source](../native/saros/README.md) and [GPU Lab build instructions](../native/gpulab/README.md).

| Component | SHA-256 |
| --- | --- |
| SAROS patched SELF | `6797e08fe14354bb21b8c0de39e27b8222eeaf712fa4f61c5f170598c389ef11` |
| Model asset | `1be3a14f7f25ea298929b485ad0cdf5e17441cd5ea94cb85f22a4f8500c9eb8e` |
| GPU Lab app | `bf874cddfee0fc71d8cf8e89ced003e891f705d38abc5740151dc9c74095a80c` |
| Launch payload | `925184fee2a3f3daa53a1512f8213f35e4ac694085c583ca5d6e3cfebf9eb3c9` |
