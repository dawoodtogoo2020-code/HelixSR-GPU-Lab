# Compatibility

Last reviewed: 2026-10-30 (v0.2.0-beta).

| Game / build | Console | Public install | What runs | Limits |
| --- | --- | --- | --- | --- |
| CONTROL Resonant / PPSA34547 / 01.300.300 | regular PS5 | **Public beta** | HelixSR r18 replaces FSR2. Performance: 1505×847 → 2560×1440. Quality (Cinematic): 2258×1270 → 3840×2160 | R17 dipped to about 58 FPS in the first heavy pans after loading. R18 has not been frame-timed. A rejected frame has no fresh fallback. Reset after a mode switch is unproven. Never run on a PS5 Pro. |
| Alan Wake 2 / PPSA02572 / 01.200.007 | regular PS5 | **Public beta** | HelixSR t12 replaces FSR2 in Performance (1505×847 → 2560×1440) on the game's own colour, depth and motion. 60 FPS (mean 59.95, minimum 59) | Performance only (other modes run stock FSR2). No sharpening: stock FSR2 looks crisper at rest. No GPU Lab switch yet (use the PC tool). Never run on a PS5 Pro. |
| SHADOW OF THE COLOSSUS / CUSA08809 / 01.00 | PS5 Pro | **Public beta** | HelixSR live3 replaces the game's own upscale in Cinematic (2560×1440 → 3840×2160) through a GNM driver hook | About 38 FPS ON vs 60 OFF. The input is the game's finished image (flat depth, no motion vectors, no jitter). Not run on a regular PS5. |
| SAROS | — | None | Historical static discovery | Installed identity, live FSR path, resource contract and recovery unverified |
| Other PS5 FSR titles | — | None | Future work | Each game and build needs its own adapter |

**Exact versions only.** The installer checks the installed version and every game file it reads. Anything different is refused before the PS5 is changed.

**Consoles tested by the developer:**
- regular PS5 CFI-1016A on firmware 11.60
- PS5 Pro on firmware 13.60

Both run kstuff, ftpsrv, pldmgr and ShadowMount+. Other firmware versions and homebrew setups are untested. Please report what you find.

## Why each game needs its own build

An FSR label does not identify a universal replaceable interface on PS5. Each integration has to:
- find the game's actual frame resources and their formats
- follow the game's motion and depth conventions
- honour its barriers and lifetimes
- write the correct output
- fall back to the original path

A game update can change any of that. This is why each entry above names one exact build.
