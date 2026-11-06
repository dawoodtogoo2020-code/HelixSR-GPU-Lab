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
VERSION = '0.3.1-saros-effects-beta'
CONSOLE = Console   # tests swap in a simulated PS5
APP0 = tuple('/mnt/sandbox/{title}_%03d/app0' % slot for slot in range(10)) + (
    '/mnt/sandbox/pfsmnt/{title}-app0',)


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
            row = journal.add(op='mkdir', path=p, pending=True)
            c.mkdir(p)
            row['pending'] = False
            journal.save()
        else:
            need(st['dir'], 'Expected a folder on the PS5, found a file: ' + p)


def state_of(c, path, accept, kind='file', cap=None, normalized_elf=None):
    """'absent', 'ours' (one of the accepted forms) or 'other'."""
    st = c.stat(path)
    if st is None:
        return 'absent'
    if st['dir']:
        return 'other'
    if kind == 'telemetry':
        return 'ours' if any(st['bytes'] == b for b, _ in accept) else 'other'
    if any(st['bytes'] == b for b, _ in accept):
        n, h = c.hash(path, (cap or max(b for b, _ in accept)) + 1)
        if (n, h) in accept:
            return 'ours'
    if normalized_elf is not None:
        spec = normalized_elf
        n = spec.get('bytes')
        digest = spec.get('sha256', '')
        need(type(n) is int and 64 <= n <= 256 * 1024 * 1024 and
             spec.get('zero_ranges') == [[40, 48], [58, 64]] and
             type(spec.get('max_zero_tail')) is int and 0 <= spec['max_zero_tail'] <= 75 and
             isinstance(digest, str) and len(digest) == 64 and all(x in '0123456789abcdef' for x in digest),
             'Invalid normalized ELF manifest')
        if n <= st['bytes'] <= n + spec['max_zero_tail']:
            data = c.read(path, n + spec['max_zero_tail'] + 1)
            if len(data) == st['bytes'] and data[:6] == b'\x7fELF\x02\x01' and not any(data[n:]):
                clean = bytearray(data[:n])
                for lo, hi in spec['zero_ranges']:
                    clean[lo:hi] = bytes(hi-lo)
                if sha(clean) == digest:
                    return 'ours'
    return 'other'


def preflight_put(c, path, data, accept, kind='file', normalized_elf=None):
    """Inspect all existing ancestors and backup collisions without changing the console."""
    parts = path.strip('/').split('/')
    absent = False
    for i in range(1, len(parts)):
        parent = '/' + '/'.join(parts[:i])
        if absent:
            continue
        st = c.stat(parent)
        if st is None:
            absent = True
        else:
            need(st['dir'], 'Expected a folder on the PS5: ' + parent)
    if absent:
        return
    current = state_of(c, path, [tuple(a) for a in accept], kind, normalized_elf=normalized_elf)
    if current == 'other':
        st = c.stat(path)
        need(not st['dir'], 'A folder is in the way on the PS5: ' + path)
        old = c.read(path, st['bytes'] + 1)
        need(len(old) == st['bytes'], 'Could not read existing file: ' + path)
        aside = '%s.bak-%s' % (path, sha(old)[:8])
        need(not c.exists(aside), 'Backup name already used on the PS5: ' + aside)


def put(c, journal, path, data, accept, kind='file', label='', normalized_elf=None):
    """Upload one file. A different file already there is backed up to the PC and renamed on the PS5 first."""
    accept = [tuple(a) for a in accept]
    current = state_of(c, path, accept, kind, normalized_elf=normalized_elf)
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
        prior = dict(sha256=sha(old), bytes=len(old), console=aside, pc=str(local))
    else:
        mkdirs(c, journal, path.rsplit('/', 1)[0])
    row = journal.add(op='put', path=path, kind=kind, accept=accept, prior=prior, already=False, done=False, normalized_elf=normalized_elf)
    if prior:
        c.rename(path, prior['console'])
    say(f'  writing {path}' + (f' ({label})' if label else ''))
    c.write(path, data)
    need(state_of(c, path, accept, kind, normalized_elf=normalized_elf) == 'ours', 'The PS5 did not store %s correctly' % path)
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
            metadata = json.loads(raw)
            value = metadata.get(p['key']) if isinstance(metadata, dict) else None
            return value if isinstance(value, str) and value else 'unreadable'
        except ValueError:
            return 'unreadable'
    return sfo_value(raw, p['key']) or 'unreadable'


