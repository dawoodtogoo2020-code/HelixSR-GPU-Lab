#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""HelixSR GPU Lab for PS5 (public beta): install, switch and remove HelixSR from your PC.

    python gpulab.py                       guided install: asks for your PS5's IP address and does the rest
    python gpulab.py install   [--ps5 IP]  same, with options (see --help)
    python gpulab.py status    [--ps5 IP]  what is installed, and ON or OFF
    python gpulab.py on|off GAME [--ps5 IP]
    python gpulab.py uninstall [--ps5 IP]  puts back everything this tool changed

Nothing from a game, Sony or NVIDIA ships with GPU Lab. This tool builds HelixSR's files on your PC from your own copy
of each game (read from your PS5 while the game runs) and from NVIDIA's DLSS DLL (downloaded from NVIDIA after you
accept NVIDIA's license), and checks every file against the exact build that was tested. Every change on the PS5 is
recorded in my-ps5/<IP>/journal.json; anything that was already there is backed up to your PC and renamed to
<name>.bak-<id> on the PS5, never overwritten, so `uninstall` can put it back.
"""
import argparse
import datetime
import ftplib
import hashlib
import json
import os
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import helixsr_patch  # noqa: E402
import helixsr_weights  # noqa: E402
from ps5_console import Console, Refused, need  # noqa: E402

ROOT = HERE.parent
GAMES = ROOT / 'games'
CONSOLES = ROOT / 'console'
HOME = ROOT / 'my-ps5'
VERSION = '0.2.0-beta'
CONSOLE = Console   # tests swap in a simulated PS5
APP0 = ('/mnt/sandbox/{title}_000/app0', '/mnt/sandbox/pfsmnt/{title}-app0')


def now():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def sha(data):
    return hashlib.sha256(data).hexdigest()


def say(text=''):
    print(text, flush=True)


class UI:
    """Questions to the user; --yes answers every question with its default (for scripts)."""
    def __init__(self, yes=False, ask=input):
        self.yes, self._ask = yes, ask

    def ask(self, question, default=''):
        if self.yes:
            return default
        answer = self._ask(question + (f' [{default}] ' if default else ' ')).strip()
        return answer or default

    def confirm(self, question, default=True):
        answer = self.ask(question + (' (Y/n)' if default else ' (y/N)'), 'y' if default else 'n')
        return answer.lower().startswith('y')

    def wait(self, text):
        if not self.yes:
            self._ask(text + ' Then press Enter. ')


# ---------------------------------------------------------------- manifests

def load_json(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))


def games():
    return {g['id']: g for g in (load_json(p) for p in sorted(GAMES.glob('*/game.json')))}


def bundle(model):
    return load_json(CONSOLES / model / 'console.json')


def accepts(row):
    return [tuple(x) for x in row['accept']]


# ---------------------------------------------------------------- local state

class Journal:
    """Everything this tool did on one PS5, in order, so it can be undone."""
    def __init__(self, host):
        self.dir = HOME / host.replace(':', '_')
        self.path = self.dir / 'journal.json'
        self.data = load_json(self.path) if self.path.exists() else dict(host=host, tool=VERSION, entries=[], games={})

    def save(self):
        self.dir.mkdir(parents=True, exist_ok=True)
        tmp = self.path.with_suffix('.tmp')
        tmp.write_text(json.dumps(self.data, indent=1) + '\n', encoding='utf-8')
        os.replace(tmp, self.path)

    def add(self, **row):
        row['utc'] = now()
        self.data['entries'].append(row)
        self.save()
        return row


def mkdirs(c, journal, path):
    parts = path.strip('/').split('/')
    for i in range(1, len(parts) + 1):
        p = '/' + '/'.join(parts[:i])
        st = c.stat(p)
        if st is None:
            c.mkdir(p)
            journal.add(op='mkdir', path=p)
        else:
            need(st['dir'], 'Expected a folder on the PS5, found a file: ' + p)


def state_of(c, path, accept, kind='file', cap=None):
    """'absent', 'ours' (one of the accepted forms) or 'other'."""
    st = c.stat(path)
    if st is None:
        return 'absent'
    if st['dir']:
        return 'other'
    if kind == 'telemetry':
        return 'ours' if any(st['bytes'] == b for b, _ in accept) else 'other'
    if not any(st['bytes'] == b for b, _ in accept):
        return 'other'
    n, h = c.hash(path, (cap or max(b for b, _ in accept)) + 1)
    return 'ours' if (n, h) in accept else 'other'


def put(c, journal, path, data, accept, kind='file', label=''):
    """Upload one file. A different file already there is backed up to the PC and renamed on the PS5 first."""
    accept = [tuple(a) for a in accept]
    current = state_of(c, path, accept, kind)
    if current == 'ours':
        journal.add(op='put', path=path, kind=kind, accept=accept, prior=None, already=True)
        return False
    prior = None
    if current == 'other':
        st = c.stat(path)
        need(not st['dir'], 'A folder is in the way on the PS5: ' + path)
        old = c.read(path, st['bytes'] + 1)
        tag = sha(old)[:8]
        local = journal.dir / 'backup' / tag / path.lstrip('/')
        local.parent.mkdir(parents=True, exist_ok=True)
        local.write_bytes(old)
        need(sha(local.read_bytes()) == sha(old), 'Could not back up %s to the PC' % path)
        aside = '%s.bak-%s' % (path, tag)
        need(not c.exists(aside), 'Backup name already used on the PS5: ' + aside)
        c.rename(path, aside)
        prior = dict(sha256=sha(old), bytes=len(old), console=aside, pc=str(local))
    else:
        mkdirs(c, journal, path.rsplit('/', 1)[0])
    row = journal.add(op='put', path=path, kind=kind, accept=accept, prior=prior, already=False, done=False)
    say(f'  writing {path}' + (f' ({label})' if label else ''))
    c.write(path, data)
    need(state_of(c, path, accept, kind) == 'ours', 'The PS5 did not store %s correctly' % path)
    row['done'] = True
    journal.save()
    return True


# ---------------------------------------------------------------- console checks

def wait_closed(c, ui):
    for _ in range(30):
        running = c.running()
        if not running:
            return
        if ui.yes:
            raise Refused('A game or app is running on the PS5 (%s): close it first' % ', '.join(running))
        ui.wait('Close every game and app on your PS5 (press the PS button, then Close Game).')
    raise Refused('Games or apps are still running on the PS5')


def game_version(c, g):
    """The installed version, or None. PS4 titles keep param.sfo in /system_data/priv/appmeta on newer firmware."""
    p = g['param']
    path = next((x for x in p['paths'] if c.stat(x) is not None), None)
    if path is None:
        return None
    raw = c.read(path, 1 << 20)
    if p['format'] == 'json':
        try:
            return json.loads(raw).get(p['key'])
        except ValueError:
            return 'unreadable'
    return sfo_value(raw, p['key']) or 'unreadable'


def sfo_value(raw, key):
    """A string value from a PS4 param.sfo."""
    if raw[:4] != b'\0PSF' or len(raw) < 20:
        return None
    keys, data, count = int.from_bytes(raw[8:12], 'little'), int.from_bytes(raw[12:16], 'little'), \
        int.from_bytes(raw[16:20], 'little')
    for i in range(count):
        e = 20 + 16 * i
        k_off = int.from_bytes(raw[e:e + 2], 'little')
        length = int.from_bytes(raw[e + 4:e + 8], 'little')
        d_off = int.from_bytes(raw[e + 12:e + 16], 'little')
        name = raw[keys + k_off:raw.index(b'\0', keys + k_off)].decode('ascii', 'replace')
        if name == key:
            return raw[data + d_off:data + d_off + length].rstrip(b'\0').decode('utf-8', 'replace')
    return None


def detect_model(c, ui, forced=None):
    if forced:
        return forced
    st = c.stat('/data/helixsr-speed/device.txt')
    if st and not st['dir'] and c.read('/data/helixsr-speed/device.txt', 64).strip() == b'ps5-pro':
        return 'ps5-pro'
    return 'ps5-pro' if ui.confirm('Is this PS5 a PS5 Pro?', default=False) else 'ps5'


def survey(c, model):
    """[(game, installed version or None, supported here, reason)]"""
    rows = []
    for g in games().values():
        version = game_version(c, g)
        if model not in g['models']:
            why = 'needs a ' + ' or '.join('PS5 Pro' if m == 'ps5-pro' else 'regular PS5' for m in g['models'])
            rows.append((g, version, False, why))
        elif version is None:
            rows.append((g, None, False, 'not installed'))
        elif version != g['version']:
            rows.append((g, version, False, f"version {version} installed; this build needs {g['version']}"))
        else:
            rows.append((g, version, True, 'ready'))
    return rows


# ---------------------------------------------------------------- building on the PC

def ensure_weights(ui, dlss=None):
    path = HOME / 'weights.ngx.bin'
    if path.exists() and sha(path.read_bytes()) == helixsr_weights.NGX_SHA256:
        return path.read_bytes()
    say('\nHelixSR network: building it on this PC from NVIDIA\'s DLSS DLL.')
    if dlss:
        data = helixsr_weights.build(dll=Path(dlss).read_bytes())
    else:
        data = helixsr_weights.build(dll=helixsr_weights.download_dll(ask=lambda q: ui.ask(q, 'yes' if ui.yes else '')))
    helixsr_weights.write(path, data)
    say('  network ready (matches the tested build)')
    return data


def cache_path(g, row):
    """Where your copy of one game file is kept on this PC (my-ps5/game-files), so a reinstall need not read it again."""
    return HOME / 'game-files' / f"{g['title']}-{g['version']}" / row['name'].replace('@', '_at_').replace('+', '_len_')


def cached_inputs(g):
    out = {}
    for row in g.get('inputs', []):
        p = cache_path(g, row)
        if p.exists():
            data = p.read_bytes()
            if any((len(data), sha(data)) == tuple(a) for a in row['accept']):
                out[row['name']] = data
    return out


def read_input(c, root, row, progress=None):
    if 'offset' in row:
        return c.read_range(root + '/' + row['path'], row['offset'], row['accept'][0][0])
    cap = max(b for b, _ in row['accept'])
    return c.read(root + '/' + row['path'], cap + 1, progress)


def app0_root(c, title):
    for pattern in APP0:
        root = pattern.format(title=title)
        listing = c.listing(root)
        if listing and 'eboot.bin' in listing:
            return root
    return None


def from_folders(g, folders):
    """Inputs from game folders on the PC (--game-files), when the user has them."""
    out = {}
    for folder in folders or []:
        for row in g.get('inputs', []):
            p = Path(folder) / row['path']
            if row['name'] in out or not p.is_file():
                continue
            with p.open('rb') as f:
                if 'offset' in row:
                    f.seek(row['offset'])
                    data = f.read(row['accept'][0][0])
                else:
                    data = f.read()
            if any((len(data), sha(data)) == tuple(a) for a in row['accept']):
                out[row['name']] = data
    return out


def gather_inputs(c, ui, g, folders=None):
    """Your own copies of the game files HelixSR's patches start from."""
    have = cached_inputs(g)
    have.update(from_folders(g, folders))
    missing = [r for r in g.get('inputs', []) if r['name'] not in have]
    if missing:
        root = app0_root(c, g['title'])
        if root is None:
            ui.wait(f"Start {g['name']} on your PS5 and wait for its title screen (HelixSR reads your copy of a few "
                    'game files from it).')
            for _ in range(20):
                root = app0_root(c, g['title'])
                if root:
                    break
                time.sleep(3)
            need(root is not None, f"Could not see {g['name']}'s files on the PS5. Start the game, wait for its title "
                                   'screen, and run the installer again.')
        for row in missing:
            say(f"  reading your copy of {row['path']}" + (' (part)' if 'offset' in row else ''))
            data = read_input(c, root, row)
            if not any((len(data), sha(data)) == tuple(a) for a in row['accept']):
                raise Refused(f"Your {g['name']} file {row['path']} is not the supported version "
                              f"({len(data)} bytes, sha256 {sha(data)[:16]}). This build supports {g['name']} "
                              f"{g['version']} only.")
            p = cache_path(g, row)
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(data)
            have[row['name']] = data
    return have


