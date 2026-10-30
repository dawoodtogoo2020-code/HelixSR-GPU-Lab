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
        self.assertEqual(self.run_cli('install', '--model', 'ps5')[0], 0)   # switches back ON, writes nothing new
        self.assertEqual(self.ps5.writes, writes)
        self.run_cli('off', 'demo')
        rc, out, _ = self.run_cli('uninstall')
        self.assertEqual(rc, 0, out)
        self.assertEqual(self.ps5.snapshot(), before)

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