def sfo_value(raw, key):
    """A bounded UTF-8 string from a PS4 param.sfo; damaged metadata is unreadable."""
    if raw[:4] != b'\0PSF' or len(raw) < 20:
        return None
    keys, data, count = int.from_bytes(raw[8:12], 'little'), int.from_bytes(raw[12:16], 'little'), \
        int.from_bytes(raw[16:20], 'little')
    if not (20 + 16 * count <= keys <= data <= len(raw)):
        return None
    result = None
    for i in range(count):
        e = 20 + 16 * i
        k_off = int.from_bytes(raw[e:e + 2], 'little')
        fmt = int.from_bytes(raw[e + 2:e + 4], 'little')
        length = int.from_bytes(raw[e + 4:e + 8], 'little')
        capacity = int.from_bytes(raw[e + 8:e + 12], 'little')
        d_off = int.from_bytes(raw[e + 12:e + 16], 'little')
        end = raw.find(b'\0', keys + k_off, data)
        if end < 0 or length > capacity or data + d_off + capacity > len(raw):
            return None
        try:
            name = raw[keys + k_off:end].decode('ascii')
        except UnicodeError:
            return None
        if name == key:
            value = raw[data + d_off:data + d_off + length]
            if fmt != 0x204 or not value or value[-1:] != b'\0' or result is not None:
                return None
            try:
                result = value[:-1].decode('utf-8')
            except UnicodeError:
                return None
            if not result or any(ord(c) < 32 for c in result):
                return None
    return result


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
    if row.get('raw_self'):
        return c.read_raw_self(root + '/' + row['path'], max(b for b, _ in row['accept']) + 1, progress)
    if 'offset' in row:
        return c.read_range(root + '/' + row['path'], row['offset'], row['accept'][0][0])
    cap = max(b for b, _ in row['accept'])
    return c.read(root + '/' + row['path'], cap + 1, progress)


def app0_root(c, title):
    for pattern in APP0:
        root = pattern.format(title=title)
        listing = c.listing(root)
        if listing and 'eboot.bin' in listing and not listing['eboot.bin']['dir']:
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

def overlay_rows(g):
    root = g['switch']['on']
    return [r for r in g['files'] if r['remote'].startswith(root + '/')]


def overlay_positions(c, g, complete=True, journal_entries=()):
    """Validate both trees and return each managed file's sole location."""
    on, off = g['switch']['on'], g['switch']['off']
    rows = overlay_rows(g)
    need(rows, 'Overlay manifest has no managed files')
    deps = g.get('preserved_dependencies', [])
    for dep in deps:
        need(dep['remote'].startswith(on + '/'), 'Invalid dependency path')
    managed = {r['remote'][len(on)+1:]: r for r in rows}
    backups = {}
    for e in journal_entries:
        prior = e.get('prior')
        path = e.get('path', '')
        if e.get('op') != 'put' or e.get('undone') or not prior or not path.startswith(on + '/'):
            continue
        digest = prior.get('sha256', '')
        if len(digest) != 64 or any(x not in '0123456789abcdef' for x in digest):
            raise Refused('Invalid journal backup identity')
        need(prior.get('console') == path + '.bak-' + digest[:8] and
             type(prior.get('bytes')) is int and prior['bytes'] >= 0, 'Invalid journal backup path')
        backups[prior['console'][len(on)+1:]] = prior
    preserved = {d['remote'][len(on)+1:]: d for d in deps}
    def inspect(root, allowed):
        if not c.exists(root):
            return set(), set()
        need(c.stat(root)['dir'], 'Expected overlay directory: ' + root)
        found, dirs = set(), set()
        def walk(path, rel):
            listing = c.listing(path)
            need(listing is not None, 'Cannot inspect overlay: ' + path)
            for name, st in listing.items():
                need(name not in ('.', '..') and '/' not in name, 'Invalid overlay entry')
                child = rel + name
                if st['dir']:
                    need(any(x.startswith(child + '/') for x in allowed), 'Unknown overlay directory: ' + child)
                    dirs.add(child)
                    walk(path + '/' + name, child + '/')
                else:
                    need(child in allowed, 'Unknown overlay file: ' + child)
                    found.add(child)
        walk(root, '')
        return found, dirs
    present_on, dirs_on = inspect(on, set(managed) | set(preserved) | set(backups))
    present_off, _ = inspect(off, set(managed) | set(backups))
    for rel, prior in backups.items():
        located = [root for root, found in ((on, present_on), (off, present_off)) if rel in found]
        need(len(located) <= 1, 'Journal backup is duplicated: ' + rel)
        for root in located:
            need(state_of(c, root + '/' + rel, [(prior['bytes'], prior['sha256'])]) == 'ours',
                 'Journal backup is changed: ' + rel)
    present_deps = present_on & set(preserved)
    dependency_dirs = {p.rsplit('/', 1)[0] for p in preserved if '/' in p}
    if present_deps or dependency_dirs & dirs_on:
        need(present_deps == set(preserved), 'CONTROL dependencies are incomplete')
    for rel in present_deps:
        d = preserved[rel]
        need(state_of(c, on + '/' + rel, [(d['bytes'], d['sha256'])]) == 'ours',
             'CONTROL dependency is changed: ' + rel)
    result = {}
    for rel, row in managed.items():
        locations = [side for side, found in (('on', present_on), ('off', present_off)) if rel in found]
        need(len(locations) <= 1 and (locations or not complete), 'Overlay file missing or duplicated: ' + rel)
        if locations:
            side = locations[0]
            need(state_of(c, g['switch'][side] + '/' + rel, accepts(row), row.get('kind', 'file')) == 'ours',
                 'Overlay file changed: ' + rel)
            result[rel] = side
    return result


