# SAROS startup on the tested PS5 11.60 setup

Updated 6 November 2026 for v0.3.3-lite-a53-beta. The SAROS game and app binaries remain the v0.3.1 effects-on build.

## One-time Lite setup on firmware 11.60

Run **setup-kstuff-lite.bat** on Windows, or **sh setup-kstuff-lite.sh** on Mac/Linux, from the extracted download. Enter the PS5 IP with Payload Manager and ftpsrv running and games closed. The release ZIP includes the tested combined payload. Setup verifies its hash and installs it as one Local entry; users do not need Test5, a separate Lite file or a build step. See [the setup guide](lite-1160-integrated.md) for the manual-copy alternative.

## After every full restart — integrated Lite recipe

1. Run your existing YouTube jailbreak and wait for it to finish. Open Payload Manager.
2. Run **ShadowMount+ 1.7-beta5fix1** once. Wait for its startup scan to finish.
3. In **Local**, run **Kstuff-Lite-v1.11-A53-11.60.elf** once. This includes the A53 fix and Lite in one payload; no separate bridge is needed.
4. Wait for initialization to complete. The verified kernel-log messages include `READMEM applied and read back 72 PPR words; GET_MMU restored` and `dynamic Native/PLAINTEXT_NOAUTH patch installed and read back by READMEM`. If initialization fails, stop before launching the game.
5. Open **HelixSR GPU Lab**, select **SAROS**, and check that **HelixSR is On**. If needed, press **Cross** to enter details, then **Cross** on HelixSR to change it.
6. Press **Triangle once**. GPU Lab closes, applies your selection and starts SAROS. Let the launch card finish; proceed through the game's opening screens.

Use this combined entry instead of plain Lite, full Test5 or the earlier bridge. Start with other K-Stuff autoload entries disabled. Do not mix boot recipes or load a second K-Stuff into the same session. Rest-mode/resume remains unqualified.

The previously tested **full Test5 alternative** is still available: from a fresh jailbreak load ShadowMount, then the exact `FPKG-11.60-Kstuff-test5.elf` once, then launch from GPU Lab. That alternative uses neither Lite nor the bridge nor the combined entry.

You do not need to reinstall HelixSR after each reboot. The game package stays installed, but the boot payloads must be started again. FTP is needed for installation and diagnostics; it is not a substitute for the boot payloads.

## If you already loaded Lite or another K-Stuff

Close applications and restart the console normally, then follow the sequence above from a fresh jailbreak. Do not try to replace a running K-Stuff by loading a second one. GPU Lab's green **Ready** state describes the installed HelixSR package; it does **not** certify that the correct kernel payload is active.

If SAROS still fails at the launch card, record the error and collect `/data/helixsr-speed/launch-handoff.txt` and the kernel log for that boot. Check the firmware, SAROS version and payload identities below before changing game files. Do not repeatedly inject payloads into a failing session.

## Exact tested scope and payloads

Regular PS5, firmware **11.60**, SAROS **PPSA07631 01.000.000**, HelixSR **v0.3.1-saros-effects-beta** game binaries, with **v0.3.3-lite-a53-beta** host setup. These instructions are not a compatibility claim for other firmware or payload revisions.

| Payload | SHA-256 |
| --- | --- |
| `ShadowMountPlus_1.7-beta5fix1.elf` | `e85fd63b705498488f39c09c9aea6f9fa22e0df2e932865de49a8cedf25172d5` |
| `Kstuff-Lite-v1.11-A53-11.60.elf` | `d02d0cd18d422ad63af0e573a040f8a26284320ce6a6f259bd230f8383de40a7` |
| Full Test5 alternative: `FPKG-11.60-Kstuff-test5.elf` | `829b45fe871dd64fbd53874ae4558076c2f1e1f203fe1ab52db5510d5fc45923` |

The local K-Stuff metadata identifies the tested file as a mirrored **Drakmor 1.13-fpkg-dr-test5** build. Payload names alone are insufficient: verify the SHA-256. The combined Lite + A53 payload is included in the full v0.3.3 release ZIP. ShadowMount and the full Test5 alternative are not included. Only users choosing the full Test5 alternative need to obtain that matching build from its maintainer; it is not needed for the integrated Lite recipe.

## What was checked

On 6 November, a fresh jailbreak-only boot was checked before loading one copy of each payload above. K-Stuff reported successful patch installation/readback. GPU Lab's normal Triangle launch with HelixSR On passed the launch card and opening sequence and reached interactive gameplay. Camera and movement input responded. The full mounted executable matched the released effects-on candidate SHA-256 `6797e08fe14354bb21b8c0de39e27b8222eeaf712fa4f61c5f170598c389ef11`. SAROS then closed normally, and FTP and the payload manager remained reachable.

This establishes a working startup sequence. A later [fresh-boot Lite v1.11 comparison](audit-2026-11-06.md) stalled at the SAROS splash screen with PPR/mount and storage I/O errors before gameplay. Only Lite was loaded on that comparison boot. A subsequent fresh boot with the separate A53-only bridge and the same original Lite file passed gameplay, a relaunch and two normal closes; see the [bridge qualification](lite-1160-compat.md). The previous release instructions omitted the exact tested boot dependency. These instructions correct that omission; they do not introduce a new upscaler binary or a new FPS benchmark. Shadows and volumetric fog remain enabled, stock dynamic resolution remains unchanged, the target remains 60 FPS, and there is no TSR fallback.

The integrated Lite + A53 route was then qualified on another fresh boot with ShadowMount followed by the combined ELF only. It passed all 72 A53 patch-word readbacks, responsive SAROS gameplay, relaunch and two normal closes, with the same mounted game hash. [Integrated setup and limits](lite-1160-integrated.md).
