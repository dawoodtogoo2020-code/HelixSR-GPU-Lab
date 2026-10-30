# Roadmap

## Done in v0.2.0-beta (30 Oct 2026)

- **Distribution.** HelixSR's PS5 ports ship with Lone Wolf's permission. No game or NVIDIA files ship: patches start from your own game files, and the network is built from NVIDIA's DLL on your PC.
- **One installer** (`gpulab.py`): it checks every file against the tested builds, backs up and journals every change, and has ON/OFF and a full uninstall. It was tested end to end against a simulated PS5 loaded with stock game files.
- **Three games:** CONTROL Resonant and Alan Wake 2 (regular PS5), and SHADOW OF THE COLOSSUS (PS5 Pro).

## Next

- **Reports from real consoles:** other firmware versions and homebrew setups, clean install and uninstall. The installer's first runs on other PS5s are this beta.
- **One GPU Lab build for both consoles,** with an Alan Wake 2 switch.
- **Alan Wake 2:** Quality mode, sharpening (the game asks for 0.6 RCAS), history reset on camera cuts.
- **SHADOW OF THE COLOSSUS:** 60 FPS with HelixSR on (HelixSR needs about 5 ms per frame; it takes 17 ms today), and the game's own depth, motion and jitter.
- **CONTROL:** frame-timed r18, fallback and reset qualification, PS5 Pro.

## Earlier plan (kept for reference)

## 1. Distributable CONTROL preview

- Resolve provenance and redistribution for every native runtime, model, shader and adapter input.
- Build the native app and payloads from documented, pinned public dependencies.
- Provide a per-build integration package without redistributed game files.
- Replace the lab-specific installer with a configurable, verified support installer and interruption recovery.
- Verify clean install, launch, activation, original-path restore and removal on a separately configured supported console.
- Complete fallback, history/reset, scene and visual-quality qualification. Publish precise supported firmware and game-build information only after testing.

## 2. Streamlined delivery

- One versioned support bundle with hashes, clear prerequisites and an installation receipt.
- A console UI that distinguishes detected, investigated and qualified games.
- A payload delivery route after the transport, privileges, app registration and recovery flow have been validated.
- No automatic application to unknown titles or mismatched builds.

## 3. More games

- Verify SAROS's installed build and active upscaler, then establish the observed temporal resource contract.
- Add one adapter at a time with reproducible numerical and live tests.
- Investigate other PS5 FSR titles. FSR use alone is not sufficient for qualification.
- Qualify alternative backends independently; PC support does not imply native PS5 support.

## 4. Performance evidence

- Matched scenes and timing boundaries, repeated interleaved runs and documented calibration.
- Direct comparison with the title's original upscaler where technically available.
- Separate image quality, GPU cost, frame time and input latency results.

There is no promised release date or universal compatibility guarantee. Issues and pull requests should move one of these concrete milestones forward.