def overlay_state(c, g, complete=True, *, allow_absent=False, journal_entries=()):
    positions = overlay_positions(c, g, complete and not allow_absent, journal_entries)
    if not positions:
        return 'absent'
    if complete and allow_absent:
        need(len(positions) == len(overlay_rows(g)), 'Some managed overlay files are missing')
    sides = set(positions.values())
    return next(iter(sides)) if len(sides) == 1 else 'mixed'


def ensure_remote_dirs(c, path):
    parts = path.strip('/').split('/')
    for i in range(1, len(parts)+1):
        p = '/' + '/'.join(parts[:i])
        st = c.stat(p)
        if st is None:
            c.mkdir(p)
        else:
            need(st['dir'], 'Expected a folder: ' + p)


def folder_state(c, g):
    """'on', 'off', 'absent' or 'both' for a whole-folder game."""
    s = g['switch']
    on, off = c.stat(s['on']), c.stat(s['off'])
    return 'both' if on and off else 'on' if on else 'off' if off else 'absent'


def switch(c, g, target):
    s = g['switch']
    if s['kind'] == 'overlay-files':
        positions = overlay_positions(c, g)
        moves = [(rel, side) for rel, side in positions.items() if side != target]
        for rel, side in moves:
            dest = s[target] + '/' + rel
            ensure_remote_dirs(c, dest.rsplit('/', 1)[0])
            c.rename(s[side] + '/' + rel, dest)
        need(overlay_state(c, g) == target, 'Switch did not verify')
        return f"{g['name']}: HelixSR {target.upper()}"
    if s['kind'] == 'text':
        current = c.read(s['path'], 256) if c.exists(s['path']) else None
        need(current in (s['on'].encode(), s['off'].encode()),
             f"{g['name']}: switch setting is missing or unrecognized")
        c.write(s['path'], s[target].encode())
        need(c.read(s['path'], 256) == s[target].encode(), 'Switch did not verify')
        return f"{g['name']}: HelixSR {target.upper()}"
    st = folder_state(c, g)
    need(st in ('on', 'off'), f"{g['name']}: HelixSR is not installed (or both copies exist; run status)")
    src = s[st]
    verify_overlay(c, g, src, complete=True)
    if target == 'on':
        verify_support(c, g)
    if st == target:
        return f"{g['name']}: HelixSR was already {target.upper()}"
    dst = s[target]
    c.rename(src, dst)
    need(folder_state(c, g) == target, 'Switch did not verify')
    return f"{g['name']}: HelixSR {target.upper()}"


