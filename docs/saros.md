# SAROS: shadows and fog on, 60 FPS target

SAROS **PPSA07631 01.000.000** on a **regular PS5, firmware 11.60** uses HelixSR Model E at **2560×1440**, followed by scaling to **3840×2160**. This is 1440p neural reconstruction presented at 4K. **Dynamic shadows and volumetric fog remain on. There is no TSR fallback.** Stock dynamic-resolution settings remain unchanged: 41.666% minimum, 60% maximum, 16.67 ms budget. Probes observed 1600×900 and 2304×1296 inputs; the game selects its input each frame.

## Separate-translucency effects correction (v0.3.6)

The prior adapter could omit laser beams, melee-breakable barriers and other effects rendered into separate translucency. The native pre-upscale compositor was leaving their colour for TSR to blend, but the HelixSR replacement did not run TSR's blending shader. The correction uses the game's existing compositor to blend colour and modulation before reconstruction. Its output remains FP16 RGBA; depth-aware handling stays in the native compositor. HelixSR recognises the exact resulting producer name without relaxing its other input checks.

The combined corrected executable SHA-256 is `68917b032b806babb10d3acc162afcd5a91b36fab5392281c38bc92e0553c44d`; its matching launch payload is `ec46352533230bdbb229f8404a7db5b537094bdb584ee480127de53944c3730f`. It includes the DOF crash fix below. CPU checks execute the original composition branches for colour-only and modulated inputs and verify the FP16 patch. A GPU Lab ON launch reached the same saved laser doorway used for an OFF reference: the gold laser grid, its bloom and floating ember/spark particles were visible. A read-only observer verified all four patched runtime ranges and confirmed composed translucency reached HelixSR, with primary and quarter outputs published and no stock TSR calls or withheld outputs. The specific R1-breakable wall was not encountered during this bounded check. The [effects audit](saros-effects-audit.json) distinguishes native pass preservation from visual qualification; it does not claim every particle asset or scene has been checked.

## DOF crash fix (v0.3.6, 6 November 2026)

The effects-on executable previously stopped with SIGILL and diagnostic `e4410003` when the game supplied `DOF.Recombine` as its scene-colour producer. The crash registers identify title-relative name `0x72f8ca0`, active size 1652×929, a 2304×1296 allocation and the expected FP16 colour format. Its name was absent from the adapter's two-name allowlist. The fix recognises that exact producer while retaining the original format, resource, allocation and rectangle checks. Six tests execute the production x86 guard, covering the recorded contract and invalid-input refusals. The Model E asset, GPU shaders and visual settings are unchanged.

The initial DOF-only trial executable SHA-256 was `7f98835d660fd693b3bb4a68d2d40a621d0434e2450b1d24229bf8328fa1039d`; its launcher SHA-256 was `b6f4e4cdf6c94004cc39dba14b83ca23401af413bc77f520cc8538eb865604ea`. That trial was verified on the developer's console before the combined effects candidate above. The first session closed normally after 21,543 published frames, including 1,986 DOF frames, with zero withheld outputs and zero stock TSR calls. A subsequent GPU Lab ON launch accepted the corrected identity, reached gameplay and produced 7,583 published frames with the same zero-error counters before normal closure. Live readers verified the owner title, hook/callsite bytes, anchors and unchanged model. Host checks passed all 108 Python tests (including the six native-guard tests) and 107 native switch cases. This is bounded validation, not an all-area stability claim. See the [verification record](saros-dof-evidence.json). The measurements below describe the earlier effects-on executable.

## Measurements and scope

A clean 90-second gameplay capture using **ps5-exporter v0.3.1's display-controller flip counter** averaged **59.933 FPS**. All **90 samples were 59–60 FPS**, with the SAROS title, running state and FPS collector valid throughout. A separate stationary capture averaged 59.95 FPS over 60 valid samples. These are once-per-second counter readings, not per-frame frametimes, HDMI capture or a guarantee for every scene.

Three native traces during camera movement, walking and a ledge-return sequence recorded no missing callback ticks, zero stock TSR calls and zero withheld outputs. The final trace covered 1,662 consecutive intervals: mean 16.686 ms, P99 17.599 ms, maximum 18.039 ms. Callback timing uses PS5 TSC calibrated against PS5 CLOCK_MONOTONIC; it does not measure GPU duration. A separate mixed capture included Remote Play reconnection and a pause menu and is excluded from the headline gameplay result.