def build_files(g, inputs, weights):
    """remote path -> (bytes, accepted readback forms, kind), built and checked on the PC."""
    out = {}
    for row in g['files']:
        if 'bundle' in row:
            data = (ROOT / row['bundle']).read_bytes()
            need(sha(data) == row['sha256'], 'Damaged download (%s): download GPU Lab again' % row['bundle'])
        elif 'text' in row:
            data = row['text'].encode()
        elif 'zeros' in row:
            data = bytes(row['zeros'])
        else:
            data = None
            for patch in row['patches']:
                header, ops = helixsr_patch.read_patch(ROOT / patch)
                sources = {s['name']: inputs.get(s['name']) for s in header['sources']}
                if all(v is not None and sha(v) == s['sha256'] for s, v in zip(header['sources'], sources.values())):
                    data = helixsr_patch.apply(header, ops, sources, {'weights': weights})
                    break
            need(data is not None, 'No patch matches your copy of the game for ' + row['remote'])
        need(sha(data) == row['sha256'], 'Built file differs from the tested build: ' + row['remote'])
        out[row['remote']] = (data, accepts(row), row.get('kind', 'file'))
    return out


# ---------------------------------------------------------------- switching

def folder_state(c, g):
    """'on', 'off', 'absent' or 'both' for a whole-folder game."""
    s = g['switch']
    on, off = c.stat(s['on']), c.stat(s['off'])
    return 'both' if on and off else 'on' if on else 'off' if off else 'absent'