def foreign_files(c, g, folder=None):
    """Files in the game's backport folder that are not HelixSR's (a folder switch would move them too)."""
    root = g['switch']['on']
    folder = folder or root
    ours = {r['remote'][len(root) + 1:] for r in g['files'] if r['remote'].startswith(root + '/')}
    found = []

    def walk(folder, rel, depth):
        listing = c.listing(folder)
        need(listing is not None, 'Cannot inspect overlay folder: ' + folder)
        for name, st in listing.items():
            r = rel + name
            if st['dir'] and depth < 6:
                if not any(path.startswith(r + '/') for path in ours):
                    found.append(r + '/')
                else:
                    walk(folder + '/' + name, r + '/', depth + 1)
            elif r not in ours:
                found.append(r)
    if c.exists(folder):
        walk(folder, '', 0)
    return found


def verify_overlay(c, g, folder, complete):
    """Only known, hash-verified files may occupy a whole-folder overlay."""
    extra = foreign_files(c, g, folder)
    need(not extra, f"{g['name']}: other files in the overlay folder; refusing to move them")
    root = g['switch']['on']
    for row in g['files']:
        if not row['remote'].startswith(root + '/'):
            continue
        path = folder + row['remote'][len(root):]
        state = state_of(c, path, accepts(row), row.get('kind', 'file'))
        need(state == 'ours' or (not complete and state == 'absent'),
             f"{g['name']}: overlay file is missing or changed: {path}")


def verify_support(c, g):
    required = {'aw2-t12': {'/data/helixsr-aw2-t12/copy.bin', '/data/helixsr-aw2-t12/chain.bin'},
                'saros-native-v1': {'/data/helixsr-saros-full/tile.bin'}}.get(g['id'])
    if required is None:
        return
    rows = {r['remote']: r for r in g['files'] if r['remote'] in required}
    need(set(rows) == required, g['name'] + ' support manifest is incomplete')
    for path, row in rows.items():
        need(state_of(c, path, accepts(row), row.get('kind', 'file')) == 'ours',
             g['name'] + ' support file is missing or changed: ' + path)


class FolderRedirect:
    """Access the OFF folder while keeping journal paths in their canonical ON form."""
    def __init__(self, console, on, off):
        self.console, self.on, self.off = console, on, off

    def path(self, path):
        return self.off + path[len(self.on):] if path == self.on or path.startswith(self.on + '/') else path

    def __getattr__(self, name):
        method = getattr(self.console, name)
        if name in ('stat', 'exists', 'read', 'hash', 'listing', 'mkdir', 'write', 'delete', 'rmdir'):
            return lambda path, *args, **kwargs: method(self.path(path), *args, **kwargs)
        if name == 'rename':
            return lambda old, new: method(self.path(old), self.path(new))
        return method


# ---------------------------------------------------------------- commands

def connect(args, ui):
    settings = HOME / 'settings.json'
    saved = load_json(settings) if settings.exists() else {}
    host = args.ps5 or ui.ask("Your PS5's IP address (Settings > Network > View Connection Status):",
                               saved.get('ps5', ''))
    need(host, 'No IP address given')
    if args.cmd != 'status':
        HOME.mkdir(parents=True, exist_ok=True)
        settings.write_text(json.dumps(dict(saved, ps5=host), indent=1) + '\n', encoding='utf-8')
    return host


