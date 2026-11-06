#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check the native composition branch using the owner's exact decoded ELF.

Requires Unicorn. Executes original title instructions with synthetic resources;
does not connect to a console or claim visual/GPU qualification.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import (
    UC_X86_REG_RAX, UC_X86_REG_RBP, UC_X86_REG_RCX, UC_X86_REG_RDI,
    UC_X86_REG_RIP, UC_X86_REG_RSP, UC_X86_REG_R13, UC_X86_REG_R14,
)


def validate(elf, candidate):
    raw = elf.read_bytes()
    assert hashlib.sha256(raw).hexdigest() == '26585333efaf6a14b914da8d33d3b2fc5471155ffb7dc724d84371e028979f46'
    phoff = struct.unpack_from('<Q', raw, 32)[0]
    size, count = struct.unpack_from('<HH', raw, 54)
    segments = [struct.unpack_from('<IIQQQQQQ', raw, phoff + i * size) for i in range(count)]

    def read(address, length):
        for kind, flags, offset, va, pa, files, memory, alignment in segments:
            if kind == 1 and va <= address and address + length <= va + files:
                return raw[offset + address - va:offset + address - va + length]
        raise ValueError('Address is outside file-backed owner ELF')

    assert read(0xad445b, 7).hex() == '8a8c2410010000'
    assert read(0xad45c4, 4).hex() == '8a4c2440'
    with candidate.open('rb') as handle:
        handle.seek(0xb0b5db)
        patched = handle.read(7)
        handle.seek(0xb0b744)
        format_patch = handle.read(4)
    assert patched.hex() == 'b1009090909090'
    assert format_patch.hex() == 'b10a9090'
    assert read(0x713b316, 64).decode('utf-16-le').split('\0')[0] == 'PostDOFTranslucency.SceneColor'
    flag_cases = []
    for flag in (0, 1):
        uc = Uc(UC_ARCH_X86, UC_MODE_64)
        uc.mem_map(0xad4000, 4096)
        uc.mem_map(0x400000, 0x10000)
        uc.mem_write(0xad445b, patched)
        uc.reg_write(UC_X86_REG_RSP, 0x401000)
        uc.mem_write(0x401110, bytes([flag]))
        original_rcx = 0x123456789abcde73
        uc.reg_write(UC_X86_REG_RCX, original_rcx)
        uc.emu_start(0xad445b, 0xad4462, count=16)
        assert uc.reg_read(UC_X86_REG_RCX) == original_rcx & ~255
        flag_cases.append({'incoming_defer_flag': flag, 'patched_defer_flag': 0})
        uc.mem_write(0xad45c4, format_patch)
        uc.reg_write(UC_X86_REG_RCX, original_rcx)
        uc.emu_start(0xad45c4, 0xad45c8, count=16)
        assert uc.reg_read(UC_X86_REG_RCX) == (original_rcx & ~255) | 10

    paths = []
    # Original compositor checks the defer flag before choosing scene colour,
    # the separate colour, or a neutral colour for the modulate-only prepass.
    for defer in (0, 1):
        for modulation in (False, True):
            uc = Uc(UC_ARCH_X86, UC_MODE_64)
            for address, length in [(0xadf000, 4096), (0x1a3000, 4096),
                                    (0x300000, 0x10000), (0x400000, 0x10000)]:
                uc.mem_map(address, length)
            uc.mem_write(0xadf832, read(0xadf832, 0x83))
            resources, config, colour, neutral = 0x301000, 0x302000, 0x303000, 0x304000
            uc.mem_write(resources, struct.pack('<I', 1))
            uc.mem_write(resources + 0x28, struct.pack('<QQ', 0x305000, 0x305100) if modulation else bytes(16))
            uc.mem_write(config + 4, bytes([defer]))
            for register, value in [(UC_X86_REG_R13, resources), (UC_X86_REG_R14, config),
                                    (UC_X86_REG_RDI, colour), (UC_X86_REG_RAX, 0x306000),
                                    (UC_X86_REG_RBP, 0x408000), (UC_X86_REG_RSP, 0x407000)]:
                uc.reg_write(register, value)
            outcome = {}

            def instruction(machine, address, length, data):
                if address == 0xadf850:
                    outcome['path'] = 'bypass_separate_colour'
                    machine.emu_stop()
                elif address == 0xadf8b5:
                    outcome['path'] = 'compose_separate_colour' if machine.reg_read(UC_X86_REG_RDI) == colour else 'modulate_only'
                    machine.emu_stop()
                elif address == 0x1a3734:
                    # Native white-dummy lookup; only its returned handle is
                    # relevant to this CPU branch test, no GPU is emulated.
                    stack = machine.reg_read(UC_X86_REG_RSP)
                    target = struct.unpack('<Q', machine.mem_read(stack, 8))[0]
                    machine.reg_write(UC_X86_REG_RAX, neutral)
                    machine.reg_write(UC_X86_REG_RSP, stack + 8)
                    machine.reg_write(UC_X86_REG_RIP, target)

            uc.hook_add(UC_HOOK_CODE, instruction)
            uc.emu_start(0xadf832, 0xadf8b6, count=200)
            expected = 'compose_separate_colour' if not defer else 'modulate_only' if modulation else 'bypass_separate_colour'
            assert outcome.get('path') == expected, outcome
            paths.append({'defer': defer, 'modulation': modulation, **outcome})
    return {'status': 'passed', 'patched_flag_cases': flag_cases, 'native_compositor_paths': paths,
            'scope': 'Exact owner CPU instructions and synthetic resources; GPU rendering/visual qualification remains separate'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stock-elf', type=Path, required=True)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    result = validate(args.stock_elf, args.candidate)
    text = json.dumps(result, indent=2) + '\n'
    if args.output:
        args.output.write_text(text)
    print(text, end='')
