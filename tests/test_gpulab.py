# SPDX-License-Identifier: MIT
"""gpulab.py, helixsr_patch.py and helixsr_weights.py against a simulated PS5 with a synthetic game (no game, Sony or
NVIDIA data). The real packages are checked the same way, with real files, before each release."""
import hashlib
import io
import json
import lzma
import math
import random
import shutil
import struct
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path[:0] = [str(ROOT / 'tools'), str(ROOT / 'tests')]
import gpulab  # noqa: E402
import helixsr_patch  # noqa: E402
import helixsr_weights  # noqa: E402
from fake_ps5 import FakePS5  # noqa: E402

sha = lambda b: hashlib.sha256(b).hexdigest()
STOCK = b'stock game executable ' * 300
WEIGHTS = bytes(range(256)) * 8
SHADER = b'stock shader table ' * 50


def make_patch(sources, new_tail, hole):
    """A patch that copies every source, appends new bytes, then leaves a hole for the weights."""
    ops = bytearray()
    at = 0
    for _, data in sources:
        ops += bytes([helixsr_patch.COPY]) + struct.pack('<QQ', at, len(data))
        at += len(data)
    body = new_tail + bytes(len(hole))
    ops += bytes([helixsr_patch.ADD]) + struct.pack('<Q', len(body)) + body + bytes([helixsr_patch.END])
    target = b''.join(d for _, d in sources) + new_tail + hole
    header = dict(format='HXD1', sources=[dict(name=n, bytes=len(d), sha256=sha(d)) for n, d in sources],
                  target_bytes=len(target), target_sha256=sha(target), ops_sha256=sha(bytes(ops)), stats={},
                  holes=[dict(name='weights', offset=len(target) - len(hole), bytes=len(hole), sha256=sha(hole),
                              about='weights')] if hole else [])
    return helixsr_patch.pack(header, bytes(ops)), target


class Release:
    """A synthetic release folder: one game for the regular PS5 with a whole-folder switch."""
    def __init__(self, folder):
        self.root = folder
        (folder / 'games/demo/patches').mkdir(parents=True)
        (folder / 'console/ps5/PPSA99272').mkdir(parents=True)
        p1, self.eboot = make_patch([('eboot.bin', STOCK)], b'HELIX LOADER', b'')
        p2, self.data = make_patch([('shaders/a.binrfx', SHADER),
                                    ('data/big.rmdblob@4096+19', b'nineteen bytes here')], b'HELIX KERNELS', WEIGHTS)
        (folder / 'games/demo/patches/eboot.hxd').write_bytes(p1)
        (folder / 'games/demo/patches/data.hxd').write_bytes(p2)
        self.app = b'GPU Lab app'
        (folder / 'console/ps5/PPSA99272/eboot.bin').write_bytes(self.app)
        installed_app = b'GPU Lab app, as the console serves it'
        self.readback = {sha(self.app): installed_app}
        game = dict(
            id='demo', name='Demo Game', title='PPSA00001', version='01.000.000', models=['ps5'], about='', howto='play',
            param=dict(paths=['/user/appmeta/PPSA00001/param.json'], format='json', key='contentVersion'),
            inputs=[dict(name='eboot.bin', path='eboot.bin', accept=[[len(STOCK), sha(STOCK)]]),
                    dict(name='shaders/a.binrfx', path='shaders/a.binrfx', accept=[[len(SHADER), sha(SHADER)]]),
                    dict(name='data/big.rmdblob@4096+19', path='data/big.rmdblob', offset=4096,
                         accept=[[19, sha(b'nineteen bytes here')]])],
            files=[dict(remote='/data/homebrew/backports/PPSA00001/eboot.bin', patches=['games/demo/patches/eboot.hxd'],
                        sha256=sha(self.eboot), accept=[[len(self.eboot), sha(self.eboot)]]),
                   dict(remote='/data/homebrew/backports/PPSA00001/data/x.bin', patches=['games/demo/patches/data.hxd'],
                        sha256=sha(self.data), accept=[[len(self.data), sha(self.data)]]),
                   dict(remote='/data/helixsr-speed/packages/PPSA00001/READY', text='READY\n', sha256=sha(b'READY\n'),
                        accept=[[6, sha(b'READY\n')]])],
            dirs=['/data/helixsr-speed/packages/PPSA00001/stored'],
            switch=dict(kind='folder', on='/data/homebrew/backports/PPSA00001',
                        off='/data/helixsr-speed/packages/PPSA00001/stored/game', gpulab=True))
        (folder / 'games/demo/game.json').write_text(json.dumps(game))
        bundle = dict(model='ps5', name='PS5 (regular)', gpulab='test',
                      files=[dict(remote='/data/homebrew/PPSA99272/eboot.bin', bundle='console/ps5/PPSA99272/eboot.bin',
                                  sha256=sha(self.app), accept=[[len(self.app), sha(self.app)],
                                                                [len(installed_app), sha(installed_app)]])],
                      dirs=['/data/helixsr-speed/scan'], texts=[])
        (folder / 'console/ps5/console.json').write_text(json.dumps(bundle))