def adopt(c, journal, chosen, b, ui):
    """Upgrade app and installer metadata without reading or writing game inputs."""
    need(chosen and all(g['id'] in ('aw2-t12', 'control-r18', 'saros-native-v1') for g in chosen),
         'Adoption supports existing AW2, CONTROL and SAROS integrations')
    app_files = []
    for row in b['files']:
        data = (ROOT / row['bundle']).read_bytes()
        need(sha(data) == row['sha256'], 'Damaged bundle: ' + row['bundle'])
        app_files.append((row, data))
    metadata = []
    original_modes = {}
    for g in chosen:
        kind = g['switch']['kind']
        if kind == 'overlay-files':
            st = overlay_state(c, g)
            need(st in ('on', 'off'), 'Finish the interrupted switch before adoption')
        else:
            st = folder_state(c, g)
            need(st in ('on', 'off'), 'The existing overlay is missing or ambiguous')
            verify_overlay(c, g, g['switch'][st], complete=True)
        original_modes[g['id']] = st
        verify_support(c, g)
        for row in g['files']:
            if row['remote'].endswith(('/READY', '/CATALOG')):
                need('text' in row, 'Unsupported metadata format')
                data = row['text'].encode()
                need(sha(data) == row['sha256'], 'Metadata digest mismatch')
                metadata.append((row, data))
    wait_closed(c, ui)
    # Recheck after the user closes games, before the first console mutation.
    for g in chosen:
        if g['switch']['kind'] == 'overlay-files':
            st = overlay_state(c, g)
            need(st == original_modes[g['id']], 'The overlay changed during preflight')
        else:
            st = folder_state(c, g)
            need(st == original_modes[g['id']], 'The overlay changed during preflight')
            verify_overlay(c, g, g['switch'][st], complete=True)
        verify_support(c, g)
    destinations = [(row['remote'], data, accepts(row), row.get('normalized_elf')) for row, data in app_files + metadata]
    destinations += [(row['remote'], row['text'].encode(), [(len(row['text'].encode()), sha(row['text'].encode()))], None)
                     for row in b.get('texts', [])]
    paths = [x[0] for x in destinations]
    need(len(paths) == len(set(paths)), 'Duplicate destination in migration manifest')
    for path, data, accepted, normalized in destinations:
        preflight_put(c, path, data, accepted, normalized_elf=normalized)
    game_dirs = [d for g in chosen for d in g.get('dirs', [])]
    for d in b.get('dirs', []) + game_dirs:
        parts = d.strip('/').split('/')
        for i in range(1, len(parts) + 1):
            st = c.stat('/' + '/'.join(parts[:i]))
            if st is None:
                break
            need(st['dir'], 'Expected a folder on the PS5: ' + d)
    for row, data in app_files:
        put(c, journal, row['remote'], data, accepts(row), normalized_elf=row.get('normalized_elf'))
    for d in b.get('dirs', []) + game_dirs:
        mkdirs(c, journal, d)
    for row in b.get('texts', []):
        data = row['text'].encode()
        put(c, journal, row['remote'], data, [(len(data), sha(data))])
    for row, data in metadata:
        put(c, journal, row['remote'], data, accepts(row), normalized_elf=row.get('normalized_elf'))
    for g in chosen:
        prior = journal.data['games'].get(g['id'], {})
        if prior.get('state') != 'adopted':
            journal.data['games'][g['id']] = dict(state='adopted', original_mode=original_modes[g['id']], utc=now())
    journal.save()
    say('Existing game files verified and preserved; app and metadata upgraded.')
    return 0


