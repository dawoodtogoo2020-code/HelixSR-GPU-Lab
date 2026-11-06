# SPDX-License-Identifier: MIT
import argparse
import contextlib
import io
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import gpulab as g
from fake_ps5 import FakePS5
from lite1160 import setup


class LiteSetup(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.home = patch.object(g, 'HOME', Path(self.temp.name))
        self.home.start()
        self.addCleanup(self.home.stop)
        self.c = FakePS5()
        self.c.add_dir(setup.PAYLOAD_ROOT + '/local')
        self.c.add_dir(setup.PAYLOAD_ROOT + '/repo')
        self.output = setup.PAYLOAD_ROOT + '/' + setup.SLOT + '/' + setup.FILENAME

    def test_discovery_requires_full_hash_in_known_payload_slots(self):
        data = b'owned-payload'
        self.c.add_file(setup.PAYLOAD_ROOT + '/local/one/wrong.elf', b'x' * len(data))
        self.c.add_file('/elsewhere/good.elf', data)
        self.assertIsNone(setup.find_input(self.c, len(data), g.sha(data)))
        self.c.add_file(setup.PAYLOAD_ROOT + '/repo/two/good.elf', data)
        self.assertEqual(setup.find_input(self.c, len(data), g.sha(data)), data)
        self.assertEqual(self.c.writes, 0)

    def test_running_game_refuses_before_inputs_or_writes(self):
        self.c.game = 'PPSA12345'
        before = self.c.snapshot()
        with patch.object(setup, 'acquire') as acquire:
            with self.assertRaises(g.Refused):
                setup.install(self.c, g.Journal('console'), g.UI(True), g)
            acquire.assert_not_called()
        self.assertEqual(self.c.snapshot(), before)

    def test_unknown_local_input_does_not_change_console(self):
        bad = Path(self.temp.name) / 'bad.elf'
        bad.write_bytes(b'wrong build')
        before = self.c.snapshot()
        with self.assertRaises(g.Refused):
            setup.install(self.c, g.Journal('console'), g.UI(True), g, test5=bad)
        self.assertEqual(self.c.snapshot(), before)

    def install_fixture(self):
        payload = b'fixture-combined-elf'
        bundle = Path(self.temp.name) / setup.FILENAME
        bundle.write_bytes(payload)
        with patch.object(setup, 'BUNDLED_FILE', bundle), \
             patch.object(setup, 'OUTPUT_BYTES', len(payload)), \
             patch.object(setup, 'OUTPUT_SHA256', g.sha(payload)):
            return setup.install(self.c, g.Journal('console'), g.UI(True), g)

    def test_bundled_install_needs_no_inputs_build_or_prompt(self):
        with patch.object(setup, 'acquire', side_effect=AssertionError('must not search for Test5')), \
             patch.object(setup.integrate, 'build', side_effect=AssertionError('must not build')), \
             patch.object(g.UI, 'ask', side_effect=AssertionError('must not ask for inputs')):
            result = self.install_fixture()
        self.assertEqual(result['payload_source'], 'bundled release')
        self.assertEqual(self.c.files[self.output], b'fixture-combined-elf')
        self.assertFalse((g.Journal('console').dir / 'lite1160' / setup.FILENAME).exists())

    def test_missing_or_corrupt_bundle_refuses_before_console_writes(self):
        bundle = Path(self.temp.name) / setup.FILENAME
        before = self.c.snapshot()
        with patch.object(setup, 'BUNDLED_FILE', bundle), \
             patch.object(setup, 'acquire', side_effect=AssertionError('must not request Test5')):
            with self.assertRaises(g.Refused):
                setup.install(self.c, g.Journal('console'), g.UI(True), g)
            bundle.write_bytes(b'wrong ELF')
            with self.assertRaises(g.Refused):
                setup.install(self.c, g.Journal('console'), g.UI(True), g)
        self.assertEqual(self.c.snapshot(), before)

    def test_explicit_developer_rebuild_remains_available(self):
        payload = b'rebuilt-fixture'
        with patch.object(setup, 'acquire', side_effect=[b'reference', b'lite']), \
             patch.object(setup.integrate, 'build', return_value=(payload, {})), \
             patch.object(setup, 'OUTPUT_SHA256', g.sha(payload)):
            result = setup.install(self.c, g.Journal('console'), g.UI(True), g,
                                   test5='explicit-reference.elf', lite='explicit-lite.elf')
        self.assertEqual(result['payload_source'], 'explicit local rebuild')
        self.assertEqual((g.Journal('console').dir / 'lite1160' / setup.FILENAME).read_bytes(), payload)

    def uninstall(self):
        with patch.object(g, 'CONSOLE', return_value=self.c):
            return g.uninstall(argparse.Namespace(cmd='uninstall', ps5='console'), g.UI(True))

    def test_install_is_repeatable_and_normal_uninstall_restores_originals(self):
        original = b'user custom payload'
        self.c.add_file(self.output, original)
        config = '/data/pldmgr/config.ini'
        self.c.add_file(config, b'AUTOLOAD_ENABLED=false\nFAVORITES=existing')
        before = self.c.snapshot()
        result = self.install_fixture()
        self.assertFalse(result['injected'])
        writes = self.c.writes
        self.install_fixture()
        self.assertEqual(self.c.writes, writes)
        self.assertEqual(self.c.files[config], before[0][config])
        self.assertEqual(self.uninstall(), 0)
        self.assertEqual(self.c.snapshot(), before)

    def test_interrupted_upload_is_recorded_and_can_be_undone(self):
        before = self.c.snapshot()
        original_write = self.c.write
        def fail_meta(path, raw):
            if path.endswith('/meta.json'):
                raise OSError('simulated disconnect before metadata upload')
            return original_write(path, raw)
        with patch.object(self.c, 'write', side_effect=fail_meta):
            with self.assertRaises(OSError):
                self.install_fixture()
        journal = g.Journal('console')
        self.assertTrue(any(e['op'] == 'put' and not e['done'] for e in journal.data['entries']))
        self.assertEqual(self.uninstall(), 0)
        self.assertEqual(self.c.snapshot(), before)

    def test_uninstall_preserves_later_user_edits(self):
        self.install_fixture()
        self.c.files[self.output] = b'later user edit'
        before = self.c.snapshot()
        with self.assertRaises(g.Refused):
            self.uninstall()
        self.assertEqual(self.c.snapshot(), before)

    def run(self, result=None):
        with contextlib.redirect_stdout(io.StringIO()):
            return super().run(result)


if __name__ == '__main__':
    unittest.main()
