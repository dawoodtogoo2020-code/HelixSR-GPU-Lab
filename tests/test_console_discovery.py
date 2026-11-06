# SPDX-License-Identifier: MIT
"""Discovery must survive malformed metadata and sandbox slot changes."""
import json
import struct
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import gpulab
from ps5_console import Console, Refused


def sfo(value=b'01.00\0'):
    key = b'APP_VER\0'
    return (struct.pack('<5I', 0x46535000, 0x101, 36, 36 + len(key), 1) +
            struct.pack('<HHIII', 0, 0x204, len(value), len(value), 0) + key + value)


class Metadata(unittest.TestCase):
    def test_sfo_valid_and_damaged_offsets(self):
        good = sfo()
        self.assertEqual(gpulab.sfo_value(good, 'APP_VER'), '01.00')
        damaged = []
        for offset, value in [(8, 999), (12, 999), (16, 999), (24, 999), (28, 1), (32, 999)]:
            raw = bytearray(good)
            struct.pack_into('<I', raw, offset, value)
            damaged.append(raw)
        damaged += [good[:n] for n in range(len(good))]
        damaged += [good[:36] + b'APP_VERX' + good[44:], sfo(b'01.00'), sfo(b'\xff\0')]
        for raw in damaged:
            with self.subTest(raw=bytes(raw).hex()):
                self.assertIsNone(gpulab.sfo_value(raw, 'APP_VER'))

    def test_json_missing_or_invalid_version_is_unreadable(self):
        class C:
            def stat(self, path): return {'dir': False}
            def read(self, path, cap): return self.raw
        g = {'param': {'paths': ['/meta'], 'format': 'json', 'key': 'contentVersion'}}
        c = C()
        for value in [[], None, {}, {'contentVersion': None}, {'contentVersion': 1200007}, {'contentVersion': ''}]:
            c.raw = json.dumps(value).encode()
            with self.subTest(value=value):
                self.assertEqual(gpulab.game_version(c, g), 'unreadable')
        c.raw = b'{"contentVersion": "01.200.007"}'
        self.assertEqual(gpulab.game_version(c, g), '01.200.007')

    def test_nonzero_sandbox_slot(self):
        expected = '/mnt/sandbox/PPSA02572_003/app0'
        class C:
            def listing(self, path):
                return {'eboot.bin': {'dir': False, 'bytes': 100}} if path == expected else None
        self.assertEqual(gpulab.app0_root(C(), 'PPSA02572'), expected)

    def test_directory_named_eboot_is_not_a_mount(self):
        class C:
            def listing(self, path): return {'eboot.bin': {'dir': True, 'bytes': 0}}
        self.assertIsNone(gpulab.app0_root(C(), 'PPSA02572'))


class ProcessGate(unittest.TestCase):
    def rows(self):
        return [dict(pid=0, name='kernel'), dict(pid=1, name='SceShellUI')] + [
            dict(pid=10 + i, name='SceWorker') for i in range(40)]

    def test_complete_idle_and_active(self):
        c = Console('unused')
        c.processes = self.rows
        self.assertEqual(c.running(), [])
        c.processes = lambda: self.rows() + [dict(pid=100, name='eboot.bin')]
        with self.assertRaises(Refused): c.gate()

    def test_duplicate_missing_or_invalid_pids_refuse(self):
        cases = [self.rows() + [dict(pid=1, name='SceWorker')],
                 self.rows() + [dict(name='SceWorker')],
                 self.rows() + [dict(pid=True, name='SceWorker')],
                 self.rows() + [dict(pid=-1, name='SceWorker')],
                 self.rows() + [dict(pid=100, name='')]]
        for rows in cases:
            c = Console('unused')
            c.processes = lambda: rows
            with self.subTest(rows=rows), self.assertRaises(Refused): c.gate()

    def test_malformed_endpoint_response_is_a_refusal(self):
        class Response:
            def __enter__(self): return self
            def __exit__(self, *args): pass
            def read(self, cap): return self.raw
        response = Response()
        with patch('ps5_console.urllib.request.urlopen', return_value=response):
            for raw in [b'not JSON', b'\xff', b'null', b'[]', b'{}', b'{"processes": {}}']:
                response.raw = raw
                with self.subTest(raw=raw), self.assertRaises(Refused): Console('unused').gate()
            response.raw = json.dumps({'processes': self.rows()}).encode()
            self.assertEqual(Console('unused').running(), [])


if __name__ == '__main__':
    unittest.main()