def switch(c, g, target):
    s = g['switch']
    if s['kind'] == 'text':
        current = c.read(s['path'], 256) if c.exists(s['path']) else None
        need(current is not None, f"{g['name']}: HelixSR is not installed")
        c.write(s['path'], s[target].encode())
        need(c.read(s['path'], 256) == s[target].encode(), 'Switch did not verify')
        return f"{g['name']}: HelixSR {target.upper()}"
    st = folder_state(c, g)
    if st == target:
        return f"{g['name']}: HelixSR was already {target.upper()}"
    need(st in ('on', 'off'), f"{g['name']}: HelixSR is not installed (or both copies exist; run status)")
    src, dst = (s['off'], s['on']) if target == 'on' else (s['on'], s['off'])
    c.rename(src, dst)
    need(folder_state(c, g) == target, 'Switch did not verify')
    return f"{g['name']}: HelixSR {target.upper()}"


def foreign_files(c, g):
    """Files in the game's backport folder that are not HelixSR's (a folder switch would move them too)."""
    root = g['switch']['on']
    ours = {r['remote'][len(root) + 1:] for r in g['files'] if r['remote'].startswith(root + '/')}
    found = []

    def walk(folder, rel, depth):
        listing = c.listing(folder) or {}
        for name, st in listing.items():
            r = rel + name
            if st['dir'] and depth < 6:
                walk(folder + '/' + name, r + '/', depth + 1)
            elif r not in ours and '.bak-' not in name:
                found.append(r)
    if c.exists(root):
        walk(root, '', 0)
    return found