def install(args, ui):
    say(f'HelixSR GPU Lab {VERSION} installer\n')
    host = connect(args, ui)
    with CONSOLE(host) as c:
        c.running()   # pldmgr reachable, full list
        journal = Journal(host)
        need(not any(e.get('undone') or e.get('pending') or (e['op'] == 'put' and not e.get('done', True))
                     for e in journal.data['entries']) and
             not any(v.get('mode_restored') for v in journal.data['games'].values()),
             'An earlier operation was interrupted. Run uninstall to recover before installing again.')
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
        if args.adopt:
            return adopt(c, journal, chosen, b, ui)
        weights = ensure_weights(ui, args.dlss)
        built = {}
        for g in chosen:
            say(f"\n{g['name']}: preparing HelixSR files on this PC")
            inputs = gather_inputs(c, ui, g, args.game_files) if g.get('inputs') else {}
            built[g['id']] = build_files(g, inputs, weights)
            say('  built and checked against the tested build')
        app_files = []
        for row in b['files']:
            data = (ROOT / row['bundle']).read_bytes()
            need(sha(data) == row['sha256'], 'Damaged download (%s): download GPU Lab again' % row['bundle'])
            app_files.append((row, data))
        say('\nInstalling on the PS5.')
        wait_closed(c, ui)
        for g in chosen:
            if g['switch']['kind'] == 'overlay-files':
                st = overlay_state(c, g, complete=False)
                need(st != 'mixed', 'An interrupted switch must be completed first')
            if g['switch']['kind'] == 'folder':
                st = folder_state(c, g)
                need(st != 'both', f"{g['name']}: both an ON and an OFF copy exist on the PS5; run uninstall first")
                if st in ('on', 'off'):
                    verify_overlay(c, g, g['switch'][st], complete=False)
        off_games = [g for g in chosen if (g['switch']['kind'] == 'folder' and folder_state(c, g) == 'off') or
                     (g['switch']['kind'] == 'overlay-files' and overlay_state(c, g, complete=False) == 'off')]
        say('\nGPU Lab app:')
        for row, data in app_files:
            put(c, journal, row['remote'], data, accepts(row), normalized_elf=row.get('normalized_elf'))
        for d in b.get('dirs', []):
            mkdirs(c, journal, d)
        for row in b.get('texts', []):
            put(c, journal, row['remote'], row['text'].encode(), [(len(row['text']), sha(row['text'].encode()))])
        for g in chosen:
            say(f"\n{g['name']}:")
            target = FolderRedirect(c, g['switch']['on'], g['switch']['off']) if g in off_games else c
            for d in g.get('dirs', []):
                mkdirs(target, journal, d)
            for remote, (data, accept, kind) in built[g['id']].items():
                put(target, journal, remote, data, accept, kind)
            journal.data['games'][g['id']] = dict(state='installed', utc=now())
            journal.save()
        say('\nDone. On your PS5:')
        say('  1. Open HelixSR GPU Lab from the home screen (if it is not there yet, restart the PS5 and run your '
            'jailbreak and ShadowMount+ again).')
        for g in chosen:
            say(f"  - {g['name']}: {g['howto']}")
        say(f'\nTo remove everything again: run uninstall. Your journal: {journal.path}')
        return 0


def configured_mode(c, g, journal_entries):
    """Read the switch on the console even when this Mac did not install it."""
    s = g['switch']
    if s['kind'] == 'overlay-files':
        return overlay_state(c, g, allow_absent=True, journal_entries=journal_entries)
    if s['kind'] == 'folder':
        st = folder_state(c, g)
        if st in ('on', 'off'):
            verify_overlay(c, g, s[st], complete=True)
            if st == 'on':
                verify_support(c, g)
        return st
    if not c.exists(s['path']):
        return 'absent'
    cur = c.read(s['path'], 256)
    need(cur in (s['on'].encode(), s['off'].encode()), 'Unrecognized switch setting')
    return 'on' if cur == s['on'].encode() else 'off'


