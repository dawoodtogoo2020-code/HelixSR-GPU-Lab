#!/usr/bin/env python3
"""HXD1 patches: build the tested HelixSR files from your own game files, without shipping any game file.

    python helixsr_patch.py apply PATCH.hxd OUT [--source NAME=FILE ...] [--fill NAME=FILE ...]
    python helixsr_patch.py info PATCH.hxd
    python helixsr_patch.py make TARGET PATCH.hxd [--source NAME=FILE ...] [--hole NAME=OFFSET:FILE ...]  (needs numpy)

A patch holds only what HelixSR changed: copy instructions that point into YOUR copies of the game's files (its
sources, in order), plus the new bytes (HelixSR kernels, plans, our own code). Holes are ranges that ship as zeros and
are filled on your PC from a file you make yourself (the NVIDIA-derived weights from helixsr_weights.py). `apply`
checks every source, every hole and the result against SHA-256 values in the patch and writes nothing on a mismatch.
`apply` and `info` need only the Python standard library.
"""
import argparse
import hashlib
import json
import lzma
import os
import struct
import sys
from pathlib import Path

MAGIC = b'HXD1'
COPY, ADD, END = 1, 2, 0
BLOCK = 64


def sha(data):
    return hashlib.sha256(data).hexdigest()


# ---------------------------------------------------------------- reading and applying

def parse_patch(raw, name='patch'):
    if raw[:4] != MAGIC:
        raise SystemExit(f'{name}: not an HXD1 patch')
    try:
        (hlen,) = struct.unpack_from('<I', raw, 4)
        header = json.loads(raw[8:8 + hlen].decode('utf-8'))
        ops = lzma.decompress(raw[8 + hlen:])
    except (struct.error, ValueError, lzma.LZMAError):
        raise SystemExit(f'{name}: patch is damaged; download GPU Lab again')
    if sha(ops) != header['ops_sha256']:
        raise SystemExit(f'{name}: patch is damaged (ops checksum)')
    return header, ops


def read_patch(path):
    return parse_patch(Path(path).read_bytes(), str(path))


def check_sources(header, sources):
    """sources: name -> bytes. Returns the concatenation the copy instructions index, in the patch's order."""
    parts = []
    for want in header['sources']:
        data = sources.get(want['name'])
        if data is None:
            raise SystemExit(f"this patch needs your copy of {want['name']} (--source {want['name']}=FILE)")
        if len(data) != want['bytes'] or sha(data) != want['sha256']:
            raise SystemExit(f"{want['name']}: not the supported game version (size {len(data)}, sha256 "
                             f"{sha(data)[:16]}...; expected {want['bytes']}, {want['sha256'][:16]}...)")
        parts.append(data)
    return b''.join(parts)


def apply(header, ops, sources, fills):
    src = memoryview(check_sources(header, sources))
    out = bytearray()
    view = memoryview(ops)
    p = 0
    while True:
        kind = view[p]
        p += 1
        if kind == END:
            break
        if kind == COPY:
            s, n = struct.unpack_from('<QQ', view, p)
            p += 16
            out += src[s:s + n]
        elif kind == ADD:
            (n,) = struct.unpack_from('<Q', view, p)
            p += 8
            out += view[p:p + n]
            p += n
        else:
            raise SystemExit('patch is damaged (unknown op)')
    if len(out) != header['target_bytes']:
        raise SystemExit('patch produced the wrong size')
    for hole in header['holes']:
        data = fills.get(hole['name'])
        if data is None:
            raise SystemExit(f"this patch needs --fill {hole['name']}=FILE ({hole['about']})")
        if len(data) != hole['bytes'] or sha(data) != hole['sha256']:
            raise SystemExit(f"{hole['name']}: wrong file (expected sha256 {hole['sha256'][:16]}...)")
        out[hole['offset']:hole['offset'] + hole['bytes']] = data
    if sha(out) != header['target_sha256']:
        raise SystemExit('result does not match the tested file; nothing written')
    return bytes(out)