# ---------------------------------------------------------------- commands

def connect(args, ui):
    settings = HOME / 'settings.json'
    saved = load_json(settings) if settings.exists() else {}
    host = args.ps5 or ui.ask("Your PS5's IP address (Settings > Network > View Connection Status):",
                               saved.get('ps5', ''))
    need(host, 'No IP address given')
    HOME.mkdir(parents=True, exist_ok=True)
    settings.write_text(json.dumps(dict(saved, ps5=host), indent=1) + '\n', encoding='utf-8')
    return host


def install(args, ui):
    say(f'HelixSR GPU Lab {VERSION} installer\n')
    host = connect(args, ui)
    with CONSOLE(host) as c:
        c.running()   # pldmgr reachable, full list
        journal = Journal(host)
        model = detect_model(c, ui, journal.data.get('model') or args.model)
        journal.data['model'] = model
        journal.save()
        b = bundle(model)
        say(f"\nConsole: {b['name']}. Games:")
        rows = survey(c, model)
        for g, version, ok, why in rows:
            say(f"  {'+' if ok else '-'} {g['name']}: {why}")
        chosen = [g for g, _, ok, _ in rows if ok and (not args.game or g['id'] in args.game)]
        if not chosen:
            say('\nNo supported game is installed on this PS5, so nothing was changed. Supported games and versions are '
                'listed in docs/install-ps5.md.')
            return 1
        weights = ensure_weights(ui, args.dlss)
        built = {}
        for g in chosen:
            say(f"\n{g['name']}: preparing HelixSR files on this PC")
            inputs = gather_inputs(c, ui, g, args.game_files) if g.get('inputs') else {}
            built[g['id']] = build_files(g, inputs, weights)
            say('  built and checked against the tested build')
        say('\nInstalling on the PS5.')
        wait_closed(c, ui)
        for g in chosen:
            if g['switch']['kind'] == 'folder':
                st = folder_state(c, g)
                need(st != 'both', f"{g['name']}: both an ON and an OFF copy exist on the PS5; run uninstall first")
                if st == 'off':
                    say(f"  {g['name']} was switched OFF; switching it ON to update it")
                    switch(c, g, 'on')
                extra = foreign_files(c, g)
                if extra:
                    raise Refused(f"{g['name']}: your PS5 already has other files in {g['switch']['on']} "
                                  f"({', '.join(extra[:5])}). HelixSR's ON/OFF switch moves that whole folder, so it "
                                  'would move yours too. Move them away first, then run the installer again.')
        say('\nGPU Lab app:')
        for row in b['files']:
            data = (ROOT / row['bundle']).read_bytes()
            need(sha(data) == row['sha256'], 'Damaged download (%s): download GPU Lab again' % row['bundle'])
            put(c, journal, row['remote'], data, accepts(row))
        for d in b.get('dirs', []):
            mkdirs(c, journal, d)
        for row in b.get('texts', []):
            put(c, journal, row['remote'], row['text'].encode(), [(len(row['text']), sha(row['text'].encode()))])
        for g in chosen:
            say(f"\n{g['name']}:")
            for d in g.get('dirs', []):
                mkdirs(c, journal, d)
            for remote, (data, accept, kind) in built[g['id']].items():
                put(c, journal, remote, data, accept, kind)
            journal.data['games'][g['id']] = dict(state='installed', utc=now())
            journal.save()
        say('\nDone. On your PS5:')
        say('  1. Open HelixSR GPU Lab from the home screen (if it is not there yet, restart the PS5 and run your '
            'jailbreak and ShadowMount+ again).')
        for g in chosen:
            say(f"  - {g['name']}: {g['howto']}")
        say(f'\nTo remove everything again: run uninstall. Your journal: {journal.path}')
        return 0


