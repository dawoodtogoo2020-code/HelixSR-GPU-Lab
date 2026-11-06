"""Execute the production x86 admission guard against the DOF crash contract.

Requires LLVM 18 and Unicorn; no owner game data or console connection.
"""
import importlib.util
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LLVM = Path('/opt/homebrew/opt/llvm@18/bin')


@unittest.skipUnless(importlib.util.find_spec('unicorn') and (LLVM / 'clang').exists(),
                     'requires Unicorn and LLVM 18')
class NativeInput(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        work = Path(cls.temp.name)
        source = ROOT / 'native/saros/replacement1440-v6/trip.c'
        subprocess.run([str(LLVM / 'clang'), '--target=x86_64-unknown-none', '-Oz',
                        '-ffreestanding', '-ffunction-sections', '-fdata-sections',
                        '-DHT_CONFIRMED_HISTORY', '-I', str(ROOT / 'native/saros/scene-triplet'),
                        '-c', str(source), '-o', str(work / 'guard.o')], check=True)
        cls.symbols = {name: 0x200000 + index * 0x1000 for index, name in enumerate([
            'hv_texture_vt', 'ht_color_name', 'ht_postprocess_color_name',
            'ht_dof_color_name', 'ht_composed_color_name', 'hv_depth_name', 'hv_velocity_name', 'ht_formats'])}
        (work / 'guard.ld').write_text('SECTIONS { . = 0x100000; .text : { *(.text*) } }\n' +
                                      '\n'.join(f'{k} = {v};' for k, v in cls.symbols.items()))
        subprocess.run([str(LLVM / 'ld.lld'), '--gc-sections', '-e', 'ht_eligible',
                        '-T', str(work / 'guard.ld'), str(work / 'guard.o'),
                        '-o', str(work / 'guard.elf')], check=True)
        subprocess.run([str(LLVM / 'llvm-objcopy'), '-O', 'binary', '--only-section=.text',
                        str(work / 'guard.elf'), str(work / 'guard.bin')], check=True)
        cls.code = (work / 'guard.bin').read_bytes()
        symbols = subprocess.check_output([str(LLVM / 'llvm-nm'), str(work / 'guard.elf')], text=True)
        cls.entry = next(int(r.split()[0], 16) for r in symbols.splitlines()
                         if r.split()[-1] == 'ht_eligible')

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def setUp(self):
        from unicorn import Uc, UC_ARCH_X86, UC_MODE_64
        self.uc = Uc(UC_ARCH_X86, UC_MODE_64)
        self.uc.mem_map(0x100000, 0x300000)
        self.uc.mem_write(0x100000, self.code)
        self.view, self.inputs = 0x300000, 0x310000
        self.textures = [0x320000, 0x321000, 0x322000]
        self.put(self.view + 0x2660, 4, 4)
        self.put(self.view + 0x2664, 1, 4)
        self.put(self.view + 0x350, 3840 | 2160 << 32)
        self.put(self.view + 0x2708, 1652 | 929 << 32)
        for offset in [0x2710, 0xde58, 0x18]:
            self.put(self.view + offset, 0x330000)
        names = ['ht_dof_color_name', 'hv_depth_name', 'hv_velocity_name']
        for offset, texture, name, fmt in zip([8, 0x20, 0x38], self.textures, names, [10, 11, 19]):
            self.put(self.inputs + offset, texture)
            self.put(self.inputs + offset + 16, 1652 | 929 << 32)
            self.put(texture, self.symbols['hv_texture_vt'])
            self.put(texture + 8, self.symbols[name])
            self.put(texture + 0x84, 2304 | 1296 << 32)
            self.put(texture + 0x8c, fmt << 56 | 0x10100010001)
        for index, value in enumerate([1, 1, 1, 8]):
            self.put(self.symbols['ht_formats'] + 10 * 48 + 12 + index * 4, value, 4)

    def put(self, address, value, size=8):
        self.uc.mem_write(address, value.to_bytes(size, 'little'))

    def admitted(self):
        from unicorn.x86_const import UC_X86_REG_RDI, UC_X86_REG_RSI, UC_X86_REG_RSP, UC_X86_REG_RAX
        self.put(0x3fff00, 0x1fff00)
        for register, value in [(UC_X86_REG_RDI, self.view), (UC_X86_REG_RSI, self.inputs),
                                (UC_X86_REG_RSP, 0x3fff00)]:
            self.uc.reg_write(register, value)
        self.uc.emu_start(self.entry, 0x1fff00, count=10000)
        return self.uc.reg_read(UC_X86_REG_RAX)

    def test_known_color_producers(self):
        for name in ['ht_color_name', 'ht_postprocess_color_name', 'ht_dof_color_name', 'ht_composed_color_name']:
            with self.subTest(name=name):
                self.put(self.textures[0] + 8, self.symbols[name])
                self.assertEqual(self.admitted(), 1)

    def test_unknown_producer_refused(self):
        self.put(self.textures[0] + 8, 0x209000)
        self.assertEqual(self.admitted(), 0)

    def test_dof_requires_original_format(self):
        self.put(self.textures[0] + 0x8c, 11 << 56 | 0x10100010001)
        self.assertEqual(self.admitted(), 0)

    def test_dof_requires_original_allocation(self):
        self.put(self.textures[0] + 0x84, 1920 | 1080 << 32)
        self.assertEqual(self.admitted(), 0)

    def test_dof_requires_matching_active_rectangle(self):
        self.put(self.inputs + 24, 1600 | 900 << 32)
        self.assertEqual(self.admitted(), 0)

    def test_dof_requires_depth_and_velocity_contract(self):
        self.put(self.textures[2] + 8, self.symbols['hv_depth_name'])
        self.assertEqual(self.admitted(), 0)


if __name__ == '__main__':
    unittest.main()
