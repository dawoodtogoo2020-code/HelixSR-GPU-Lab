# SPDX-License-Identifier: MIT
"""Deterministically link the exact A53 reference into the exact Lite 1.11 ELF.

One ELF, one process, one Lite kernel. No child payload or manager request.
The existing A53 runtime remains in its own position-independent namespace.
Both original dynamic tables remain private to their respective SDK runtimes.
"""
import hashlib
import struct

from . import build_bridge

LITE_SHA256 = 'ab9a6cb4d3b1daf139d4d646e402b1cf569071acd64599c936d7a3a6164dc779'
LITE_BYTES = 1737080
MODULE_VA = 0x200000
WRAPPER = MODULE_VA + 0x2a00
LITE_MAIN = 0x3cd0
LITE_GET_ARGS = 0xf22c0
LITE_MAIN_CALL = 0xf25a0
A53_MAIN_CLEANUP = 0xfae28
A53_TERMINATE = 0xfadda
A53_EPILOGUE = 0xfadf8
A53_PAYLOAD_ARGS = 0x1b3f70
PAGE = 0x4000


def sha(data):
    return hashlib.sha256(data).hexdigest()


def align(value, amount=PAGE):
    return (value + amount - 1) & -amount


def headers(data):
    if data[:7] != b'\x7fELF\x02\x01\x01':
        raise ValueError('Expected ELF64 little endian input')
    phoff, shoff = struct.unpack_from('<QQ', data, 32)
    phentsize, phnum, shentsize, shnum, shstr = struct.unpack_from('<5H', data, 54)
    if phentsize != 56 or shentsize != 64:
        raise ValueError('Unexpected ELF header sizes')
    ph = [list(struct.unpack_from('<II6Q', data, phoff + i * 56)) for i in range(phnum)]
    sh = [list(struct.unpack_from('<II4QII2Q', data, shoff + i * 64)) for i in range(shnum)]
    return ph, sh, shstr


def file_offset(ph, va, length=1):
    matches = [p[2] + va - p[3] for p in ph
               if p[0] == 1 and p[3] <= va and va + length <= p[3] + p[5]]
    if len(matches) != 1:
        raise ValueError('Address is not in exactly one file-backed load segment')
    return matches[0]


def rel32(opcode, here, target):
    return opcode + struct.pack('<i', target - here - len(opcode) - 4)


def wrapper():
    # Preserve main(argc, argv, envp), all callee-saved registers we use and
    # the SysV 16-byte call alignment. Fail closed before the Lite main.
    code = bytearray.fromhex('55 48 89 e5 41 54 41 55 41 56 48 83 ec 08')
    code += bytes.fromhex('41 89 fc 49 89 f5 49 89 d6')
    def call(target):
        code.extend(rel32(b'\xe8', WRAPPER + len(code), target))
    call(LITE_GET_ARGS)
    code += bytes.fromhex('48 89 c7')
    call(MODULE_VA + 0xfaae8)
    code += bytes.fromhex('85 c0 75 0e')
    code += bytes.fromhex('44 89 e7 4c 89 ee 4c 89 f2')
    call(LITE_MAIN)
    code += bytes.fromhex('48 83 c4 08 41 5e 41 5d 41 5c 5d c3')
    return bytes(code)


def a53_return_stub():
    # The embedded runtime is a called module, not a process entry that may
    # call libc exit(). Return its payloadout, including SDK initialization
    # failures, to the outer Lite runtime. The outer CRT owns process exit.
    code = bytearray(b'\x48\x8b\x05' + struct.pack('<i', A53_PAYLOAD_ARGS - A53_TERMINATE - 7))
    code += bytes.fromhex('48 8b 40 28 48 85 c0 74 04 8b 00 eb 05 b8 01 00 00 00')
    code += rel32(b'\xe9', A53_TERMINATE + len(code), A53_EPILOGUE)
    if A53_TERMINATE + len(code) > A53_EPILOGUE:
        raise ValueError('A53 return stub exceeds its reserved region')
    return bytes(code)