def status(args, ui):
    host = connect(args, ui)
    with CONSOLE(host) as c:
        running = c.running()
        journal = Journal(host)
        model = journal.data.get('model') or detect_model(c, ui, args.model)
        say(f"PS5 {host} ({bundle(model)['name']})" + (f"; running: {', '.join(running)}" if running else ''))
        for g, version, ok, why in survey(c, model):
            line = f"  {g['name']}: {why}"
            if ok and g['id'] in journal.data['games']:
                if g['switch']['kind'] == 'folder':
                    st = folder_state(c, g)
                    line += '; HelixSR ' + {'on': 'ON', 'off': 'OFF', 'absent': 'not on the PS5',
                                             'both': 'DAMAGED (both copies exist)'}[st]
                else:
                    s = g['switch']
                    cur = c.read(s['path'], 256) if c.exists(s['path']) else b''
                    line += '; HelixSR ' + ('ON' if cur == s['on'].encode() else 'OFF' if cur == s['off'].encode()
                                            else 'not on the PS5')
            say(line)
        return 0


def onoff(args, ui, target):
    host = connect(args, ui)
    g = games().get(args.target_game)
    need(g is not None, 'Unknown game %r; one of: %s' % (args.target_game, ', '.join(games())))
    with CONSOLE(host) as c:
        wait_closed(c, ui)
        say(switch(c, g, target))
        return 0


