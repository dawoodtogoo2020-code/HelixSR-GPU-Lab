#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Package the Lite setup update, reusing verified unchanged beta binaries."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
NAME = 'HelixSR-GPU-Lab-v0.3.2-lite-a53-beta'
BASE_NAME = 'HelixSR-GPU-Lab-v0.3.1-saros-effects-beta'
BASE_SHA = 'c4544ab7cf1f53327f655caed092b2ec9fae722d376b4370fd4e8f57c5d74941'
SOURCE_SHA = '1f5545a91b9a2629b8265fb9c8c7e5e7538e5203a981f4f4d96bb1441f536470'


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def read_archive(path, expected, prefix):
    if sha(path.read_bytes()) != expected:
        raise ValueError('Baseline archive hash mismatch: ' + path.name)
    result = {}
    with zipfile.ZipFile(path) as archive:
        if archive.testzip() is not None:
            raise ValueError('Corrupt baseline archive')
        for entry in archive.infolist():
            parts = PurePosixPath(entry.filename).parts
            if not parts or parts[0] != prefix or '..' in parts or '\\' in entry.filename:
                raise ValueError('Unexpected archive path: ' + entry.filename)
            if entry.is_dir():
                continue
            name = '/'.join(parts[1:])
            if name in result:
                raise ValueError('Duplicate archive path: ' + name)
            result[name] = archive.read(entry)
    return result


def write_archive(path, prefix, files):
    if path.exists():
        raise ValueError('Refusing to overwrite existing output: ' + path.name)
    with zipfile.ZipFile(path, 'w') as archive:
        for name, raw in sorted(files.items()):
            info = zipfile.ZipInfo(prefix + '/' + name, (2026, 11, 6, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, raw, compresslevel=9)
    with zipfile.ZipFile(path) as archive:
        assert archive.testzip() is None
        assert len(archive.namelist()) == len(files)
        for name, raw in files.items():
            assert archive.read(prefix + '/' + name) == raw


def build(base_zip, base_source_zip, output):
    subprocess.run([__import__('sys').executable, '-B', str(ROOT / 'tools/check_public_tree.py')], check=True)
    baseline = read_archive(base_zip, BASE_SHA, BASE_NAME)
    original_source = read_archive(base_source_zip, SOURCE_SHA, BASE_NAME + '-source')
    names = subprocess.check_output(['git', 'ls-files', '--cached', '--others', '--exclude-standard', '-z'], cwd=ROOT).decode().split('\0')
    source = {}
    for name in filter(None, names):
        if any(part in {'.git', '.build', 'my-ps5', '__pycache__', 'build', 'build-repeat'} for part in Path(name).parts):
            raise ValueError('Unexpected private/build source path: ' + name)
        source[name] = (ROOT / name).read_bytes()
    # Corresponding source for unchanged legacy native binaries remains
    # available without retaining the old host build intermediates.
    source['baseline-native-source.zip'] = original_source['baseline-native-source.zip']
    runtime = dict(baseline)
    runtime.update({k: v for k, v in source.items() if k != 'baseline-native-source.zip'})
    runtime.pop('SHA256SUMS', None)
    for folder in ('console', 'games'):
        suffix = '/console.json' if folder == 'console' else '/game.json'
        for name, raw in runtime.items():
            if not name.startswith(folder + '/') or not name.endswith(suffix):
                continue
            for row in json.loads(raw)['files']:
                if 'bundle' in row:
                    assert sha(runtime[row['bundle']]) == row['sha256'], row['bundle']
    binary_suffixes = ('.elf', '.bin', '.prx', '.hxp', '.pkg')
    binaries = {name: sha(raw) for name, raw in baseline.items() if name.endswith(binary_suffixes)}
    for name, digest in binaries.items():
        assert sha(runtime[name]) == digest, name
    assert not any('test5' in name.lower() or name.lower().endswith('a53-11.60.elf') for name in runtime)
    assert 'setup-kstuff-lite.bat' in runtime and 'setup-kstuff-lite.sh' in runtime
    output.mkdir(parents=True, exist_ok=True)
    artifacts = {}
    for suffix, files in [('', runtime), ('-source', source)]:
        path = output / (NAME + suffix + '.zip')
        write_archive(path, NAME + suffix, files)
        artifacts[path.name] = {'bytes': path.stat().st_size, 'sha256': sha(path.read_bytes())}
    (output / 'SHA256SUMS').write_text(''.join(f"{v['sha256']}  {k}\n" for k, v in artifacts.items()))
    receipt = {'release': NAME, 'base_runtime_sha256': BASE_SHA, 'base_source_sha256': SOURCE_SHA,
               'artifacts': artifacts, 'unchanged_binary_hashes': binaries,
               'source_files': len(source), 'runtime_files': len(runtime),
               'derived_a53_or_reference_payloads_bundled': False}
    (output / 'release-verification.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base-zip', type=Path, required=True)
    parser.add_argument('--base-source-zip', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    build(args.base_zip, args.base_source_zip, args.output)