def write_new(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_name(path.name + '.part')
    tmp.write_bytes(data)
    os.replace(tmp, path)


# ---------------------------------------------------------------- making (maintainers)

def make(sources, target, holes):
    """sources: [(name, bytes)] in order; holes: name -> (offset, bytes, about)."""
    import numpy as np
    np.seterr(over='ignore')
    source = b''.join(d for _, d in sources)
    tgt = bytearray(target)
    hole_rows = []
    for name, (offset, data, about) in sorted(holes.items(), key=lambda kv: kv[1][0]):
        if bytes(tgt[offset:offset + len(data)]) != data:
            raise SystemExit(f'hole {name}: target does not hold that file at offset {offset}')
        tgt[offset:offset + len(data)] = bytes(len(data))
        hole_rows.append(dict(name=name, offset=offset, bytes=len(data), sha256=sha(data), about=about))
    S = np.frombuffer(source, np.uint8)
    T = np.frombuffer(bytes(tgt), np.uint8)
    consts = np.array([0x9E3779B97F4A7C15, 0xC2B2AE3D27D4EB4F, 0x165667B19E3779F9, 0xD6E8FEB86659FD93,
                       0xFF51AFD7ED558CCD, 0xC4CEB9FE1A85EC53, 0x94D049BB133111EB, 0xBF58476D1CE4E5B9], np.uint64)

    def words(buf, start, count):
        """uint64 little-endian word at every byte position start..start+count-1."""
        seg = buf[start:start + count + 7].astype(np.uint64)
        w = np.zeros(count, np.uint64)
        for k in range(8):
            w |= seg[k:k + count] << np.uint64(8 * k)
        return w

    # Index the sources' aligned 64-byte blocks, then look up the window at every target byte.
    nb = len(S) // BLOCK
    cand_pos = cand_src = np.zeros(0, np.int64)
    if nb and len(T) >= BLOCK:
        sw = np.frombuffer(source[:nb * BLOCK], np.uint64).reshape(nb, 8)
        hs = (sw * consts).sum(axis=1, dtype=np.uint64)
        order = np.argsort(hs, kind='stable')
        hs_sorted = hs[order]
        pos, src = [], []
        chunk, last = 1 << 22, len(T) - BLOCK
        for a in range(0, last + 1, chunk):
            count = min(chunk, last + 1 - a)
            w = words(T, a, count + 56)
            h = np.zeros(count, np.uint64)
            for j in range(8):
                h += w[8 * j:8 * j + count] * consts[j]
            idx = np.searchsorted(hs_sorted, h)
            idx[idx >= nb] = nb - 1
            hit = hs_sorted[idx] == h
            pos.append(np.flatnonzero(hit) + a)
            src.append(order[idx[hit]].astype(np.int64) * BLOCK)
        cand_pos, cand_src = np.concatenate(pos), np.concatenate(src)

    def forward(q, s):
        n, step, limit = 0, 4096, min(len(T) - q, len(S) - s)
        while n < limit:
            m = min(step, limit - n)
            ne = np.flatnonzero(T[q + n:q + n + m] != S[s + n:s + n + m])
            if ne.size:
                return n + int(ne[0])
            n += m
            step = min(step * 2, 1 << 24)
        return n

    def backward(q, s, floor):
        limit = min(q - floor, s)
        n, step = 0, 64
        while n < limit:
            m = min(step, limit - n)
            ne = np.flatnonzero(T[q - n - m:q - n][::-1] != S[s - n - m:s - n][::-1])
            if ne.size:
                return n + int(ne[0])
            n += m
            step = min(step * 2, 1 << 20)
        return n

    out = bytearray()
    stats = dict(copy_ops=0, copy_bytes=0, add_ops=0, add_bytes=0)

    def emit_add(a, b):
        if b > a:
            out.append(ADD)
            out.extend(struct.pack('<Q', b - a))
            out.extend(T[a:b].tobytes())
            stats['add_ops'] += 1
            stats['add_bytes'] += b - a

    def emit_copy(s, n):
        out.append(COPY)
        out.extend(struct.pack('<QQ', s, n))
        stats['copy_ops'] += 1
        stats['copy_bytes'] += n

    p = lit = 0
    prev_delta = None          # source - target offset of the last copy, tried first (keeps runs of zeros in place)
    while True:
        ci = int(np.searchsorted(cand_pos, p, side='left'))
        if ci >= len(cand_pos):
            break
        q, s = int(cand_pos[ci]), int(cand_src[ci])
        if prev_delta is not None and 0 <= q + prev_delta <= len(S) - BLOCK and \
                np.array_equal(T[q:q + BLOCK], S[q + prev_delta:q + prev_delta + BLOCK]):
            s = q + prev_delta
        elif not np.array_equal(T[q:q + BLOCK], S[s:s + BLOCK]):
            p = q + 1
            continue
        back = backward(q, s, lit)
        n = forward(q, s)
        emit_add(lit, q - back)
        emit_copy(s - back, back + n)
        prev_delta = s - q
        p = lit = q + n
    emit_add(lit, len(T))
    out.append(END)
    ops = bytes(out)
    header = dict(format='HXD1', sources=[dict(name=n, bytes=len(d), sha256=sha(d)) for n, d in sources],
                  target_bytes=len(target), target_sha256=sha(target), holes=hole_rows, ops_sha256=sha(ops),
                  stats=stats)
    return header, ops


def pack(header, ops):
    hb = json.dumps(header, indent=1).encode('utf-8')
    return MAGIC + struct.pack('<I', len(hb)) + hb + lzma.compress(ops, preset=9 | lzma.PRESET_EXTREME)


# ---------------------------------------------------------------- command line

def pairs(items, flag):
    out = []
    for item in items or []:
        if '=' not in item:
            raise SystemExit(f'{flag}: use NAME=...')
        out.append(tuple(item.split('=', 1)))
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description='HXD1 patch tool (HelixSR GPU Lab)')
    sub = ap.add_subparsers(dest='cmd', required=True)
    a = sub.add_parser('apply', help='build a file from your own game files')
    a.add_argument('patch')
    a.add_argument('out')
    a.add_argument('--source', action='append', help='NAME=FILE: your copy of a game file the patch names')
    a.add_argument('--fill', action='append', help='NAME=FILE for a hole (e.g. weights=weights.ngx.bin)')
    i = sub.add_parser('info', help='show what a patch needs and makes')
    i.add_argument('patch')
    m = sub.add_parser('make', help='maintainers: make a patch (needs numpy)')
    m.add_argument('target')
    m.add_argument('patch')
    m.add_argument('--source', action='append', help='NAME=FILE, in order')
    m.add_argument('--hole', action='append', help='NAME=OFFSET:FILE (target range shipped as zeros)')
    m.add_argument('--about', default='weights.ngx.bin from helixsr_weights.py', help='what a hole file is')
    args = ap.parse_args(argv)

    if args.cmd == 'apply':
        header, ops = read_patch(args.patch)
        sources = {k: Path(v).read_bytes() for k, v in pairs(args.source, '--source')}
        fills = {k: Path(v).read_bytes() for k, v in pairs(args.fill, '--fill')}
        data = apply(header, ops, sources, fills)
        write_new(args.out, data)
        print(f'wrote {args.out} ({len(data)} bytes, sha256 {sha(data)[:16]}..., matches the tested file)')
    elif args.cmd == 'info':
        header, _ = read_patch(args.patch)
        print(json.dumps(header, indent=2))
    else:
        sources = [(k, Path(v).read_bytes()) for k, v in pairs(args.source, '--source')]
        holes = {}
        for name, spec in pairs(args.hole, '--hole'):
            offset, file = spec.split(':', 1)
            holes[name] = (int(offset, 0), Path(file).read_bytes(), args.about)
        target = Path(args.target).read_bytes()
        header, ops = make(sources, target, holes)
        blob = pack(header, ops)
        check = apply(*parse_patch(blob), dict(sources), {k: v[1] for k, v in holes.items()})
        assert check == target
        write_new(args.patch, blob)
        print(json.dumps(dict(patch=str(args.patch), patch_bytes=len(blob), **header['stats']), indent=1))


if __name__ == '__main__':
    sys.exit(main())
