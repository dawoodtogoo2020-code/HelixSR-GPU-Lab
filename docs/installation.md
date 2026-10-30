# Installation

**To install HelixSR GPU Lab on your PS5, follow the [install guide](install-ps5.md).** It covers requirements, the three steps, ON/OFF, uninstall and help.

## What the installer changes on the PS5

| Where | What | Removed by `uninstall` |
| --- | --- | --- |
| `/data/homebrew/PPSA99272/` | The GPU Lab app (ShadowMount+ shows it on the home screen) | yes |
| `/data/pldmgr/payloads/local/helixsr-devloop/` | GPU Lab's launch and inventory payloads | yes |
| `/data/helixsr-speed/` | GPU Lab's data folders, `device.txt` (PS5 Pro), and each game's READY marker and OFF slot | yes |
| `/data/homebrew/backports/PPSA34547/` | CONTROL's HelixSR files: eboot, archive, TOC, effect | yes |
| `/data/homebrew/backports/PPSA02572/`, `/data/helixsr-aw2-t12/` | Alan Wake 2's HelixSR eboot, effect, chain and status | yes |
| `/data/homebrew/backports/CUSA08809/fakelib2/`, `/data/helixsr-hook/CUSA08809/` | SHADOW OF THE COLOSSUS's GNM hook, HelixSR package and switch | yes |

- **Replaced files are kept.** A file that is already there is copied to your PC and renamed on the PS5 to `<name>.bak-<id>`. `uninstall` deletes what was added and renames those back.
- **Writes need everything closed.** Each write needs a full process list with no game or app running.
- **Every write is checked.** Each file is read back and must match its tested hash. The console may serve an executable in another form, and the manifests list both forms.
- **Changed files are protected.** `uninstall` refuses if a file it would remove was changed after install.
- **ON/OFF is a move.** For CONTROL and Alan Wake 2, the game's backport folder moves between `backports/<title>` (ON) and `/data/helixsr-speed/packages/<title>-<version>/stored/game` (OFF), so ShadowMount+ applies nothing when it is OFF. For SHADOW OF THE COLOSSUS, the switch is `helixsr.txt`.

## On the PC

`my-ps5/` in the unzipped folder holds:
- `settings.json` (the PS5's address)
- `<IP>/journal.json` and backups
- `game-files/` (your copies of the game files the patches start from)
- `weights.ngx.bin` (the network built from NVIDIA's DLL)

It is yours. Do not share it or commit it.

## Developer tools

The earlier host tools still work: the bundle verifier, the benchmark comparator and the temporal adapter contract. See [installer.md](installer.md) and [benchmark.md](benchmark.md).