def status(args, ui):
    host = connect(args, ui)
    with CONSOLE(host) as c:
        running = c.running()
        journal = Journal(host)
        model = journal.data.get('model') or detect_model(c, ui, args.model)
        say(f"PS5 {host} ({bundle(model)['name']})" + (f"; running: {', '.join(running)}" if running else ''))
        rc = 0
        for g, version, ok, why in survey(c, model):
            line = f"  {g['name']}: {why}"
            if ok:
                try:
                    st = configured_mode(c, g, journal.data['entries'])
                    line += '; HelixSR ' + {'on': 'ON', 'off': 'OFF', 'absent': 'not configured',
                                            'both': 'DAMAGED (both copies exist)', 'mixed': 'switch interrupted'}[st]
                    if st in ('both', 'mixed'):
                        rc = 2
                except Refused as exc:
                    line += f'; HelixSR unverified ({exc})'
                    rc = 2
            say(line)
        return rc


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
        mappings = []
        for g in by_id.values():
            if g['switch']['kind'] != 'folder':
                continue
            st = folder_state(c, g)
            if st == 'both':
                raise Refused(f"{g['name']}: both ON and OFF folders exist; resolve this before uninstalling")
            if st == 'off':
                mappings.append((g['switch']['on'], g['switch']['off']))

        file_mappings = {}
        for g in by_id.values():
            if g['switch']['kind'] == 'overlay-files' and any(
                    e.get('path', '').startswith(g['switch']['on'] + '/') for e in entries):
                positions = overlay_positions(c, g, complete=False, journal_entries=entries)
                for rel, side in positions.items():
                    file_mappings[g['switch']['on'] + '/' + rel] = g['switch'][side] + '/' + rel
                for e in entries:
                    prior = e.get('prior') or {}
                    backup = prior.get('console', '')
                    if backup.startswith(g['switch']['on'] + '/'):
                        off_backup = g['switch']['off'] + backup[len(g['switch']['on']):]
                        if c.exists(off_backup):
                            file_mappings[backup] = off_backup

        def actual(path):
            if path in file_mappings:
                return file_mappings[path]
            for on, off in mappings:
                if path == on or path.startswith(on + '/'):
                    return off + path[len(on):]
            return path

        problems = []
        for e in entries:
            if e.get('undone') or e['op'] != 'put' or e.get('already'):
                continue
            path = actual(e['path'])
            prior = e.get('prior')
            backup = actual(prior['console']) if prior else None
            st = state_of(c, path, [tuple(a) for a in e['accept']], e.get('kind', 'file'), normalized_elf=e.get('normalized_elf'))
            if prior and c.exists(backup):
                bs = c.stat(backup)
                if bs['dir'] or bs['bytes'] != prior['bytes'] or c.hash(backup, prior['bytes'] + 1) != (prior['bytes'], prior['sha256']):
                    problems.append(backup)
            elif prior:
                if not c.exists(path) or c.stat(path)['dir'] or c.stat(path)['bytes'] != prior['bytes'] or c.hash(path, prior['bytes'] + 1) != (prior['bytes'], prior['sha256']):
                    problems.append(backup)
                continue
            if st == 'other':
                problems.append(path)
        if problems:
            raise Refused('These files changed on the PS5 after installing, so nothing was removed: ' +
                          ', '.join(problems))
        # Restore the mode recorded at adoption before reinstating older launch payloads.
        for gid, record in journal.data['games'].items():
            if record.get('state') != 'adopted':
                continue
            g = by_id.get(gid)
            need(g is not None and record.get('original_mode') in ('on', 'off'), 'Missing adopted game recovery data')
            if g['switch']['kind'] == 'overlay-files':
                overlay_positions(c, g)
            else:
                st = folder_state(c, g)
                need(st in ('on', 'off'), 'Adopted overlay state is ambiguous')
                verify_overlay(c, g, g['switch'][st], complete=True)
        for gid, record in journal.data['games'].items():
            if record.get('state') != 'adopted':
                continue
            switch(c, by_id[gid], record['original_mode'])
            record['mode_restored'] = True
            journal.save()
        # A restored mode may have moved managed files since preflight.
        for g in by_id.values():
            if g['switch']['kind'] == 'overlay-files' and any(
                    e.get('path', '').startswith(g['switch']['on'] + '/') for e in entries):
                for rel, side in overlay_positions(c, g, complete=False, journal_entries=entries).items():
                    file_mappings[g['switch']['on'] + '/' + rel] = g['switch'][side] + '/' + rel
        for e in reversed(entries):
            if e.get('undone'):
                continue
            path = actual(e['path'])
            if e['op'] == 'put' and not e.get('already'):
                prior = e.get('prior')
                backup = actual(prior['console']) if prior else None
                if not prior or c.exists(backup):
                    if c.exists(path):
                        c.delete(path)
                    if prior:
                        c.rename(backup, path)
                say(f"  removed {path}" + (' (your earlier file is back)' if prior else ''))
            elif e['op'] == 'mkdir' and not e.get('pending'):
                listing = c.listing(path)
                if listing is not None and not listing:
                    c.rmdir(path)
            e['undone'] = True
            journal.save()
    journal.data['games'] = {}
    journal.data['uninstalled_utc'] = now()
    journal.save()
    os.replace(journal.path, journal.path.with_name('journal-uninstalled-%d.json' % int(time.time())))
    say('Uninstall completed; recorded files and adopted game modes were restored. Empty directories may remain.')
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
            p.add_argument('target_game', metavar='GAME', help='control-r18, aw2-t12, saros-native-v1 or sotc-live3')
        if name == 'install':
            p.add_argument('--adopt', action='store_true', help='upgrade app and metadata using already installed verified game files')
            p.add_argument('--dlss', help="your own copy of NVIDIA's nvngx_dlss.dll (DLSS 310.7.0) instead of the download")
            p.add_argument('--game', action='append', help='only this game (control-r18, aw2-t12, saros-native-v1, sotc-live3)')
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