Read-only probes verified ShadowQuality=5, virtual shadows=1, general fog=1, volumetric fog=1 and fog grid size=10. Native full-frame scaler readbacks passed independent FP16 reference comparisons with one packed-channel code-step tolerance: 8,294,400 primary pixels, 518,400 quarter-output pixels, and 8,294,400 larger-input primary pixels. No invalid/non-finite pixels were found. These establish scaler correctness in captured frames, not neural fidelity or comprehensive temporal image quality.

Coverage is limited to the tested room, camera motion, walking, startup and a ledge-return/postprocess sequence. Combat, other areas and full death/respawn coverage remain unqualified. Unsupported render contracts stop execution rather than invoking TSR. Earlier network outages remain unexplained, including one before SAROS launched; the successful run remained reachable through normal game closure. GPU Lab controller OFF and ON launches passed: full mounted hashes matched stock and the effects-on candidate. Both reached gameplay and closed normally. The fresh ON launch then averaged 59.917 FPS over 60 valid display-counter samples (59–60 FPS); a 1,661-interval native trace again recorded zero TSR calls, withheld outputs or tick gaps.

## Install and use

**Before launching after a reboot, follow [the tested firmware 11.60 startup sequence](SAROS-START-HERE.md).** The current tested route uses ShadowMount+ 1.7-beta5fix1 followed by the bundled combined Lite + A53 payload. Plain K-Stuff Lite v1.11 alone is not the verified boot setup. GPU Lab's package Ready state does not certify kernel-payload readiness.

Download **HelixSR-GPU-Lab-v0.3.6-saros-dof-beta.zip** from [Releases](https://github.com/dawoodtogoo2020-code/HelixSR-GPU-Lab/releases). With Python 3.10+, ftpsrv, pldmgr and ShadowMount+ running:

```sh
python tools/gpulab.py install --game saros-native-v1
```

Follow the installer prompts, including NVIDIA's license decision. It reads the exact executable and a bounded shader range from your own game, reconstructs the package locally, and checks every hash. No game executable or NVIDIA weights ship.

In GPU Lab, select SAROS, press **Cross** to enter details and **Cross** on HelixSR to choose ON/OFF. **Triangle** applies the choice and launches. OFF restores the original executable. **Square** opens imported callback logs; their timing basis is labelled. There is no built-in SAROS FPS HUD or arbitrary resolution override.

For an earlier v0.3.0/v27 installation, use that release's installer and retained `my-ps5` journal to uninstall before installing this release. `--adopt` accepts only the exact matching effects-on package; it cannot upgrade an older trial. Preserve installer journals needed for rollback.

## Upgrade from an earlier release

The DOF and effects corrections change both the SAROS executable recipe and the regular-PS5 launch payload. An older launcher cannot enable the corrected package. Download the complete v0.3.6 ZIP; do not mix its files with an older release.

1. Keep your old extracted release folder and its `my-ps5` journal/backups. With games and apps closed, run that folder's `uninstall.bat` or `sh uninstall.sh` and let it finish restoring the previous installation.
2. Extract v0.3.6 into a new folder and run `install.bat` or `sh install.sh`. Select SAROS and any other integrations you want reinstalled. The installer reconstructs and verifies the corrected game file from your own stock copy.
3. Open GPU Lab, select SAROS, set HelixSR On and press Triangle. Follow the startup recipe after a full reboot.

`--adopt` accepts only the exact corrected executable and sidecar. It cannot convert an older effects-on package into this fix. Preserve the old journal until uninstall succeeds; retain the new journal for future uninstall. Game saves are not part of these installation paths.

## Earlier effects-on identities (v0.3.1–v0.3.5)

HXD recipes reconstruct the exact tested executable and asset from owner-provided inputs and locally generated weights. Wrong executable, shader archive and model weights are rejected. See [native source](../native/saros/README.md) and [GPU Lab build instructions](../native/gpulab/README.md).

| Component | SHA-256 |
| --- | --- |
| SAROS patched SELF | `6797e08fe14354bb21b8c0de39e27b8222eeaf712fa4f61c5f170598c389ef11` |
| Model asset | `1be3a14f7f25ea298929b485ad0cdf5e17441cd5ea94cb85f22a4f8500c9eb8e` |
| GPU Lab app | `bf874cddfee0fc71d8cf8e89ced003e891f705d38abc5740151dc9c74095a80c` |
| Launch payload | `925184fee2a3f3daa53a1512f8213f35e4ac694085c583ca5d6e3cfebf9eb3c9` |