def uninstall(args, ui):
    host = connect(args, ui)
    journal = Journal(host)
    entries = journal.data['entries']
    need(entries, 'Nothing to uninstall: this PC has no record of installing on %s' % host)
    by_id = games()
    with CONSOLE(host) as c:
        wait_closed(c, ui)
        for gid in journal.data['games']:
            g = by_id.get(gid)
            if g and g['switch']['kind'] == 'folder' and folder_state(c, g) == 'off':
                say(f"  switching {g['name']} ON first, so its files are where they were installed")
                switch(c, g, 'on')
        problems = []
        for e in entries:
            if e['op'] == 'put' and not e.get('already'):
                st = state_of(c, e['path'], [tuple(a) for a in e['accept']], e.get('kind', 'file'))
                if st == 'other' and e.get('kind') != 'setting' and e.get('done', True):
                    problems.append(e['path'])
        if problems:
            raise Refused('These files changed on the PS5 after installing, so nothing was removed: ' +
                          ', '.join(problems))
        for e in reversed(entries):
            if e.get('undone'):
                continue
            if e['op'] == 'put' and not e.get('already'):
                if c.exists(e['path']):
                    c.delete(e['path'])
                if e.get('prior'):
                    c.rename(e['prior']['console'], e['path'])
                say(f"  removed {e['path']}" + (' (your earlier file is back)' if e.get('prior') else ''))
            elif e['op'] == 'mkdir':
                listing = c.listing(e['path'])
                if listing is not None and not listing:
                    c.rmdir(e['path'])
            e['undone'] = True
            journal.save()
    journal.data['games'] = {}
    journal.data['uninstalled_utc'] = now()
    journal.save()
    os.replace(journal.path, journal.path.with_name('journal-uninstalled-%d.json' % int(time.time())))
    say('Uninstalled: the PS5 is as it was before GPU Lab was installed.')
    return 0


def main(argv=None, ui=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd')
    for name in ('install', 'status', 'uninstall', 'on', 'off'):
        p = sub.add_parser(name)
        p.add_argument('--ps5', help="your PS5's IP address")
        p.add_argument('--model', choices=('ps5', 'ps5-pro'), help='skip the PS5 Pro question')
        p.add_argument('--yes', action='store_true', help='answer every question with its default (scripts)')
        if name in ('on', 'off'):
            p.add_argument('target_game', metavar='GAME', help='control-r18, aw2-t12 or sotc-live3')
        if name == 'install':
            p.add_argument('--dlss', help="your own copy of NVIDIA's nvngx_dlss.dll (DLSS 310.7.0) instead of the download")
            p.add_argument('--game', action='append', help='only this game (control-r18, aw2-t12, sotc-live3)')
            p.add_argument('--game-files', action='append',
                           help="a folder on this PC with the game's files (its app0 contents), instead of reading them "
                                'from the PS5')
    args = ap.parse_args(argv)
    if args.cmd is None:
        args = ap.parse_args(['install'] + (argv or []))
    ui = ui or UI(args.yes)
    try:
        if args.cmd == 'install':
            return install(args, ui)
        if args.cmd == 'status':
            return status(args, ui)
        if args.cmd in ('on', 'off'):
            return onoff(args, ui, args.cmd)
        return uninstall(args, ui)
    except Refused as e:
        say('\nStopped: %s' % e)
        return 2
    except SystemExit as e:          # a check in helixsr_patch / helixsr_weights refused
        if not isinstance(e.code, str):
            raise
        say('\nStopped: %s' % e.code)
        return 2
    except (OSError, ftplib.Error) as e:
        say('\nStopped: the connection to the PS5 failed (%s). Nothing half-done is left unrecorded: run the same '
            'command again, or uninstall.' % e)
        return 2
    except (KeyboardInterrupt, EOFError):
        say('\nStopped by you. Run the same command again to continue; uninstall undoes everything done so far.')
        return 130


if __name__ == '__main__':
    sys.exit(main())
