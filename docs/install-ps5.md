# Install HelixSR GPU Lab on your PS5 (public beta)

HelixSR GPU Lab puts [Lone Wolf's HelixSR](https://github.com/lonewolf0622/HelixSR) neural upscaler into three PS5 games and adds an app to switch it on and off. You run one installer on your PC. It does everything over your home network.

> **Public beta.** The HelixSR builds it installs are the ones that ran on the developer's own regular PS5 and PS5 Pro. The installer itself is new, and this is its first run on other consoles. Every step is checked, and `uninstall` puts your PS5 back exactly as it was. Please report how it went (see [Help](#help)).

## What you need

**A jailbroken PS5 running the usual homebrew:**
- an FTP server on port **2121** (ftpsrv)
- the payload manager **pldmgr** on port **8084**
- **ShadowMount+** with backports
- tested setups: a regular PS5 (CFI-1016A) on firmware **11.60**, and a PS5 Pro on firmware **13.60**

**One or more of these games, at exactly this version:**

| Game | Version | Console | What you get |
| --- | --- | --- | --- |
| CONTROL Resonant (PPSA34547) | 01.300.300 | regular PS5 | Performance: 1505×847 → 1440p, 60 FPS target. Quality: 2258×1270 → 4K, 30 FPS |
| Alan Wake 2 (PPSA02572) | 01.200.007 | regular PS5 | Performance: 1505×847 → 1440p, 60 FPS (other modes use the game's own FSR2) |
| SHADOW OF THE COLOSSUS (CUSA08809) | 01.00 | PS5 Pro | Cinematic: 1440p → 4K, about 38 FPS |

**A Windows, Mac or Linux PC on the same network as the PS5.** On Windows nothing else is needed: if Python is missing, the installer fetches the official one from python.org by itself. On Mac or Linux you need Python 3.10 or newer ([python.org](https://www.python.org/downloads/) if `python3 --version` says otherwise).

## Install (about 10 minutes)

1. **Download** `HelixSR-GPU-Lab-v0.2.0-beta.zip` from [Releases](https://github.com/dawoodtogoo2020-code/HelixSR-GPU-Lab/releases) and unzip it.
2. **Start the installer:**
   - **Windows:** double-click `install.bat`.
   - **Mac / Linux:** open a terminal in the folder and run `sh install.sh`.
3. **Answer its questions:**
   - Your PS5's **IP address**: on the PS5, go to *Settings › Network › View Connection Status*.
   - Whether it is a **PS5 Pro**.
   - Type **yes** to accept **NVIDIA's DLSS license**. HelixSR's network is built from NVIDIA's DLSS file, which GPU Lab does not ship. The installer downloads it from NVIDIA's GitHub, uses it once on your PC and deletes it. This is the same thing HelixSR's own setup does.
   - When asked, **start the game** on your PS5 and wait at its title screen. The installer reads a few of your game's own files to build HelixSR's versions, because GPU Lab does not ship any game files.
   - When asked, **close the game**. The installer then copies everything to the PS5 and checks each file.
4. **Play.** Open **HelixSR GPU Lab** on the PS5 home screen and pick your game:
   - **Cross** switches HelixSR on or off.
   - **Triangle** starts the game.
   - **CONTROL:** use Performance or Quality.
   - **SHADOW OF THE COLOSSUS:** use the Cinematic graphics mode.
   - **Alan Wake 2:** HelixSR is on as soon as it is installed. Start the game from the home screen and use Performance mode. GPU Lab has no Alan Wake 2 switch yet: on your PC, double-click `alan-wake-2-off.bat` or `alan-wake-2-on.bat` (on Mac/Linux, `sh gpulab.sh off aw2-t12`).

If GPU Lab does not show up on the home screen, restart the PS5 and run your jailbreak and ShadowMount+ again.

## Turn it off, check it, remove it

| What | Windows | Mac / Linux |
| --- | --- | --- |
| HelixSR off / on | Cross in GPU Lab, or `gpulab.bat off control-r18` | `sh gpulab.sh off control-r18` |
| See what is installed | `gpulab.bat status` | `sh gpulab.sh status` |
| Remove everything | double-click `uninstall.bat` | `sh uninstall.sh` |

Game names for `on` and `off`: `control-r18`, `aw2-t12`, `sotc-live3`.

`uninstall` removes every file the installer added and puts back anything it replaced. Replaced files were backed up to your PC first, and renamed to `<name>.bak-<id>` on the PS5. Nothing was overwritten.

## What the installer does, and what it will not do

- **It checks before it writes.** Every file it builds on your PC must match the exact build that was tested, byte for byte. If your game is a different version, or one of its files differs, it stops before changing anything and tells you why.
- **It only writes with nothing running.** It asks you to close games and apps first.
- **It installs into these folders:**
  - GPU Lab: `/data/homebrew/PPSA99272`
  - its payloads: `/data/pldmgr/payloads/local/helixsr-devloop`
  - each game's HelixSR files: `/data/homebrew/backports/<title>`, read by ShadowMount+
  - settings: `/data/helixsr-speed`, `/data/helixsr-aw2-t12` and `/data/helixsr-hook`
- **It will not touch other mods.** If you already have other files in a game's backport folder, it stops, because the ON/OFF switch moves that whole folder.
- **The `my-ps5` folder stays on your PC.** It holds your journal, backups, your game files and the network built from NVIDIA's DLL. Do not share it.

## Help

Most messages tell you what to do. For example: load ftpsrv or pldmgr, start or close the game, or which game version is needed.

To report a result, good or bad, post in the [#helixsr-playstation-5](https://discord.com/channels/1555240126139867169/1565267777135312987) channel of the [Blackheart 909 Gaming Discord](https://discord.gg/k4UWq8DqK), or open a [GitHub issue](https://github.com/dawoodtogoo2020-code/HelixSR-GPU-Lab/issues). Include:
- your console, firmware and game versions
- what you did
- the installer's last lines

Leave out your IP address and your `my-ps5` folder.