class User(gpulab.UI):
    def __init__(self, ps5):
        super().__init__(False)
        self.ps5, self.prompts = ps5, []

    def ask(self, question, default=''):
        return default

    def wait(self, text):
        self.prompts.append(text)
        self.ps5.game = 'PPSA00001' if text.startswith('Start ') else None


class GpuLabTest(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp(prefix='gpulab-test-'))
        self.release = Release(self.tmp / 'release')
        self.saved = (gpulab.ROOT, gpulab.GAMES, gpulab.CONSOLES, gpulab.HOME, gpulab.CONSOLE, gpulab.ensure_weights)
        gpulab.ROOT, gpulab.GAMES, gpulab.CONSOLES = self.release.root, self.release.root / 'games', \
            self.release.root / 'console'
        gpulab.HOME = self.tmp / 'my-ps5'
        gpulab.ensure_weights = lambda ui, dlss=None: WEIGHTS
        ps5 = self.ps5 = FakePS5()
        ps5.add_dir('/data/homebrew')
        ps5.add_file('/user/appmeta/PPSA00001/param.json', json.dumps({'contentVersion': '01.000.000'}).encode())
        ps5.add_file('/data/homebrew/PPSA99272/eboot.bin', b'an older GPU Lab')
        big = bytearray(8192)
        big[4096:4115] = b'nineteen bytes here'
        ps5.app0['PPSA00001'] = {'eboot.bin': STOCK, 'shaders/a.binrfx': SHADER, 'data/big.rmdblob': bytes(big)}
        ps5.readback.update(self.release.readback)
        gpulab.CONSOLE = lambda host: ps5

    def tearDown(self):
        gpulab.ROOT, gpulab.GAMES, gpulab.CONSOLES, gpulab.HOME, gpulab.CONSOLE, gpulab.ensure_weights = self.saved
        shutil.rmtree(self.tmp)

    def run_cli(self, *argv):
        user = User(self.ps5)
        out = io.StringIO()
        with redirect_stdout(out):
            rc = gpulab.main(list(argv) + ['--ps5', '10.0.0.9'], ui=user)
        return rc, out.getvalue(), user

    def test_install_switch_uninstall_restores_console(self):
        before = self.ps5.snapshot()
        rc, out, user = self.run_cli('install', '--model', 'ps5')
        self.assertEqual(rc, 0, out)
        self.assertEqual(self.ps5.files['/data/homebrew/backports/PPSA00001/eboot.bin'], self.release.eboot)
        self.assertEqual(self.ps5.files['/data/homebrew/backports/PPSA00001/data/x.bin'], self.release.data)
        self.assertEqual(self.ps5.files['/data/homebrew/PPSA99272/eboot.bin'], self.release.app)
        self.assertTrue(any(p.startswith('/data/homebrew/PPSA99272/eboot.bin.bak-') for p in self.ps5.files))
        self.assertEqual(len(user.prompts), 2)                     # start the game, then close it
        self.assertIn('HelixSR ON', self.run_cli('status', '--model', 'ps5')[1])
        self.assertEqual(self.run_cli('off', 'demo')[0], 0)
        self.assertIn('HelixSR OFF', self.run_cli('status', '--model', 'ps5')[1])
        writes = self.ps5.writes
        self.assertEqual(self.run_cli('install', '--model', 'ps5')[0], 0)
        self.assertEqual(self.ps5.writes, writes)
        self.assertIn('HelixSR OFF', self.run_cli('status', '--model', 'ps5')[1])
        self.run_cli('off', 'demo')
        rc, out, _ = self.run_cli('uninstall')
        self.assertEqual(rc, 0, out)
        self.assertEqual(self.ps5.snapshot(), before)

    def test_status_observes_folder_without_local_state(self):
        self.assertEqual(self.run_cli('install', '--model', 'ps5')[0], 0)
        self.assertEqual(self.run_cli('off', 'demo')[0], 0)
        shutil.rmtree(gpulab.HOME)
        before = self.ps5.snapshot()
        rc, out, _ = self.run_cli('status', '--model', 'ps5')
        self.assertEqual(rc, 0, out)
        self.assertIn('HelixSR OFF', out)
        self.assertEqual(self.ps5.snapshot(), before)
        self.assertFalse(gpulab.HOME.exists())

        g = gpulab.games()['demo']
        self.ps5.add_file(g['switch']['off'] + '/eboot.bin', b'unknown')
        rc, out, _ = self.run_cli('status', '--model', 'ps5')
        self.assertEqual(rc, 2, out)
        self.assertIn('unverified', out)

    def test_status_distinguishes_absent_complete_and_partial_file_overlays(self):
        g = self.overlay_game()
        deps = g['preserved_dependencies']
        for dep, content in zip(deps, (b'library a', b'library b', b'library c')):
            self.ps5.add_file(dep['remote'], content)
        for expected_rc, text, added in (
                (0, 'not configured', None),
                (2, 'unverified', ('on', 'eboot.bin', self.release.eboot)),
                (2, 'switch interrupted', ('off', 'data/x.bin', self.release.data))):
            if added:
                side, name, data = added
                self.ps5.add_file(g['switch'][side] + '/' + name, data)
            rc, out, _ = self.run_cli('status', '--model', 'ps5')
            self.assertEqual(rc, expected_rc, out)
            self.assertIn(text, out)
        self.assertFalse(gpulab.HOME.exists())

    def test_wrong_game_version_changes_nothing(self):
        self.ps5.files['/user/appmeta/PPSA00001/param.json'] = b'{"contentVersion": "01.010.000"}'
        before = self.ps5.snapshot()
        rc, out, _ = self.run_cli('install', '--model', 'ps5')
        self.assertEqual(rc, 1)
        self.assertIn('needs 01.000.000', out)
        self.assertEqual(self.ps5.snapshot(), before)

    def test_other_copy_of_game_file_changes_nothing(self):
        self.ps5.app0['PPSA00001']['shaders/a.binrfx'] = b'modified'
        before = self.ps5.snapshot()
        rc, out, _ = self.run_cli('install', '--model', 'ps5')
        self.assertEqual(rc, 2)
        self.assertIn('not the supported version', out)
        self.assertEqual(self.ps5.snapshot(), before)

    def test_foreign_backport_folder_is_refused(self):
        self.ps5.add_file('/data/homebrew/backports/PPSA00001/fakelib/libSomething.sprx', b'not ours')
        rc, out, _ = self.run_cli('install', '--model', 'ps5')
        self.assertEqual(rc, 2)
        self.assertIn('other files', out)
        self.assertNotIn('/data/homebrew/backports/PPSA00001/eboot.bin', self.ps5.files)

    def test_uninstall_refuses_changed_files(self):
        self.run_cli('install', '--model', 'ps5')
        self.ps5.files['/data/homebrew/backports/PPSA00001/data/x.bin'] = b'changed by someone'
        rc, out, _ = self.run_cli('uninstall')
        self.assertEqual(rc, 2)
        self.assertIn('changed on the PS5', out)

    def test_off_reinstall_refusal_does_not_move_folder(self):
        self.assertEqual(self.run_cli('install', '--model', 'ps5')[0], 0)
        self.assertEqual(self.run_cli('off', 'demo')[0], 0)
        g = gpulab.games()['demo']
        self.ps5.add_file(g['switch']['off'] + '/foreign.bin', b'other')
        before = self.ps5.snapshot()
        rc, out, _ = self.run_cli('install', '--model', 'ps5')
        self.assertEqual(rc, 2, out)
        self.assertEqual(self.ps5.snapshot(), before)

    def test_bad_bundle_does_not_switch_off_overlay(self):
        self.assertEqual(self.run_cli('install', '--model', 'ps5')[0], 0)
        self.assertEqual(self.run_cli('off', 'demo')[0], 0)
        (self.release.root / 'console/ps5/PPSA99272/eboot.bin').write_bytes(b'damaged')
        before = self.ps5.snapshot()
        rc, out, _ = self.run_cli('install', '--model', 'ps5')
        self.assertEqual(rc, 2, out)
        self.assertEqual(self.ps5.snapshot(), before)

    def test_off_uninstall_refusal_does_not_move_folder(self):
        self.assertEqual(self.run_cli('install', '--model', 'ps5')[0], 0)
        self.assertEqual(self.run_cli('off', 'demo')[0], 0)
        g = gpulab.games()['demo']
        self.ps5.files[g['switch']['off'] + '/data/x.bin'] = b'changed'
        before = self.ps5.snapshot()
        rc, out, _ = self.run_cli('uninstall')
        self.assertEqual(rc, 2, out)
        self.assertEqual(self.ps5.snapshot(), before)

    def test_interrupted_backup_rename_can_be_uninstalled(self):
        before = self.ps5.snapshot()
        rename = self.ps5.rename
        def fail_after(old, new):
            rename(old, new)
            if '.bak-' in new:
                raise gpulab.Refused('injected interruption')
        self.ps5.rename = fail_after
        self.assertEqual(self.run_cli('install', '--model', 'ps5')[0], 2)
        self.ps5.rename = rename
        self.assertEqual(self.run_cli('install', '--model', 'ps5')[0], 2)
        rc, out, _ = self.run_cli('uninstall')
        self.assertEqual(rc, 0, out)
        self.assertEqual(self.ps5.snapshot(), before)

    def test_interrupted_restore_can_be_retried(self):
        before = self.ps5.snapshot()
        self.assertEqual(self.run_cli('install', '--model', 'ps5')[0], 0)
        rename = self.ps5.rename
        def fail_after(old, new):
            rename(old, new)
            if '.bak-' in old:
                raise gpulab.Refused('injected interruption')
        self.ps5.rename = fail_after
        self.assertEqual(self.run_cli('uninstall')[0], 2)
        self.ps5.rename = rename
        rc, out, _ = self.run_cli('uninstall')
        self.assertEqual(rc, 0, out)
        self.assertEqual(self.ps5.snapshot(), before)

    def test_failed_reinstall_keeps_overlay_off(self):
        self.assertEqual(self.run_cli('install', '--model', 'ps5')[0], 0)
        self.assertEqual(self.run_cli('off', 'demo')[0], 0)
        g = gpulab.games()['demo']
        # Force a write to fail after preflight and after the app has been checked.
        self.ps5.files[g['switch']['off'] + '/data/x.bin'] = b'old'
        write = self.ps5.write
        def fail_write(path, data):
            if path.endswith('/data/x.bin'):
                raise gpulab.Refused('injected write failure')
            write(path, data)
        self.ps5.write = fail_write
        rc, out, _ = self.run_cli('install', '--model', 'ps5')
        self.assertEqual(rc, 2, out)
        self.assertEqual(gpulab.folder_state(self.ps5, g), 'off')
        self.ps5.write = write
        self.assertEqual(self.run_cli('uninstall')[0], 2)
        self.ps5.files[g['switch']['off'] + '/data/x.bin'] = self.release.data
        self.assertEqual(self.run_cli('uninstall')[0], 0)

    def test_interrupted_upload_refuses_foreign_replacement(self):
        write = self.ps5.write
        def fail_write(path, data):
            if path.endswith('/eboot.bin') and '/backports/' in path:
                self.ps5.files[path] = b'foreign replacement'
                raise gpulab.Refused('injected interruption')
            write(path, data)
        self.ps5.write = fail_write
        self.assertEqual(self.run_cli('install', '--model', 'ps5')[0], 2)
        self.ps5.write = write
        before = self.ps5.snapshot()
        rc, out, _ = self.run_cli('uninstall')
        self.assertEqual(rc, 2, out)
        self.assertEqual(self.ps5.snapshot(), before)
        self.assertIn('changed on the PS5', out)

    def test_pending_mkdir_does_not_remove_uncertain_directory(self):
        path = '/data/preexisting-empty'
        self.ps5.add_dir(path)
        journal = gpulab.Journal('10.0.0.9')
        journal.add(op='mkdir', path=path, pending=True)
        rc, out, _ = self.run_cli('uninstall')
        self.assertEqual(rc, 0, out)
        self.assertIn(path, self.ps5.dirs)

    def test_switch_refuses_foreign_files(self):
        self.assertEqual(self.run_cli('install', '--model', 'ps5')[0], 0)
        g = gpulab.games()['demo']
        self.ps5.add_file(g['switch']['on'] + '/foreign.bin', b'other')
        before = self.ps5.snapshot()
        self.assertEqual(self.run_cli('off', 'demo')[0], 2)
        self.assertEqual(self.ps5.snapshot(), before)

    def test_switch_refuses_backup_named_foreign_file(self):
        self.assertEqual(self.run_cli('install', '--model', 'ps5')[0], 0)
        g = gpulab.games()['demo']
        self.ps5.add_file(g['switch']['on'] + '/foreign.bak-user', b'user')
        before = self.ps5.snapshot()
        self.assertEqual(self.run_cli('off', 'demo')[0], 2)
        self.assertEqual(self.ps5.snapshot(), before)

    def test_switch_refuses_damaged_or_missing_expected_files(self):
        self.assertEqual(self.run_cli('install', '--model', 'ps5')[0], 0)
        self.assertEqual(self.run_cli('off', 'demo')[0], 0)
        g = gpulab.games()['demo']
        path = g['switch']['off'] + '/eboot.bin'
        self.ps5.files[path] = b'damaged'
        before = self.ps5.snapshot()
        self.assertEqual(self.run_cli('on', 'demo')[0], 2)
        self.assertEqual(self.ps5.snapshot(), before)
        del self.ps5.files[path]
        self.assertEqual(self.run_cli('on', 'demo')[0], 2)

    def test_unknown_overlay_cannot_be_overwritten_on_install(self):
        g = gpulab.games()['demo']
        self.ps5.add_file(g['switch']['on'] + '/eboot.bin', b'foreign')
        before = self.ps5.snapshot()
        self.assertEqual(self.run_cli('install', '--model', 'ps5')[0], 2)
        self.assertEqual(self.ps5.snapshot(), before)

    def test_unavailable_overlay_listing_refuses_switch(self):
        self.assertEqual(self.run_cli('install', '--model', 'ps5')[0], 0)
        g = gpulab.games()['demo']
        listing = self.ps5.listing
        self.ps5.listing = lambda p: None if p == g['switch']['on'] else listing(p)
        self.assertEqual(self.run_cli('off', 'demo')[0], 2)

    def test_setting_changed_is_preserved_on_uninstall(self):
        self.assertEqual(self.run_cli('install', '--model', 'ps5')[0], 0)
        path = '/data/helixsr-speed/custom-setting'
        self.ps5.add_file(path, b'custom')
        j = gpulab.Journal('10.0.0.9')
        j.add(op='put', path=path, kind='setting', accept=[[2, sha(b'on')], [3, sha(b'off')]],
              prior=None, already=False, done=True)
        before = self.ps5.snapshot()
        self.assertEqual(self.run_cli('uninstall')[0], 2)
        self.assertEqual(self.ps5.snapshot(), before)

    def test_aw2_support_is_required_only_on_activation(self):
        self.assertEqual(self.run_cli('install', '--model', 'ps5')[0], 0)
        g = gpulab.games()['demo']
        g['id'] = 'aw2-t12'
        for name in ('copy.bin', 'chain.bin'):
            path = '/data/helixsr-aw2-t12/' + name
            g['files'].append(dict(remote=path, accept=[[2, sha(b'ok')]]))
            self.ps5.add_file(path, b'ok')
        self.assertIn('OFF', gpulab.switch(self.ps5, g, 'off'))
        self.ps5.files['/data/helixsr-aw2-t12/copy.bin'] = b'bad'
        with self.assertRaises(gpulab.Refused): gpulab.switch(self.ps5, g, 'on')
        self.assertIn('already OFF', gpulab.switch(self.ps5, g, 'off'))
        self.ps5.files['/data/helixsr-aw2-t12/copy.bin'] = b'ok'
        self.assertIn('ON', gpulab.switch(self.ps5, g, 'on'))
    def test_normalized_elf_accepts_only_pinned_bytes_and_zero_tail(self):
        elf = bytearray(b'\x7fELF\x02\x01' + b'Q' * 94)
        normalized = bytearray(elf)
        normalized[40:48] = bytes(8); normalized[58:64] = bytes(6)
        spec = dict(bytes=len(elf), sha256=sha(normalized), zero_ranges=[[40, 48], [58, 64]], max_zero_tail=75)
        path = '/data/test.elf'
        self.ps5.add_file(path, bytes(elf) + bytes(75))
        self.assertEqual(gpulab.state_of(self.ps5, path, [(1, sha(b'x'))], normalized_elf=spec), 'ours')
        for data in (bytes(elf) + bytes(76), bytes(elf) + b'X', bytes(elf[:70]) + b'X' + bytes(elf[71:])):
            self.ps5.files[path] = data
            self.assertEqual(gpulab.state_of(self.ps5, path, [(1, sha(b'x'))], normalized_elf=spec), 'other')
        self.ps5.files[path] = bytes(elf)
        bad = dict(spec, zero_ranges=[[0, 8]])
        with self.assertRaises(gpulab.Refused):
            gpulab.state_of(self.ps5, path, [(1, sha(b'x'))], normalized_elf=bad)

    def existing_aw2_for_adoption(self):
        self.assertEqual(self.run_cli('install', '--model', 'ps5')[0], 0)
        p = self.release.root / 'games/demo/game.json'
        g = json.loads(p.read_text())
        g['id'] = 'aw2-t12'
        package = '/data/helixsr-speed/packages/PPSA02572-01.200.007'
        g['switch']['off'] = package + '/stored/game'
        g['dirs'] = [package + '/stored']
        for name, data in (('copy.bin', b'copy support'), ('chain.bin', b'chain support')):
            path = '/data/helixsr-aw2-t12/' + name
            self.ps5.add_file(path, data)
            g['files'].append(dict(remote=path, text=data.decode(), sha256=sha(data), accept=[[len(data), sha(data)]]))
        p.write_text(json.dumps(g))
        shutil.rmtree(gpulab.HOME)
        self.ps5.files['/data/homebrew/PPSA99272/eboot.bin'] = b'prior app'
        return g, package

    def test_adopt_creates_missing_stored_directory_without_touching_overlay(self):
        g, package = self.existing_aw2_for_adoption()
        before = {p: v for p, v in self.ps5.files.items() if p.startswith(g['switch']['on'] + '/')}
        self.assertNotIn(package + '/stored', self.ps5.dirs)
        rc, out, _ = self.run_cli('install', '--model', 'ps5', '--adopt', '--game', 'aw2-t12')
        self.assertEqual(rc, 0, out)
        self.assertIn(package + '/stored', self.ps5.dirs)
        self.assertEqual(before, {p: v for p, v in self.ps5.files.items() if p.startswith(g['switch']['on'] + '/')})
        self.assertEqual(gpulab.folder_state(self.ps5, g), 'on')
        journal = gpulab.Journal('10.0.0.9')
        self.assertTrue(any(e['op'] == 'mkdir' and e['path'] == package + '/stored' and not e.get('pending')
                            for e in journal.data['entries']))

    def test_adopt_conflicting_game_directory_refuses_before_app_write(self):
        g, package = self.existing_aw2_for_adoption()
        self.ps5.add_file(package + '/stored', b'not a directory')
        before = self.ps5.snapshot()
        rc, out, _ = self.run_cli('install', '--model', 'ps5', '--adopt', '--game', 'aw2-t12')
        self.assertEqual(rc, 2, out)
        self.assertEqual(self.ps5.snapshot(), before)

    def test_adopt_fresh_journal_uninstall_preserves_preexisting_overlay(self):
        self.assertEqual(self.run_cli('install', '--model', 'ps5')[0], 0)
        g = self.overlay_game()
        # Simulate an existing installation made by another PC, with no local journal.
        shutil.rmtree(gpulab.HOME)
        before = {p: v for p, v in self.ps5.files.items() if p.startswith(g['switch']['on'] + '/')}
        rc, out, _ = self.run_cli('install', '--model', 'ps5', '--adopt', '--game', 'control-r18')
        self.assertEqual(rc, 0, out)
        self.assertEqual(self.run_cli('uninstall')[0], 0)
        self.assertEqual(before, {p: v for p, v in self.ps5.files.items() if p.startswith(g['switch']['on'] + '/')})

    def check_adoption_uninstall_mode(self, initial):
        self.assertEqual(self.run_cli('install', '--model', 'ps5')[0], 0)
        g = self.overlay_game()
        for dep, data in zip(g['preserved_dependencies'], (b'library a', b'library b', b'library c')):
            self.ps5.add_file(dep['remote'], data)
        if initial == 'off':
            self.assertEqual(self.run_cli('off', 'control-r18')[0], 0)
        original = {p: v for p, v in self.ps5.files.items() if p.startswith(g['switch']['on'] + '/') or p.startswith(g['switch']['off'] + '/')}
        shutil.rmtree(gpulab.HOME)
        app = '/data/homebrew/PPSA99272/eboot.bin'
        self.ps5.files[app] = b'older app for migration'
        rc, out, _ = self.run_cli('install', '--model', 'ps5', '--adopt', '--game', 'control-r18')
        self.assertEqual(rc, 0, out)
        self.assertEqual(self.run_cli('off' if initial == 'on' else 'on', 'control-r18')[0], 0)
        rc, out, _ = self.run_cli('uninstall')
        self.assertEqual(rc, 0, out)
        self.assertEqual(gpulab.overlay_state(self.ps5, g), initial)
        self.assertEqual(self.ps5.files[app], b'older app for migration')
        self.assertEqual(original, {p: v for p, v in self.ps5.files.items() if p.startswith(g['switch']['on'] + '/') or p.startswith(g['switch']['off'] + '/')})

    def test_adopt_on_restore_on(self):
        self.check_adoption_uninstall_mode('on')

    def test_adopt_off_restore_off(self):
        self.check_adoption_uninstall_mode('off')

    def test_uninstall_retries_interrupted_mode_restoration_before_app_restore(self):
        self.assertEqual(self.run_cli('install', '--model', 'ps5')[0], 0)
        g = self.overlay_game()
        shutil.rmtree(gpulab.HOME)
        app = '/data/homebrew/PPSA99272/eboot.bin'
        self.ps5.files[app] = b'previous app'
        self.assertEqual(self.run_cli('install', '--model', 'ps5', '--adopt', '--game', 'control-r18')[0], 0)
        self.assertEqual(self.run_cli('off', 'control-r18')[0], 0)
        rename = self.ps5.rename
        def fail_after(old, new):
            rename(old, new)
            if old.startswith(g['switch']['off'] + '/'):
                raise gpulab.Refused('injected interruption')
        self.ps5.rename = fail_after
        self.assertEqual(self.run_cli('uninstall')[0], 2)
        self.assertEqual(self.ps5.files[app], self.release.app)
        self.ps5.rename = rename
        self.assertEqual(self.run_cli('uninstall')[0], 0)
        self.assertEqual(gpulab.overlay_state(self.ps5, g), 'on')
        self.assertEqual(self.ps5.files[app], b'previous app')

    def test_repeated_adopt_retains_first_mode_for_uninstall(self):
        self.assertEqual(self.run_cli('install', '--model', 'ps5')[0], 0)
        g = self.overlay_game()
        shutil.rmtree(gpulab.HOME)
        self.assertEqual(self.run_cli('install', '--model', 'ps5', '--adopt', '--game', 'control-r18')[0], 0)
        self.assertEqual(self.run_cli('off', 'control-r18')[0], 0)
        self.assertEqual(self.run_cli('install', '--model', 'ps5', '--adopt', '--game', 'control-r18')[0], 0)
        self.assertEqual(self.run_cli('uninstall')[0], 0)
        self.assertEqual(gpulab.overlay_state(self.ps5, g), 'on')

    def test_adoption_preflights_later_backup_collision(self):
        self.assertEqual(self.run_cli('install', '--model', 'ps5')[0], 0)
        self.overlay_game()
        shutil.rmtree(gpulab.HOME)
        p = self.release.root / 'console/ps5/console.json'
        bundle = json.loads(p.read_text())
        row = dict(bundle['files'][0], remote='/data/homebrew/other/eboot.bin')
        bundle['files'].append(row);p.write_text(json.dumps(bundle))
        old = b'previous second app'
        self.ps5.add_file(row['remote'], old)
        self.ps5.add_file(row['remote'] + '.bak-' + sha(old)[:8], b'occupied')
        self.ps5.files[bundle['files'][0]['remote']] = b'first old app'
        before = self.ps5.snapshot()
        rc, out, _ = self.run_cli('install', '--model', 'ps5', '--adopt', '--game', 'control-r18')
        self.assertEqual(rc, 2, out)
        self.assertEqual(self.ps5.snapshot(), before)

    def test_recovery_allows_only_exact_journal_overlay_backup(self):
        self.assertEqual(self.run_cli('install', '--model', 'ps5')[0], 0)
        g = self.overlay_game()
        shutil.rmtree(gpulab.HOME)
        path = g['switch']['on'] + '/eboot.bin'
        old = b'earlier original overlay'
        backup = path + '.bak-' + sha(old)[:8]
        self.ps5.add_file(backup, old)
        j = gpulab.Journal('10.0.0.9')
        row = g['files'][0]
        j.add(op='put', path=path, accept=row['accept'], prior=dict(console=backup, sha256=sha(old), bytes=len(old), pc='unused'), already=False, done=True)
        self.assertEqual(self.run_cli('uninstall')[0], 0)
        self.assertEqual(self.ps5.files[path], old)
        self.assertNotIn(backup, self.ps5.files)

    def test_unowned_backup_in_overlay_is_refused_before_mutation(self):
        self.assertEqual(self.run_cli('install', '--model', 'ps5')[0], 0)
        g = self.overlay_game()
        self.ps5.add_file(g['switch']['on'] + '/eboot.bin.bak-unknown', b'foreign')
        before = self.ps5.snapshot()
        self.assertEqual(self.run_cli('uninstall')[0], 2)
        self.assertEqual(self.ps5.snapshot(), before)

    def test_missing_console_side_backup_refuses_uninstall(self):
        self.assertEqual(self.run_cli('install', '--model', 'ps5')[0], 0)
        g = self.overlay_game()
        shutil.rmtree(gpulab.HOME)
        app = '/data/homebrew/PPSA99272/eboot.bin'
        old = b'older app for recovery'
        self.ps5.files[app] = old
        self.assertEqual(self.run_cli('install', '--model', 'ps5', '--adopt', '--game', 'control-r18')[0], 0)
        del self.ps5.files[app + '.bak-' + sha(old)[:8]]
        before = self.ps5.snapshot()
        self.assertEqual(self.run_cli('uninstall')[0], 2)
        self.assertEqual(self.ps5.snapshot(), before)

    def overlay_game(self):
        p = self.release.root / 'games/demo/game.json'
        g = json.loads(p.read_text())
        g['id'] = 'control-r18'
        g['switch']['kind'] = 'overlay-files'
        root = g['switch']['on']
        g['preserved_dependencies'] = [dict(remote=root + '/fakelib2/' + n, bytes=len(b), sha256=sha(b))
                                       for n, b in [('a.sprx', b'library a'), ('b.sprx', b'library b'), ('c.sprx', b'library c')]]
        p.write_text(json.dumps(g))
        return g

    def test_overlay_files_preserves_dependencies_and_recovers_mixed(self):
        self.assertEqual(self.run_cli('install', '--model', 'ps5')[0], 0)
        g = self.overlay_game()
        for dep, content in zip(g['preserved_dependencies'], (b'library a', b'library b', b'library c')):
            self.ps5.add_file(dep['remote'], content)
        rename = self.ps5.rename
        count = [0]
        def fail_after(old, new):
            rename(old, new)
            count[0] += 1
            if count[0] == 1:
                raise gpulab.Refused('injected failure')
        self.ps5.rename = fail_after
        self.assertEqual(self.run_cli('off', 'control-r18')[0], 2)
        self.ps5.rename = rename
        self.assertEqual(gpulab.overlay_state(self.ps5, g), 'mixed')
        self.assertEqual(self.run_cli('off', 'control-r18')[0], 0)
        self.assertEqual(gpulab.overlay_state(self.ps5, g), 'off')
        for dep, content in zip(g['preserved_dependencies'], (b'library a', b'library b', b'library c')):
            self.assertEqual(self.ps5.files[dep['remote']], content)
        self.assertEqual(self.run_cli('on', 'control-r18')[0], 0)
        self.assertEqual(gpulab.overlay_state(self.ps5, g), 'on')

    def test_overlay_refuses_partial_or_foreign_dependencies_without_changes(self):
        self.assertEqual(self.run_cli('install', '--model', 'ps5')[0], 0)
        g = self.overlay_game()
        self.ps5.add_file(g['preserved_dependencies'][0]['remote'], b'library a')
        before = self.ps5.snapshot()
        self.assertEqual(self.run_cli('off', 'control-r18')[0], 2)
        self.assertEqual(self.ps5.snapshot(), before)
        for dep, content in zip(g['preserved_dependencies'][1:], (b'library b', b'library c')):
            self.ps5.add_file(dep['remote'], content)
        self.ps5.add_file(g['switch']['on'] + '/fakelib2/extra.sprx', b'foreign')
        before = self.ps5.snapshot()
        self.assertEqual(self.run_cli('off', 'control-r18')[0], 2)
        self.assertEqual(self.ps5.snapshot(), before)

    def test_adopt_keeps_game_bytes_and_never_reads_inputs_or_weights(self):
        self.assertEqual(self.run_cli('install', '--model', 'ps5')[0], 0)
        g = self.overlay_game()
        original = {p: v for p, v in self.ps5.files.items() if p.startswith(g['switch']['on'] + '/')}
        gpulab.ensure_weights = lambda *a: self.fail('weights requested during adoption')
        self.ps5.app0.clear()
        rc, out, _ = self.run_cli('install', '--model', 'ps5', '--adopt', '--game', 'control-r18')
        self.assertEqual(rc, 0, out)
        self.assertEqual(original, {p: v for p, v in self.ps5.files.items() if p.startswith(g['switch']['on'] + '/')})



