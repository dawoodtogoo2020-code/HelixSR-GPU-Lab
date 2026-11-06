#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Package the SAROS DOF and translucency fixes over the verified v0.3.5 runtime."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

from helixsr_patch import parse_patch
from package_lite_release import read_archive, sha, write_archive

ROOT = Path(__file__).resolve().parents[1]
NAME = 'HelixSR-GPU-Lab-v0.3.6-saros-dof-beta'
BASE_NAME = 'HelixSR-GPU-Lab-v0.3.5-lite-a53-beta'
BASE_SHA = '6df2dafd481f0d20de462b37c83bf94c4cf89cd891093303947beead100f75c2'
SOURCE_SHA = '258acbba17dd6f37baf3faf1058817f2f786f91d286172c4ca9ca888233330d0'
LAUNCH_PATH = 'console/ps5/payloads/helixsr-launch.elf'
OLD_LAUNCH_SHA = '925184fee2a3f3daa53a1512f8213f35e4ac694085c583ca5d6e3cfebf9eb3c9'
LAUNCH_SHA = 'ec46352533230bdbb229f8404a7db5b537094bdb584ee480127de53944c3730f'
RECIPE_PATH = 'games/saros-native-v1/eboot.hxd'
TARGET_SHA = '68917b032b806babb10d3acc162afcd5a91b36fab5392281c38bc92e0553c44d'


def build(args):
    subprocess.run([sys.executable, '-B', str(ROOT / 'tools/check_public_tree.py')], check=True)
    baseline = read_archive(args.base_zip, BASE_SHA, BASE_NAME)
    original_source = read_archive(args.base_source_zip, SOURCE_SHA, BASE_NAME + '-source')
    names = subprocess.check_output(
        ['git', 'ls-files', '--cached', '--others', '--exclude-standard', '-z'], cwd=ROOT
    ).decode().split('\0')
    source = {}
    for name in filter(None, names):
        if any(p in {'.git', '.build', 'my-ps5', '__pycache__', 'build', 'build-repeat'}
               for p in Path(name).parts):
            raise ValueError('Unexpected private/build source path: ' + name)
        source[name] = (ROOT / name).read_bytes()
    source['baseline-native-source.zip'] = original_source['baseline-native-source.zip']
    runtime = dict(baseline)
    runtime.update({k: v for k, v in source.items() if k != 'baseline-native-source.zip'})
    runtime.pop('SHA256SUMS', None)
    launcher, recipe = args.launcher.read_bytes(), args.recipe.read_bytes()
    assert sha(baseline[LAUNCH_PATH]) == OLD_LAUNCH_SHA
    assert len(launcher) == 143208 and sha(launcher) == LAUNCH_SHA
    header, _ = parse_patch(recipe)
    assert header['target_sha256'] == TARGET_SHA and header['target_bytes'] == 191835315
    assert header['sources'] == [{
        'name': 'eboot.bin', 'bytes': 191835315,
        'sha256': '18a27d890ff5089ac8b0bc4e8d057b32a46271f5a3f1958c193e7e4aa3924cb3'}]
    assert header['holes'] == []
    runtime[LAUNCH_PATH], runtime[RECIPE_PATH] = launcher, recipe

    # Validate the exact packaged bytes against every installer manifest.
    for folder, suffix in [('console', '/console.json'), ('games', '/game.json')]:
        for name, raw in runtime.items():
            if not name.startswith(folder + '/') or not name.endswith(suffix):
                continue
            for row in json.loads(raw)['files']:
                if 'bundle' in row:
                    assert sha(runtime[row['bundle']]) == row['sha256'], row['bundle']
                if 'text' in row:
                    assert sha(row['text'].encode()) == row['sha256'], name
                for patch in row.get('patches', []):
                    patch_header, _ = parse_patch(runtime[patch])
                    assert patch_header['target_sha256'] == row['sha256'], patch
    saros = json.loads(runtime['games/saros-native-v1/game.json'])
    assert saros['files'][0]['sha256'] == TARGET_SHA
    assert TARGET_SHA.encode() in saros['files'][2]['text'].encode()
    # Only the regular-PS5 launcher and SAROS owner-file recipe may change
    # among baseline native binaries and recipes. Model and Pro stay pinned.
    unchanged = {}
    suffixes = ('.elf', '.bin', '.prx', '.hxp', '.pkg', '.hxd')
    for name, raw in baseline.items():
        if name.endswith(suffixes) and name not in (LAUNCH_PATH, RECIPE_PATH):
            assert runtime[name] == raw, name
            unchanged[name] = sha(raw)
    assert not any('test5' in name.lower() for name in runtime)
    assert not any(name.endswith(('.self', '.dll')) for name in runtime)
    assert RECIPE_PATH not in source and LAUNCH_PATH not in source

    args.output.mkdir(parents=True, exist_ok=True)
    artifacts = {}
    for suffix, files in [('', runtime), ('-source', source)]:
        path = args.output / (NAME + suffix + '.zip')
        write_archive(path, NAME + suffix, files)
        artifacts[path.name] = {'bytes': path.stat().st_size, 'sha256': sha(path.read_bytes())}
    (args.output / 'SHA256SUMS').write_text(''.join(
        f"{info['sha256']}  {name}\n" for name, info in artifacts.items()))
    receipt = {
        'release': NAME, 'base_runtime_sha256': BASE_SHA, 'base_source_sha256': SOURCE_SHA,
        'artifacts': artifacts, 'changed_native_binary': {LAUNCH_PATH: LAUNCH_SHA},
        'changed_owner_recipe': {RECIPE_PATH: sha(recipe)},
        'unchanged_binary_and_recipe_hashes': unchanged,
        'source_files': len(source), 'runtime_files': len(runtime),
        'scope': 'SAROS PPSA07631 01.000.000 regular PS5 firmware 11.60; bounded DOF gameplay, relaunch and laser/translucency validation',
    }
    (args.output / 'release-verification.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base-zip', type=Path, required=True)
    parser.add_argument('--base-source-zip', type=Path, required=True)
    parser.add_argument('--launcher', type=Path, required=True)
    parser.add_argument('--recipe', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    build(parser.parse_args())