def build(reference, lite):
    if len(lite) != LITE_BYTES or sha(lite) != LITE_SHA256:
        raise ValueError('K-Stuff Lite must be the exact verified v1.11 file')
    module = bytearray(build_bridge.build(reference))
    lph, lsh, shstr = headers(lite)
    aph, ash, _ = headers(module)
    if len(lph) != 4 or len(aph) != 4:
        raise ValueError('Unexpected load layout')
    call_at = file_offset(lph, LITE_MAIN_CALL, 6)
    if lite[call_at:call_at + 6] != b'\x67' + rel32(b'\xe8', LITE_MAIN_CALL + 1, LITE_MAIN):
        raise ValueError('Unexpected Lite CRT main call')
    if module[file_offset(aph, A53_MAIN_CLEANUP, 6):][:6] != bytes.fromhex('4c 89 ef 67 e8 9f'):
        raise ValueError('Unexpected A53 runtime cleanup path')
    # Keep the A53 runtime and its dependencies live until the Lite process
    # ends. Do not finalize shared dependencies from a nested runtime.
    at = file_offset(aph, A53_MAIN_CLEANUP, 6)
    module[at:at + 6] = rel32(b'\xe9', A53_MAIN_CLEANUP, A53_TERMINATE) + b'\x90'
    at = file_offset(aph, A53_TERMINATE, A53_EPILOGUE - A53_TERMINATE)
    stub = a53_return_stub()
    module[at:at + len(stub)] = stub
    at = file_offset(aph, WRAPPER - MODULE_VA, len(wrapper()))
    module[at:at + len(wrapper())] = wrapper()

    result = bytearray(lite)
    result[call_at:call_at + 6] = rel32(b'\xe8', LITE_MAIN_CALL, WRAPPER) + b'\x90'
    blob_offset = align(len(result))
    result += bytes(blob_offset - len(result)) + module
    newph = list(lph)
    for p in aph:
        if p[0] == 1:
            p = list(p)
            p[2] += blob_offset
            p[3] += MODULE_VA
            p[4] += MODULE_VA
            newph.append(p)
    if 64 + len(newph) * 56 > PAGE:
        raise ValueError('Program headers exceed the reserved header page')
    # elfldr applies only R_X86_64_RELATIVE entries from SHT_RELA sections.
    # Duplicate the module's loader relocations with the namespace shift.
    # Its original dynamic/relocation tables remain untouched for its CRT.
    relative = bytearray()
    for section in ash:
        if section[1] != 4:
            continue
        for at in range(section[4], section[4] + section[5], 24):
            offset, info, addend = struct.unpack_from('<QQq', module, at)
            if info & 0xffffffff == 8:
                relative += struct.pack('<QQq', offset + MODULE_VA, info, addend + MODULE_VA)
    rela_offset = align(len(result), 8)
    result += bytes(rela_offset - len(result)) + relative
    oldstrings = lite[lsh[shstr][4]:lsh[shstr][4] + lsh[shstr][5]]
    strings = oldstrings + b'.a53.loader.rela\0'
    strings_offset = len(result)
    result += strings
    lsh[shstr][4:6] = [strings_offset, len(strings)]
    lsh.append([len(oldstrings), 4, 0, 0, rela_offset, len(relative), 0, 0, 8, 24])
    section_offset = align(len(result), 8)
    result += bytes(section_offset - len(result))
    for p in lsh:
        result += struct.pack('<II4QII2Q', *p)
    for i, p in enumerate(newph):
        struct.pack_into('<II6Q', result, 64 + i * 56, *p)
    struct.pack_into('<Q', result, 40, section_offset)
    struct.pack_into('<H', result, 56, len(newph))
    struct.pack_into('<H', result, 60, len(lsh))
    return bytes(result), {
        'format': 1, 'firmware': '11.60 only',
        'lite_sha256': LITE_SHA256, 'a53_reference_sha256': build_bridge.REFERENCE_SHA256,
        'output_sha256': sha(result), 'output_bytes': len(result),
        'a53_file_offset': blob_offset, 'a53_virtual_offset': MODULE_VA,
        'a53_loader_relative_relocations': len(relative) // 24,
        'lite_original_main_preserved': True,
        'embedded_test5_kernel_erased': True,
        'kernel_payloads': 1, 'processes': 1, 'separate_bridge_required': False,
        'hardware_qualification': '2026-11-06: regular PS5 FW11.60, SAROS PPSA07631 01.000.000 gameplay, relaunch and two normal closes',
    }