class ConsoleListingTest(unittest.TestCase):
    def test_ftpsrv_226_delete_requires_confirmed_absence(self):
        import ftplib
        from ps5_console import Console, Refused
        c = Console('example')
        c.gate = lambda: None

        class FTP:
            reply = '226 File deleted'
            def delete(self, path):
                raise ftplib.error_reply(self.reply)

        c.ftp = FTP()
        c.exists = lambda path: False
        c.delete('/removed')
        c.exists = lambda path: True
        with self.assertRaises(Refused):
            c.delete('/still-present')
        c.ftp.reply = '350 Incomplete'
        with self.assertRaises(ftplib.error_reply):
            c.delete('/still-present')

    def test_existing_parent_unavailable_is_not_treated_as_absent(self):
        from ps5_console import Console, Refused
        c = Console('example')
        def listing(path):
            return {'data': {'dir': True, 'bytes': 0}} if path == '/' else None
        c.listing = listing
        with self.assertRaises(Refused): c.stat('/data/file')
        self.assertIsNone(c.stat('/missing/file'))

    def test_refuses_unparseable_or_symlink_entries(self):
        from ps5_console import Console, Refused
        class FTP:
            def __init__(self, line): self.line = line
            def retrlines(self, cmd, callback): callback(self.line)
        for line in ('not a listing', 'lrwxrwxrwx 1 user group 2 Jan 01 00:00 eboot.bin -> /else'):
            c = Console('example'); c.ftp = FTP(line)
            with self.assertRaises(Refused): c.listing('/data/example')



