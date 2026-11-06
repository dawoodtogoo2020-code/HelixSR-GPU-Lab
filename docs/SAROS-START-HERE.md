# SAROS startup on the tested PS5 11.60 setup

Updated 6 November 2026 for v0.3.1-saros-effects-beta.

A separate [Lite v1.11 compatibility route](lite-1160-compat.md) has now passed gameplay, relaunch and normal close on the same firmware. It requires the exact local A53-only bridge before ShadowMount and original Lite. Choose one boot recipe and use it from a fresh jailbreak.

## After every full restart — Test5 recipe

1. Run your existing YouTube jailbreak and wait for it to finish. Open Payload Manager.
2. Run **ShadowMount+ 1.7-beta5fix1** once. Wait for its startup notification before continuing.
3. In Payload Manager's **Local** payloads, run **FPKG-11.60-Kstuff-test5.elf** once. This is the firmware-specific build used for this release's tests. **This recipe uses Test5. For Lite, use the separate bridge recipe; do not load both K-Stuff builds on the same boot.**
4. Wait for K-Stuff initialization to complete. The verified kernel-log message is `dynamic Native/PLAINTEXT_NOAUTH patch installed and read back by READMEM`. If initialization fails, stop before launching the game.
5. Open **HelixSR GPU Lab**, select **SAROS**, and check that **HelixSR is On**. If needed, press **Cross** to enter details, then **Cross** on HelixSR to change it.
6. Press **Triangle once**. GPU Lab closes, applies your selection and starts SAROS. Let the launch card finish; proceed through the game's opening screens.

You do not need to reinstall HelixSR after each reboot. The game package stays installed, but the boot payloads must be started again. FTP is needed for installation and diagnostics; it is not a substitute for the boot payloads.

## If you already loaded Lite or another K-Stuff

Close applications and restart the console normally, then follow the sequence above from a fresh jailbreak. Do not try to replace a running K-Stuff by loading a second one. GPU Lab's green **Ready** state describes the installed HelixSR package; it does **not** certify that the correct kernel payload is active.

If SAROS still fails at the launch card, record the error and collect `/data/helixsr-speed/launch-handoff.txt` and the kernel log for that boot. Check the firmware, SAROS version and payload identities below before changing game files. Do not repeatedly inject payloads into a failing session.

## Exact tested scope and payloads

Regular PS5, firmware **11.60**, SAROS **PPSA07631 01.000.000**, HelixSR **v0.3.1-saros-effects-beta**. These instructions are not a compatibility claim for other firmware or payload revisions.

| Payload | SHA-256 |
| --- | --- |
| `ShadowMountPlus_1.7-beta5fix1.elf` | `e85fd63b705498488f39c09c9aea6f9fa22e0df2e932865de49a8cedf25172d5` |
| `FPKG-11.60-Kstuff-test5.elf` | `829b45fe871dd64fbd53874ae4558076c2f1e1f203fe1ab52db5510d5fc45923` |

The local K-Stuff metadata identifies the tested file as a mirrored **Drakmor 1.13-fpkg-dr-test5** build. Payload names alone are insufficient: verify the SHA-256. These third-party payloads are not included in the HelixSR archive. If the matching build is not already available in your setup, obtain it from its maintainer and verify its identity before following this procedure; a generic K-Stuff download is not a verified replacement.

## What was checked

On 6 November, a fresh jailbreak-only boot was checked before loading one copy of each payload above. K-Stuff reported successful patch installation/readback. GPU Lab's normal Triangle launch with HelixSR On passed the launch card and opening sequence and reached interactive gameplay. Camera and movement input responded. The full mounted executable matched the released effects-on candidate SHA-256 `6797e08fe14354bb21b8c0de39e27b8222eeaf712fa4f61c5f170598c389ef11`. SAROS then closed normally, and FTP and the payload manager remained reachable.

This establishes a working startup sequence. A later [fresh-boot Lite v1.11 comparison](audit-2026-11-06.md) stalled at the SAROS splash screen with PPR/mount and storage I/O errors before gameplay. Only Lite was loaded on that comparison boot. A subsequent fresh boot with the separate A53-only bridge and the same original Lite file passed gameplay, a relaunch and two normal closes; see the [bridge qualification](lite-1160-compat.md). The previous release instructions omitted the exact tested boot dependency. These instructions correct that omission; they do not introduce a new upscaler binary or a new FPS benchmark. Shadows and volumetric fog remain enabled, stock dynamic resolution remains unchanged, the target remains 60 FPS, and there is no TSR fallback.