class PatchTest(unittest.TestCase):
    def test_apply_checks_sources_holes_and_result(self):
        blob, target = make_patch([('a', b'hello ' * 20)], b'world', b'W' * 32)
        header, ops = helixsr_patch.parse_patch(blob)
        self.assertEqual(helixsr_patch.apply(header, ops, {'a': b'hello ' * 20}, {'weights': b'W' * 32}), target)
        with self.assertRaises(SystemExit):
            helixsr_patch.apply(header, ops, {'a': b'other'}, {'weights': b'W' * 32})
        with self.assertRaises(SystemExit):
            helixsr_patch.apply(header, ops, {'a': b'hello ' * 20}, {'weights': b'X' * 32})
        with self.assertRaises(SystemExit):
            helixsr_patch.parse_patch(blob[:-8] + lzma.compress(b'damaged')[:8])


class WeightsTest(unittest.TestCase):
    def test_fp16_round_toward_zero(self):
        rng = random.Random(7)
        values = [0.0, -0.0, 1.0, -1.0, 65504.0, 65519.9, 65520.0, 1e9, -1e9, 6.1e-5, 6.0e-8, 5.0e-9, 3.14159, -2.71828]
        values += [rng.uniform(-70000, 70000) * 10 ** rng.randint(-9, 0) for _ in range(5000)]
        for x in values:
            x32 = struct.unpack('<f', struct.pack('<f', x))[0]
            bits = struct.unpack('<I', struct.pack('<f', x32))[0]
            h = struct.unpack('<e', struct.pack('<H', helixsr_weights.f16_rtz(bits)))[0]
            self.assertLessEqual(abs(h), abs(x32), x)                  # never rounds away from zero
            if abs(x32) <= 65504:
                nxt = abs(h) + (2.0 ** -24 if abs(h) < 2 ** -14 else 2.0 ** (math.floor(math.log2(abs(h))) - 10)
                                if h else 2.0 ** -24)
                self.assertLess(abs(x32), nxt, x)                      # and is the largest such fp16
            else:
                self.assertEqual(abs(h), 65504.0)
            self.assertTrue(h == 0 or math.copysign(1, h) == math.copysign(1, x32))

    def test_tables_match_the_canonical_layout(self):
        import helixsr_model_tables as T
        self.assertEqual(len(T.TENSORS), len(T.FINGERPRINTS))
        self.assertEqual(sum(2 * O * I * k * k for _, O, I, k, _ in T.TENSORS) <= T.CANON_BYTES, True)
        self.assertEqual(T.NGX_BYTES, T.CANON_BYTES)
        for blob_off, size, canon_off, O, I, K, layout in T.NGX_ROWS:
            self.assertLessEqual(blob_off + size, T.NGX_BYTES)
            self.assertEqual(size, 2 * O * I * K * K)


if __name__ == '__main__':
    unittest.main()
